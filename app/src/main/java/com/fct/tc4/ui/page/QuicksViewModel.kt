// QuicksViewModel.kt -- This file is part of tiny_container.
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

package com.fct.tc4.ui.page

import android.app.Application
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import com.fct.tc4.R
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.yaml.snakeyaml.Yaml

class QuicksViewModel(application: Application) : AndroidViewModel(application) {

    enum class Tab { COMMANDS, OPTIONS }

    // ===================== Config 引用 =====================

    private var configRef: MutableMap<String, Any>? = null
    private var saveAction: (() -> Unit)? = null

    fun attach(config: Map<String, Any>, save: () -> Unit) {
        if (configRef != null) return
        @Suppress("UNCHECKED_CAST")
        configRef = config as MutableMap<String, Any>
        saveAction = save
        refresh()
    }

    // ===================== 导航状态 =====================

    private val _currentTab = MutableStateFlow(Tab.COMMANDS)
    val currentTab: StateFlow<Tab> = _currentTab.asStateFlow()

    /** 各自保留 path，切换 tab 时不丢失 */
    private val commandsPath = mutableListOf<Int>()
    private val optionsPath = mutableListOf<Int>()
    private val currentPath: MutableList<Int>
        get() = if (_currentTab.value == Tab.COMMANDS) commandsPath else optionsPath
    val pathDepth: Int get() = currentPath.size

    // ===================== UI 状态（合并为单次发射） =====================

    private val _displayState = MutableStateFlow(QuickDisplayState())
    val displayState: StateFlow<QuickDisplayState> = _displayState.asStateFlow()

    // ===================== 多选状态 =====================

    private val _selectedIndices = MutableStateFlow<Set<Int>>(emptySet())
    val selectedIndices: StateFlow<Set<Int>> = _selectedIndices.asStateFlow()

    // ===================== 路径管理 =====================

    private val rootKey: String
        get() = if (_currentTab.value == Tab.COMMANDS) "quick_commands" else "options"

    private val subKey: String
        get() = if (_currentTab.value == Tab.COMMANDS) "commands" else "options"

    @Suppress("UNCHECKED_CAST")
    private fun getCurrentList(): List<MutableMap<String, Any>>? {
        val root = configRef?.get(rootKey) as? List<MutableMap<String, Any>> ?: return null
        var current: List<MutableMap<String, Any>> = root
        for (idx in currentPath) {
            val entry = current.getOrNull(idx) ?: return null
            current = entry[subKey] as? List<MutableMap<String, Any>>
                ?: mutableListOf<MutableMap<String, Any>>().also { entry[subKey] = it }
        }
        return current
    }

    @Suppress("UNCHECKED_CAST")
    private fun getNodeAtPath(path: List<Int>): Map<String, Any>? {
        val root = configRef?.get(rootKey) as? List<Map<String, Any>> ?: return null
        if (path.isEmpty()) return null
        var current: List<Map<String, Any>> = root
        for ((i, idx) in path.withIndex()) {
            val entry = current.getOrNull(idx) ?: return null
            if (i == path.lastIndex) return entry
            current = entry[subKey] as? List<Map<String, Any>> ?: return null
        }
        return null
    }

    // ===================== Tab 切换 =====================

    fun switchTab(tab: Tab) {
        if (_currentTab.value == tab) return
        _currentTab.value = tab
        _selectedIndices.value = emptySet()
        refresh()
    }

    // ===================== 导航 =====================

    fun navigateTo(index: Int) {
        currentPath.add(index)
        _selectedIndices.value = emptySet()
        refresh()
    }

    fun navigateToBreadcrumb(level: Int) {
        // level -1 = root, level 0 = first folder, level 1 = second folder
        // targetSize = level + 1
        val targetSize = level + 1
        while (currentPath.size > targetSize) {
            currentPath.removeAt(currentPath.lastIndex)
        }
        _selectedIndices.value = emptySet()
        refresh()
    }

    // ===================== 多选操作 =====================

    fun toggleSelection(index: Int) {
        val current = _selectedIndices.value
        _selectedIndices.value = if (index in current) current - index else current + index
        refresh()
    }

    fun clearSelection() {
        _selectedIndices.value = emptySet()
        refresh()
    }

    fun getSelectedRawNodes(): List<Map<String, Any>> {
        val list = getCurrentList() ?: return emptyList()
        return _selectedIndices.value.sorted().mapNotNull { list.getOrNull(it) }
    }

    /** 获取单个选中节点的配置（用于编辑） */
    fun getSingleSelectedConfig(): Map<String, Any>? {
        val sel = _selectedIndices.value
        if (sel.size != 1) return null
        return getCurrentList()?.getOrNull(sel.first())
    }

    fun getSingleSelectedIndex(): Int? {
        val sel = _selectedIndices.value
        return if (sel.size == 1) sel.first() else null
    }

    // ===================== 增删改 =====================

    fun addFolder(name: String, description: String) {
        val list = getCurrentList() ?: return
        val folderType = if (_currentTab.value == Tab.COMMANDS) "commands" else "options"
        val node = mutableMapOf<String, Any>("type" to folderType, "name" to name)
        if (description.isNotBlank()) node["description"] = description
        node[subKey] = mutableListOf<Any>()
        @Suppress("UNCHECKED_CAST")
        (list as MutableList<Any>).add(node)
        save()
    }

    fun addNode(data: Map<String, Any>) {
        val list = getCurrentList() ?: return
        @Suppress("UNCHECKED_CAST")
        (list as MutableList<Any>).add(data)
        save()
    }

    @Suppress("UNCHECKED_CAST")
    fun updateNode(index: Int, data: Map<String, Any>) {
        val list = getCurrentList() ?: return
        if (index !in list.indices) return
        val existing = list[index] as MutableMap<String, Any>

        // 编辑对话框管理的字段：根据节点类型不同
        // 不在集合中的字段（如 enabled、commands/options 子列表）将被保留
        val dialogManagedKeys = mutableSetOf("name", "description")
        when (existing["type"] as? String) {
            "option" -> dialogManagedKeys += setOf(
                "env", "path", "ld_library_path", "ld_preload", "args",
                "pre_start_host_command", "post_start_container_command",
                "post_end_host_command"
            )
            "command" -> dialogManagedKeys += "command"
        }

        // 先移除对话框管理的旧值（支持用户清空字段）
        existing.keys.removeAll { it in dialogManagedKeys }
        // 合并新值进已有 Map（enabled、type、子节点等保持不变）
        existing.putAll(data)
        save()
    }

    @Suppress("UNCHECKED_CAST")
    fun deleteSelected() {
        val list = getCurrentList() ?: return
        val mutable = list as MutableList<Any>
        val indices = _selectedIndices.value.sortedDescending()
        for (idx in indices) {
            if (idx in mutable.indices) mutable.removeAt(idx)
        }
        _selectedIndices.value = emptySet()
        save()
    }

    /** 导出选中节点到剪贴板 */
    fun exportSelected(context: Context) {
        val nodes = getSelectedRawNodes()
        if (nodes.isEmpty()) return
        val yaml = Yaml()
        val yamlText = yaml.dump(nodes)
        val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        clipboard.setPrimaryClip(ClipData.newPlainText("yaml", yamlText))
    }

    /** 导出整份 Quicks 配置（quick_commands + options）为 YAML 文本，用于写入文件 */
    fun exportAllToYaml(): String {
        val config = configRef ?: return ""
        return exportConfigToYaml(config)
    }

    // ===================== 叶子节点操作 =====================

    @Suppress("UNCHECKED_CAST")
    fun toggleOption(index: Int) {
        val list = getCurrentList() ?: return
        val node = list.getOrNull(index) ?: return
        val current = node["enabled"] as? Boolean ?: false
        node["enabled"] = !current
        save()
    }

    // ===================== 导入 =====================

    data class ImportResult(val successCount: Int, val errors: List<String>)

    fun importFromClipboard(context: Context): ImportResult {
        val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clip = clipboard.primaryClip ?: return ImportResult(0, listOf(context.getString(R.string.tc4_validate_clipboard_empty)))
        val text = clip.getItemAt(0)?.text?.toString() ?: return ImportResult(0, listOf(context.getString(R.string.tc4_validate_clipboard_read)))

        val yaml = Yaml()
        val parsed: List<Any> = try {
            val raw: Any? = yaml.load(text)
            when (raw) {
                is List<*> -> (raw as List<Any?>).filterNotNull()
                null -> return ImportResult(0, listOf(context.getString(R.string.tc4_validate_clipboard_content_empty)))
                else -> listOf(raw as Any)
            }
        } catch (e: Exception) {
            return ImportResult(0, listOf(context.getString(R.string.tc4_validate_yaml_format_error, e.message)))
        }

        val currentTabValue = _currentTab.value
        val validTypes = when (currentTabValue) {
            Tab.COMMANDS -> setOf("command", "commands")
            Tab.OPTIONS -> setOf("option", "options")
        }

        val errors = mutableListOf<String>()
        val validNodes = mutableListOf<Map<String, Any>>()

        parsed.forEachIndexed { i, obj ->
            val err = validateNode(obj, validTypes, context)
            if (err != null) {
                errors.add(getApplication<Application>().getString(R.string.tc4_validate_yaml_line_error, i + 1, err))
            } else {
                @Suppress("UNCHECKED_CAST")
                validNodes.add(obj as Map<String, Any>)
            }
        }

        if (validNodes.isNotEmpty()) {
            val list = getCurrentList() ?: return ImportResult(0, listOf(context.getString(R.string.tc4_validate_list_unavailable)))
            @Suppress("UNCHECKED_CAST")
            (list as MutableList<Any>).addAll(validNodes.map { it.toMutableMap() })
            save()
        }

        return ImportResult(validNodes.size, errors)
    }

    /** 从文件读取的 YAML 文本导入整份配置：校验后合并 quick_commands 与 options 并保存 */
    fun importAllFromYaml(text: String, context: Context): ImportResult {
        val parsed: Map<*, *> = try {
            when (val raw: Any? = Yaml().load(text)) {
                is Map<*, *> -> raw
                null -> return ImportResult(0, listOf(context.getString(R.string.tc4_validate_clipboard_content_empty)))
                else -> return ImportResult(0, listOf(context.getString(R.string.tc4_validate_yaml_not_object)))
            }
        } catch (e: Exception) {
            return ImportResult(0, listOf(context.getString(R.string.tc4_validate_yaml_format_error, e.message)))
        }

        val errors = mutableListOf<String>()
        var successCount = 0
        successCount += importSubtree(parsed["quick_commands"], "quick_commands", setOf("command", "commands"), context, errors)
        successCount += importSubtree(parsed["options"], "options", setOf("option", "options"), context, errors)

        if (successCount > 0) save()
        return ImportResult(successCount, errors)
    }

    @Suppress("UNCHECKED_CAST")
    private fun importSubtree(
        raw: Any?, rootKey: String, validTypes: Set<String>,
        context: Context, errors: MutableList<String>
    ): Int {
        if (raw == null) return 0
        val nodes = (raw as? List<*>)?.filterNotNull() ?: run {
            errors.add(context.getString(R.string.tc4_validate_yaml_not_object))
            return 0
        }

        val validNodes = mutableListOf<Map<String, Any>>()
        nodes.forEachIndexed { i, obj ->
            val err = validateNode(obj, validTypes, context)
            if (err != null) {
                errors.add(getApplication<Application>().getString(R.string.tc4_validate_yaml_line_error, i + 1, err))
            } else {
                validNodes.add(obj as Map<String, Any>)
            }
        }

        if (validNodes.isNotEmpty()) {
            val list = configRef?.get(rootKey) as? MutableList<Any>
                ?: mutableListOf<Any>().also { configRef?.put(rootKey, it) }
            list.addAll(validNodes.map { it.toMutableMap() })
        }
        return validNodes.size
    }

    private fun validateNode(node: Any?, validTypes: Set<String>, context: Context): String? {
        if (node !is Map<*, *>) return context.getString(R.string.tc4_validate_yaml_not_object)
        val type = node["type"]
        if (type !is String) return context.getString(R.string.tc4_validate_yaml_missing_type)
        if (type !in validTypes) return context.getString(R.string.tc4_validate_yaml_invalid_type, type, validTypes.joinToString(", "))

        if (type == "commands" || type == "options") {
            val sub = node[type]
            if (sub !is List<*>) return context.getString(R.string.tc4_validate_yaml_missing_type)
            for ((j, child) in sub.withIndex()) {
                if (child == null) return getApplication<Application>().getString(R.string.tc4_validate_yaml_child_null, j)
                val childErr = validateNode(child, validTypes, context)
                if (childErr != null) return getApplication<Application>().getString(R.string.tc4_validate_yaml_child_error, j, childErr)
            }
        }
        return null
    }

    // ===================== 刷新 =====================

    @Suppress("UNCHECKED_CAST")
    private fun refresh() {
        val list = getCurrentList()
        val sel = _selectedIndices.value

        val nodes = list?.let { lst ->
            val count = lst.size
            lst.mapIndexed { i, m ->
                val type = m["type"] as? String ?: "unknown"
                val name = m["name"] as? String ?: ""
                val desc = m["description"] as? String ?: ""
                when (type) {
                    "command" -> QuicksNodeItem.CommandItem(
                        index = i, name = name, description = desc,
                        isSelected = i in sel, totalCount = count,
                        command = when (val raw = m["command"]) {
                            is String -> raw
                            is ByteArray -> raw.decodeToString()
                            else -> ""
                        }
                    )

                    "commands" -> QuicksNodeItem.CommandsFolderItem(
                        index = i, name = name, description = desc,
                        isSelected = i in sel, totalCount = count,
                        childCount = (m["commands"] as? List<*>)?.size ?: 0
                    )
                    "option" -> QuicksNodeItem.OptionItem(
                        index = i, name = name, description = desc,
                        isSelected = i in sel, totalCount = count,
                        enabled = m["enabled"] as? Boolean ?: false
                    )
                    "options" -> QuicksNodeItem.OptionsFolderItem(
                        index = i, name = name, description = desc,
                        isSelected = i in sel, totalCount = count,
                        childCount = (m["options"] as? List<*>)?.size ?: 0
                    )
                    else -> QuicksNodeItem.CommandItem(
                        index = i, name = name, description = desc,
                        isSelected = i in sel, totalCount = count,
                        command = ""
                    )
                }
            }
        } ?: emptyList()

        val breadcrumbs = generateBreadcrumbs()

        val (headerTitle, headerDesc) = if (currentPath.isEmpty()) {
            val app = getApplication<Application>()
            if (_currentTab.value == Tab.COMMANDS)
                app.getString(R.string.tc4_quick_commands_tab) to app.getString(R.string.tc4_quick_commands_default_desc)
            else
                app.getString(R.string.tc4_quick_options_tab) to app.getString(R.string.tc4_quick_options_default_desc)
        } else {
            val parentNode = getNodeAtPath(currentPath)
            (parentNode?.get("name") as? String ?: "") to
                    (parentNode?.get("description") as? String ?: "")
        }

        _displayState.value = QuickDisplayState(
            nodes = nodes,
            breadcrumbs = breadcrumbs,
            headerTitle = headerTitle,
            headerDesc = headerDesc
        )
    }

    @Suppress("UNCHECKED_CAST")
    private fun generateBreadcrumbs(): List<BreadcrumbChip> {
        val app = getApplication<Application>()
        val crumbs = mutableListOf(
            BreadcrumbChip(
                label = if (_currentTab.value == Tab.COMMANDS)
                    app.getString(R.string.tc4_quick_commands_tab)
                else
                    app.getString(R.string.tc4_quick_options_tab),
                level = -1
            )
        )
        val root = configRef?.get(rootKey) as? List<Map<String, Any>> ?: return crumbs
        var current: List<Map<String, Any>> = root
        for ((depth, idx) in currentPath.withIndex()) {
            val entry = current.getOrNull(idx) ?: break
            val name = entry["name"] as? String ?: "?"
            crumbs.add(BreadcrumbChip(label = name, level = depth))
            if (depth < currentPath.lastIndex) {
                current = entry[subKey] as? List<Map<String, Any>> ?: break
            }
        }
        return crumbs
    }

    private fun save() {
        saveAction?.invoke()
        refresh()
    }

    class Factory(private val application: Application) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T {
            return QuicksViewModel(application) as T
        }
    }

    companion object {
        /** 将整份配置的 quick_commands 与 options 子树序列化为 YAML（不依赖 Android Context，便于单测） */
        fun exportConfigToYaml(config: Map<String, Any>): String {
            val export = LinkedHashMap<String, Any>()
            (config["quick_commands"] as? List<*>)?.let { export["quick_commands"] = it }
            (config["options"] as? List<*>)?.let { export["options"] = it }
            return Yaml().dump(export)
        }
    }
}

// ===================== 数据模型 =====================

data class QuickDisplayState(
    val nodes: List<QuicksNodeItem> = emptyList(),
    val breadcrumbs: List<BreadcrumbChip> = emptyList(),
    val headerTitle: String = "",
    val headerDesc: String = ""
)

sealed class QuicksNodeItem {
    abstract val index: Int
    abstract val name: String
    abstract val description: String
    abstract val isSelected: Boolean
    abstract val totalCount: Int

    data class CommandItem(
        override val index: Int,
        override val name: String,
        override val description: String,
        override val isSelected: Boolean,
        override val totalCount: Int,
        val command: String
    ) : QuicksNodeItem()

    data class CommandsFolderItem(
        override val index: Int,
        override val name: String,
        override val description: String,
        override val isSelected: Boolean,
        override val totalCount: Int,
        val childCount: Int
    ) : QuicksNodeItem()

    data class OptionItem(
        override val index: Int,
        override val name: String,
        override val description: String,
        override val isSelected: Boolean,
        override val totalCount: Int,
        val enabled: Boolean
    ) : QuicksNodeItem()

    data class OptionsFolderItem(
        override val index: Int,
        override val name: String,
        override val description: String,
        override val isSelected: Boolean,
        override val totalCount: Int,
        val childCount: Int
    ) : QuicksNodeItem()
}

data class BreadcrumbChip(
    val label: String,
    val level: Int  // -1 = 根, 0..n = 第几层
)
