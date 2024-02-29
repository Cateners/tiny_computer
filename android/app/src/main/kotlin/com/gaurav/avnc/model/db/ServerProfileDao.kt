/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.model.db

import androidx.lifecycle.LiveData
import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Update
import com.gaurav.avnc.model.ServerProfile

@Dao
interface ServerProfileDao {

    @Query("SELECT * FROM profiles")
    fun getLiveList(): LiveData<List<ServerProfile>>

    @Query("SELECT * FROM profiles ORDER BY name COLLATE NOCASE")
    fun getSortedLiveList(): LiveData<List<ServerProfile>>

    //Synchronous version
    @Query("SELECT * FROM profiles")
    suspend fun getList(): List<ServerProfile>

    @Query("SELECT * FROM profiles WHERE ID = :id")
    suspend fun getByID(id: Long): ServerProfile?

    @Query("SELECT * FROM profiles WHERE name = :name")
    suspend fun getByName(name: String): List<ServerProfile>

    @Query("SELECT * FROM profiles WHERE name LIKE :query OR host LIKE :query OR sshHost LIKE :query ORDER BY useCount DESC")
    fun search(query: String): LiveData<List<ServerProfile>>

    @Insert
    suspend fun insert(profile: ServerProfile): Long

    @Insert
    suspend fun insert(profiles: List<ServerProfile>)

    @Update
    suspend fun update(profile: ServerProfile)

    @Delete
    suspend fun delete(profile: ServerProfile)

    @Query("DELETE FROM profiles")
    suspend fun deleteAll()
}