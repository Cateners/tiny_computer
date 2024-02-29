/*
 * Copyright (c) 2021  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.viewmodel

import android.app.Application
import android.net.Uri
import androidx.lifecycle.MutableLiveData
import androidx.room.withTransaction
import com.gaurav.avnc.model.ServerProfile
import com.gaurav.avnc.util.LiveEvent
import kotlinx.serialization.Serializable
import kotlinx.serialization.decodeFromString
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.io.IOException

/**
 * Viewmodel for preferences activity.
 */
class PrefsViewModel(app: Application) : BaseViewModel(app) {


    /**************************************************************************
     * Import/Export
     *
     * Currently, we are only exporting server profiles but preferences can be
     * exported in the future.
     *
     * Importing/Exporting is done on a background thread.
     **************************************************************************/

    @Serializable
    private data class Container(
            val version: Int = 1,
            val profiles: List<ServerProfile>
    )

    private val serializer = Json {
        encodeDefaults = false
        ignoreUnknownKeys = true
        prettyPrint = true
    }

    val importFinishedEvent = LiveEvent<Boolean>()
    val exportFinishedEvent = LiveEvent<Boolean>()
    var importExportError = MutableLiveData<String>()


    /**
     * Exports data to given [uri].
     */
    fun export(uri: Uri) {
        launchIO {
            runCatching {
                // Serialize
                val profiles = serverProfileDao.getList()
                val data = Container(profiles = profiles)
                val json = serializer.encodeToString(data)

                // Write out
                app.contentResolver.openOutputStream(uri)?.use { stream ->
                    stream.writer().use { it.write(json) }
                } ?: throw IOException("Unable to write the file.")

            }.let {
                importExportError.postValue(it.exceptionOrNull()?.message)
                exportFinishedEvent.fireAsync(it.isSuccess)
            }
        }
    }


    /**
     * Imports data from given [uri].
     */
    fun import(uri: Uri, deleteCurrentServers: Boolean) {
        launchIO {
            runCatching {

                val json = app.contentResolver.openInputStream(uri)?.use { stream ->
                    stream.reader().use { it.readText() }
                } ?: throw IOException("Unable to read the file.")

                // Deserialize
                val data = serializer.decodeFromString<Container>(json)

                //This is where migrations would be applied (if required in future)

                //Update database
                if (deleteCurrentServers) {
                    db.withTransaction {
                        serverProfileDao.deleteAll()
                        serverProfileDao.insert(data.profiles)
                    }
                } else {
                    //Reset IDs so that they don't conflict with saved profiles
                    data.profiles.forEach { it.ID = 0 }
                    serverProfileDao.insert(data.profiles)
                }

            }.let {
                importExportError.postValue(it.exceptionOrNull()?.message)
                importFinishedEvent.fireAsync(it.isSuccess)
            }
        }
    }
}