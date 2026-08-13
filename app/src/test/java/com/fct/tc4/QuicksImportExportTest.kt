// QuicksImportExportTest.kt -- This file is part of tiny_container.
//
// Copyright (C) 2026 Caten Hu
//
// Tiny Container is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License,
// or any later version.
//
// Tiny Container is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.

package com.fct.tc4

import com.fct.tc4.ui.page.QuicksViewModel
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.yaml.snakeyaml.Yaml

/**
 * Unit tests for the file-based Quicks export serialization. These cover the
 * Context-free serialization helper used by [QuicksViewModel.exportAllToYaml];
 * the import path relies on Android resources and is exercised on-device.
 */
class QuicksImportExportTest {

    private fun sampleConfig(): Map<String, Any> = mapOf(
        "quick_commands" to listOf(
            mapOf("type" to "command", "name" to "ls", "command" to "ls -la"),
            mapOf(
                "type" to "commands", "name" to "folder",
                "commands" to listOf(
                    mapOf("type" to "command", "name" to "echo", "command" to "echo hi")
                )
            )
        ),
        "options" to listOf(
            mapOf("type" to "option", "name" to "verbose", "enabled" to true)
        ),
        "unrelated" to "should not be exported"
    )

    @Test
    fun export_includes_only_quicks_subtrees() {
        val yaml = QuicksViewModel.exportConfigToYaml(sampleConfig())
        assertTrue(yaml.contains("quick_commands"))
        assertTrue(yaml.contains("options"))
        assertTrue(!yaml.contains("unrelated"))
    }

    @Test
    fun export_round_trips_through_yaml() {
        val original = sampleConfig()
        val yaml = QuicksViewModel.exportConfigToYaml(original)

        @Suppress("UNCHECKED_CAST")
        val parsed = Yaml().load<Any?>(yaml) as Map<String, Any>

        assertEquals(original["quick_commands"], parsed["quick_commands"])
        assertEquals(original["options"], parsed["options"])
    }

    @Test
    fun export_omits_absent_subtree() {
        val yaml = QuicksViewModel.exportConfigToYaml(mapOf("quick_commands" to emptyList<Any>()))
        assertTrue(yaml.contains("quick_commands"))
        assertTrue(!yaml.contains("options"))
    }
}
