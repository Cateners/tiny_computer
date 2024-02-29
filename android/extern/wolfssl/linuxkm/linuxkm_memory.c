/* linuxkm_memory.c
 *
 * Copyright (C) 2006-2022 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/* included by wolfcrypt/src/memory.c */

#if defined(WOLFSSL_LINUXKM_SIMD_X86)
    #ifdef LINUXKM_SIMD_IRQ
        #if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
            static union fpregs_state **wolfcrypt_linuxkm_fpu_states = NULL;
        #else
            static struct fpstate **wolfcrypt_linuxkm_fpu_states = NULL;
        #endif
    #else
        static unsigned int *wolfcrypt_linuxkm_fpu_states = NULL;
    #endif

    static WARN_UNUSED_RESULT inline int am_in_hard_interrupt_handler(void)
    {
        return (preempt_count() & (NMI_MASK | HARDIRQ_MASK)) != 0;
    }

    WARN_UNUSED_RESULT int allocate_wolfcrypt_linuxkm_fpu_states(void)
    {
        #ifdef LINUXKM_SIMD_IRQ
            #if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
            wolfcrypt_linuxkm_fpu_states =
                (union fpregs_state **)kzalloc(nr_cpu_ids
                                               * sizeof(struct fpu_state *),
                                               GFP_KERNEL);
            #else
            wolfcrypt_linuxkm_fpu_states =
                (struct fpstate **)kzalloc(nr_cpu_ids
                                           * sizeof(struct fpstate *),
                                           GFP_KERNEL);
            #endif
        #else
            wolfcrypt_linuxkm_fpu_states =
                (unsigned int *)kzalloc(nr_cpu_ids * sizeof(unsigned int),
                                        GFP_KERNEL);
        #endif

        if (! wolfcrypt_linuxkm_fpu_states) {
            pr_err("warning, allocation of %lu bytes for "
                   "wolfcrypt_linuxkm_fpu_states failed.\n",
                   nr_cpu_ids * sizeof(struct fpu_state *));
            return MEMORY_E;
        }
#ifdef LINUXKM_SIMD_IRQ
        {
            typeof(nr_cpu_ids) i;
            for (i=0; i<nr_cpu_ids; ++i) {
                _Static_assert(sizeof(union fpregs_state) <= PAGE_SIZE,
                               "union fpregs_state is larger than expected.");
                #if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
                wolfcrypt_linuxkm_fpu_states[i] =
                    (union fpregs_state *)kzalloc(PAGE_SIZE
                                                  /* sizeof(union fpregs_state) */,
                                                  GFP_KERNEL);
                #else
                wolfcrypt_linuxkm_fpu_states[i] =
                    (struct fpstate *)kzalloc(PAGE_SIZE
                                              /* sizeof(struct fpstate) */,
                                              GFP_KERNEL);
                #endif
                if (! wolfcrypt_linuxkm_fpu_states[i])
                    break;
                /* double-check that the allocation is 64-byte-aligned as needed
                 * for xsave.
                 */
                if ((unsigned long)wolfcrypt_linuxkm_fpu_states[i] & 63UL) {
                    pr_err("warning, allocation for wolfcrypt_linuxkm_fpu_states "
                           "was not properly aligned (%px).\n",
                           wolfcrypt_linuxkm_fpu_states[i]);
                    kfree(wolfcrypt_linuxkm_fpu_states[i]);
                    wolfcrypt_linuxkm_fpu_states[i] = 0;
                    break;
                }
            }
            if (i < nr_cpu_ids) {
                pr_err("warning, only %u/%u allocations succeeded for "
                       "wolfcrypt_linuxkm_fpu_states.\n",
                       i, nr_cpu_ids);
                return MEMORY_E;
            }
        }
#endif /* LINUXKM_SIMD_IRQ */
        return 0;
    }

    void free_wolfcrypt_linuxkm_fpu_states(void)
    {
        if (wolfcrypt_linuxkm_fpu_states) {
#ifdef LINUXKM_SIMD_IRQ
            typeof(nr_cpu_ids) i;
            for (i=0; i<nr_cpu_ids; ++i) {
                if (wolfcrypt_linuxkm_fpu_states[i])
                    kfree(wolfcrypt_linuxkm_fpu_states[i]);
            }
#endif /* LINUXKM_SIMD_IRQ */
            kfree(wolfcrypt_linuxkm_fpu_states);
            wolfcrypt_linuxkm_fpu_states = 0;
        }
    }

    WARN_UNUSED_RESULT int save_vector_registers_x86(void)
    {
        int processor_id;

        preempt_disable();

        processor_id = smp_processor_id();

        {
            static int _warned_on_null = -1;
            if ((wolfcrypt_linuxkm_fpu_states == NULL)
#ifdef LINUXKM_SIMD_IRQ
                || (wolfcrypt_linuxkm_fpu_states[processor_id] == NULL)
#endif
                )
            {
                preempt_enable();
                if (_warned_on_null < processor_id) {
                    _warned_on_null = processor_id;
                    pr_err("save_vector_registers_x86 called for cpu id %d "
                           "with null context buffer.\n", processor_id);
                }
                return BAD_STATE_E;
            }
        }

        if (! irq_fpu_usable()) {

#ifdef LINUXKM_SIMD_IRQ
            if (am_in_hard_interrupt_handler()) {

                /* allow for nested calls */
                if (((unsigned char *)wolfcrypt_linuxkm_fpu_states[processor_id])[PAGE_SIZE-1] != 0) {
                    if (((unsigned char *)wolfcrypt_linuxkm_fpu_states[processor_id])[PAGE_SIZE-1] == 255) {
                        preempt_enable();
                        pr_err("save_vector_registers_x86 recursion register overflow for "
                               "cpu id %d.\n", processor_id);
                        return BAD_STATE_E;
                    } else {
                        ++((unsigned char *)wolfcrypt_linuxkm_fpu_states[processor_id])[PAGE_SIZE-1];
                        return 0;
                    }
                }
                /* note, fpregs_lock() is not needed here, because
                 * interrupts/preemptions are already disabled here.
                 */
                {
                    /* save_fpregs_to_fpstate() only accesses fpu->state, which
                     * has stringent alignment requirements (64 byte cache
                     * line), but takes a pointer to the parent struct.  work
                     * around this.
                     */
                #if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
                    struct fpu *fake_fpu_pointer =
                        (struct fpu *)(((char *)wolfcrypt_linuxkm_fpu_states[processor_id])
                                       - offsetof(struct fpu, state));
                    copy_fpregs_to_fpstate(fake_fpu_pointer);
                #elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
                    struct fpu *fake_fpu_pointer =
                        (struct fpu *)(((char *)wolfcrypt_linuxkm_fpu_states[processor_id])
                                       - offsetof(struct fpu, state));
                    save_fpregs_to_fpstate(fake_fpu_pointer);
                #else
                    struct fpu *fake_fpu_pointer =
                        (struct fpu *)(((char *)wolfcrypt_linuxkm_fpu_states[processor_id])
                                       - offsetof(struct fpu, fpstate));
                    save_fpregs_to_fpstate(fake_fpu_pointer);
                #endif
                }
                /* mark the slot as used. */
                ((unsigned char *)wolfcrypt_linuxkm_fpu_states[processor_id])[PAGE_SIZE-1] = 1;
                /* note, not preempt_enable()ing, mirroring kernel_fpu_begin()
                 * semantics, even though routine will have been entered already
                 * non-preemptable.
                 */
                return 0;
            } else
#endif /* LINUXKM_SIMD_IRQ */
            {
                preempt_enable();
                return BAD_STATE_E;
            }
        } else {

            /* allow for nested calls */
#ifdef LINUXKM_SIMD_IRQ
            if (((unsigned char *)wolfcrypt_linuxkm_fpu_states[processor_id])[PAGE_SIZE-1] != 0) {
                if (((unsigned char *)wolfcrypt_linuxkm_fpu_states[processor_id])[PAGE_SIZE-1] == 255) {
                    preempt_enable();
                    pr_err("save_vector_registers_x86 recursion register overflow for "
                           "cpu id %d.\n", processor_id);
                    return BAD_STATE_E;
                } else {
                    ++((unsigned char *)wolfcrypt_linuxkm_fpu_states[processor_id])[PAGE_SIZE-1];
                    return 0;
                }
            }
            kernel_fpu_begin();
            preempt_enable(); /* kernel_fpu_begin() does its own
                               * preempt_disable().  decrement ours.
                               */
            ((unsigned char *)wolfcrypt_linuxkm_fpu_states[processor_id])[PAGE_SIZE-1] = 1;
#else /* !LINUXKM_SIMD_IRQ */
            if (wolfcrypt_linuxkm_fpu_states[processor_id] != 0) {
                if (wolfcrypt_linuxkm_fpu_states[processor_id] == ~0U) {
                    preempt_enable();
                    pr_err("save_vector_registers_x86 recursion register overflow for "
                           "cpu id %d.\n", processor_id);
                    return BAD_STATE_E;
                } else {
                    ++wolfcrypt_linuxkm_fpu_states[processor_id];
                    return 0;
                }
            }
            kernel_fpu_begin();
            preempt_enable(); /* kernel_fpu_begin() does its own
                               * preempt_disable().  decrement ours.
                               */
            wolfcrypt_linuxkm_fpu_states[processor_id] = 1;
#endif /* !LINUXKM_SIMD_IRQ */

            return 0;
        }
    }
    void restore_vector_registers_x86(void)
    {
        int processor_id = smp_processor_id();

        if ((wolfcrypt_linuxkm_fpu_states == NULL)
#ifdef LINUXKM_SIMD_IRQ
            || (wolfcrypt_linuxkm_fpu_states[processor_id] == NULL)
#endif
           )
        {
                pr_err("restore_vector_registers_x86 called for cpu id %d "
                       "with null context buffer.\n", processor_id);
                return;
        }

#ifdef LINUXKM_SIMD_IRQ
        if (((unsigned char *)wolfcrypt_linuxkm_fpu_states[processor_id])[PAGE_SIZE-1] == 0)
        {
            pr_err("restore_vector_registers_x86 called for cpu id %d "
                   "without saved context.\n", processor_id);
            return;
        }

        if (--((unsigned char *)wolfcrypt_linuxkm_fpu_states[processor_id])[PAGE_SIZE-1] > 0) {
            preempt_enable(); /* preempt_disable count will still be nonzero after this decrement. */
            return;
        }

        if (am_in_hard_interrupt_handler()) {
        #if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
            copy_kernel_to_fpregs(wolfcrypt_linuxkm_fpu_states[processor_id]);
        #elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
            __restore_fpregs_from_fpstate(wolfcrypt_linuxkm_fpu_states[processor_id],
                                          xfeatures_mask_all);
        #else
            restore_fpregs_from_fpstate(wolfcrypt_linuxkm_fpu_states[processor_id],
                                          fpu_kernel_cfg.max_features);
        #endif
            preempt_enable();
        } else {
            kernel_fpu_end();
        }
#else /* !LINUXKM_SIMD_IRQ */
        if (wolfcrypt_linuxkm_fpu_states[processor_id] == 0)
        {
            pr_err("restore_vector_registers_x86 called for cpu id %d "
                   "without saved context.\n", processor_id);
            return;
        }

        if (--wolfcrypt_linuxkm_fpu_states[processor_id] > 0) {
            preempt_enable(); /* preempt_disable count will still be nonzero after this decrement. */
            return;
        }

        kernel_fpu_end();
#endif /* !LINUXKM_SIMD_IRQ */

        return;
    }
#endif /* WOLFSSL_LINUXKM_SIMD_X86 && WOLFSSL_LINUXKM_SIMD_X86_IRQ_ALLOWED */

#if defined(__PIE__) && (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
/* needed in 6.1+ because show_free_areas() static definition in mm.h calls
 * __show_free_areas(), which isn't exported (neither was show_free_areas()).
 */
void my__show_free_areas(
    unsigned int flags,
    nodemask_t *nodemask,
    int max_zone_idx)
{
    (void)flags;
    (void)nodemask;
    (void)max_zone_idx;
    return;
}
#endif
