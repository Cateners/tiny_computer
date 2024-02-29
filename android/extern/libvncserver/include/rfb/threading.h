/*
 *  LibVNCServer/LibVNCClient common platform threading defines and includes.
 *
 *  Copyright (C) 2020 Christian Beier <dontmind@freeshell.org>
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#ifndef _RFB_COMMON_THREADING_H
#define _RFB_COMMON_THREADING_H

#include <rfb/rfbconfig.h>

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
#include <pthread.h>
#if 0 /* debugging */
#define LOCK(mutex)                   (rfbLog("%s:%d LOCK(%s,0x%x)\n",__FILE__,__LINE__,#mutex,&(mutex)), pthread_mutex_lock(&(mutex)))
#define UNLOCK(mutex)                 (rfbLog("%s:%d UNLOCK(%s,0x%x)\n",__FILE__,__LINE__,#mutex,&(mutex)), pthread_mutex_unlock(&(mutex)))
#define MUTEX(mutex)                  pthread_mutex_t (mutex)
#define INIT_MUTEX(mutex)             (rfbLog("%s:%d INIT_MUTEX(%s,0x%x)\n",__FILE__,__LINE__,#mutex,&(mutex)), pthread_mutex_init(&(mutex),NULL))
#define TINI_MUTEX(mutex)             (rfbLog("%s:%d TINI_MUTEX(%s)\n",__FILE__,__LINE__,#mutex), pthread_mutex_destroy(&(mutex)))
#define TSIGNAL(cond)                 (rfbLog("%s:%d TSIGNAL(%s)\n",__FILE__,__LINE__,#cond), pthread_cond_signal(&(cond)))
#define WAIT(cond,mutex)              (rfbLog("%s:%d WAIT(%s,%s)\n",__FILE__,__LINE__,#cond,#mutex), pthread_cond_wait(&(cond),&(mutex)))
#define COND(cond) pthread_cond_t     (cond)
#define INIT_COND(cond)               (rfbLog("%s:%d INIT_COND(%s)\n",__FILE__,__LINE__,#cond), pthread_cond_init(&(cond),NULL))
#define TINI_COND(cond)               (rfbLog("%s:%d TINI_COND(%s)\n",__FILE__,__LINE__,#cond), pthread_cond_destroy(&(cond)))
#define IF_PTHREADS(x)                x
#else
#if !NONETWORK
#define LOCK(mutex)                   pthread_mutex_lock(&(mutex))
#define UNLOCK(mutex)                 pthread_mutex_unlock(&(mutex))
#endif
#define MUTEX(mutex)                  pthread_mutex_t (mutex)
#define MUTEX_SIZE                    (sizeof(pthread_mutex_t))
#define INIT_MUTEX(mutex)             pthread_mutex_init(&(mutex),NULL)
#define TINI_MUTEX(mutex)             pthread_mutex_destroy(&(mutex))
#define TSIGNAL(cond)                 pthread_cond_signal(&(cond))
#define WAIT(cond,mutex)              pthread_cond_wait(&(cond),&(mutex))
#define COND(cond)                    pthread_cond_t (cond)
#define INIT_COND(cond)               pthread_cond_init(&(cond),NULL)
#define TINI_COND(cond)               pthread_cond_destroy(&(cond))
#define IF_PTHREADS(x)                x
#define THREAD_ROUTINE_RETURN_TYPE    void*
#define THREAD_ROUTINE_RETURN_VALUE   NULL
#define THREAD_SLEEP_MS(ms)           usleep(ms*1000)
#define THREAD_JOIN(thread)           pthread_join(thread, NULL)
#define CURRENT_THREAD_ID             pthread_self()
#endif
#elif defined(LIBVNCSERVER_HAVE_WIN32THREADS)
#include <process.h>
#define LOCK(mutex)                   EnterCriticalSection(&(mutex))
#define UNLOCK(mutex)                 LeaveCriticalSection(&(mutex))
#define MUTEX(mutex)                  CRITICAL_SECTION (mutex)
#define MUTEX_SIZE                    (sizeof(CRITICAL_SECTION))
#define INIT_MUTEX(mutex)             InitializeCriticalSection(&(mutex))
#define TINI_MUTEX(mutex)             DeleteCriticalSection(&(mutex))
#define TSIGNAL(cond)                 WakeAllConditionVariable(&(cond))
#define WAIT(cond,mutex)              SleepConditionVariableCS(&(cond),&(mutex),INFINITE);
#define COND(cond)                    CONDITION_VARIABLE (cond)
#define INIT_COND(cond)               InitializeConditionVariable(&(cond));
#define TINI_COND(cond)
#define IF_PTHREADS(x)
#define THREAD_ROUTINE_RETURN_TYPE    void
#define THREAD_ROUTINE_RETURN_VALUE
#define THREAD_SLEEP_MS(ms)           Sleep(ms)
#define THREAD_JOIN(thread)           WaitForSingleObject((HANDLE)thread, INFINITE)
#define CURRENT_THREAD_ID             GetCurrentThreadId()
#else
#define LOCK(mutex)
#define UNLOCK(mutex)
#define MUTEX(mutex)
#define INIT_MUTEX(mutex)
#define TINI_MUTEX(mutex)
#define TSIGNAL(cond)
#define WAIT(cond,mutex)              this_is_unsupported
#define COND(cond)
#define INIT_COND(cond)
#define TINI_COND(cond)
#define IF_PTHREADS(x)
#endif

#endif /* _RFB_COMMON_THREADING_H */
