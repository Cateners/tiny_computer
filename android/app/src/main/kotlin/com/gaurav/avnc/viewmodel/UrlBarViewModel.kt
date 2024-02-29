/*
 * Copyright (c) 2023  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.viewmodel

import android.app.Application
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.switchMap

class UrlBarViewModel(app: Application) : BaseViewModel(app) {

    val query = MutableLiveData("")
    val filteredServers = query.switchMap {
        if (it.isNotBlank()) serverProfileDao.search("%$it%")
        else MutableLiveData(listOf())
    }
}