/*
 * Copyright (c) 2024  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.util

import androidx.annotation.MainThread
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.Observer

/**
 * This class implements single-shot observable events.
 * It is based on [LiveData] which does the heavy lifting for us.
 *
 * Single-shot
 * ===========
 * When this event is fired, it will notify exactly one active observer.
 * If there is no active observer, it will wait for one so that the event
 * is not "lost".
 *
 * This is the main difference between this class & [LiveData]. [LiveData] will
 * notify the future observers to bring them up-to date. This can happen during
 * Activity restarts where old observers are detached and new ones are attached.
 *
 * This class is used for events which should be handled only once.
 * E.g. starting a fragment.
 */
open class LiveEvent<T> {

    private class WrappedData<T>(val data: T, var consumed: Boolean = false)
    private class WrappedObserver<T>(private val observer: Observer<T>) : Observer<WrappedData<T>> {
        override fun onChanged(value: WrappedData<T>) {
            if (!value.consumed) {
                value.consumed = true
                observer.onChanged(value.data)
            }
        }
    }

    private val liveData = MutableLiveData<WrappedData<T>>()
    private val wrappedObservers = mutableMapOf<Observer<T>, WrappedObserver<T>>()

    /**
     * Peek current value of this event, irrespective of whether any observer has been notified.
     */
    val value get() = liveData.value?.data

    /**
     * Fire this event with given data.
     * Must be called from main thread.
     */
    @MainThread
    fun fire(data: T) {
        liveData.value = WrappedData(data)
    }

    /**
     * Asynchronous version of [fire].
     * Can be called from any thread.
     */
    fun fireAsync(data: T) {
        liveData.postValue(WrappedData(data))
    }

    @MainThread
    fun observe(owner: LifecycleOwner, observer: Observer<T>) {
        val wrapped = WrappedObserver(observer)
        wrappedObservers[observer] = wrapped
        liveData.observe(owner, wrapped)
    }

    @MainThread
    fun observeForever(observer: Observer<T>) {
        val wrapped = WrappedObserver(observer)
        wrappedObservers[observer] = wrapped
        liveData.observeForever(wrapped)
    }

    @MainThread
    fun removeObserver(observer: Observer<T>) {
        wrappedObservers.remove(observer)?.let { liveData.removeObserver(it) }
    }
}