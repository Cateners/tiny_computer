/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.gaurav.avnc.model.db.MainDb
import com.gaurav.avnc.util.AppPreferences
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlin.coroutines.CoroutineContext
import kotlin.coroutines.EmptyCoroutineContext

/**
 * Base view model.
 */
open class BaseViewModel(val app: Application) : AndroidViewModel(app) {

    protected val db by lazy { MainDb.getInstance(app) }

    protected val serverProfileDao by lazy { db.serverProfileDao }

    val pref by lazy { AppPreferences(app) }

    /**
     * Launches a new coroutine using [viewModelScope], and executes [block] in that coroutine.
     */
    protected fun launch(context: CoroutineContext = EmptyCoroutineContext, block: suspend CoroutineScope.() -> Unit): Job {
        return viewModelScope.launch(context) { this.block() }
    }

    protected fun launchMain(block: suspend CoroutineScope.() -> Unit) = launch(Dispatchers.Main, block)
    protected fun launchIO(block: suspend CoroutineScope.() -> Unit) = launch(Dispatchers.IO, block)
}