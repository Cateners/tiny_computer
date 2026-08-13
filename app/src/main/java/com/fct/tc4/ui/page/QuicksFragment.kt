// QuicksFragment.kt -- This file is part of tiny_container.
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

import android.content.Context
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.fct.tc4.R
import com.fct.tc4.databinding.Tc4FragmentQuicksBinding
import com.fct.tc4.databinding.Tc4QuickCommandItemBinding
import com.fct.tc4.databinding.Tc4QuickFolderItemBinding
import com.fct.tc4.databinding.Tc4QuickOptionItemBinding
import com.fct.tc4.ui.main.MainViewModel
import com.fct.tc4.ui.misc.ConfirmDialogFragment
import com.fct.tc4.ui.misc.Global
import com.fct.tc4.ui.misc.QuickNodeEditDialogFragment
import android.content.res.ColorStateList
import android.graphics.Color.alpha
import android.graphics.Color.argb
import android.graphics.Color.blue
import android.graphics.Color.green
import android.graphics.Color.red
import com.google.android.material.button.MaterialButton
import com.google.android.material.color.MaterialColors
import com.google.android.material.listitem.ListItemViewHolder
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.launch
import java.util.Stack

class QuicksFragment : Fragment() {

    companion object {
        private const val CONFIRM_DELETE_KEY = "confirm_quick_delete"
    }

    private var _binding: Tc4FragmentQuicksBinding? = null
    private val binding get() = _binding!!

    private val parentVM: ContainerMainViewModel by lazy {
        (requireParentFragment() as ContainerMainFragment).viewModel
    }

    private val mainViewModel: MainViewModel by activityViewModels()
    private val viewModel: QuicksViewModel by viewModels {
        QuicksViewModel.Factory(requireActivity().application)
    }

    private lateinit var adapter: QuicksNodesAdapter
    /** 每个深度的滚动位置栈，进入子项时 push，返回时 pop */
    private data class ScrollPos(val position: Int, val offset: Int)
    private val scrollPositions = Stack<ScrollPos>()
    private var prevBreadcrumbDepth = 0
    private lateinit var backCallback: OnBackPressedCallback

    private val currentTabName: String
        get() = when (viewModel.currentTab.value) {
            QuicksViewModel.Tab.COMMANDS -> "commands"
            QuicksViewModel.Tab.OPTIONS -> "options"
        }

    // ========== 文件导入/导出（Storage Access Framework） ==========

    private val exportToFileLauncher = registerForActivityResult(
        ActivityResultContracts.CreateDocument("application/x-yaml")
    ) { uri -> uri?.let { writeExport(it) } }

    private val importFromFileLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri -> uri?.let { readImport(it) } }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = Tc4FragmentQuicksBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // ========== 注册结果监听 ==========

        childFragmentManager.setFragmentResultListener(
            QuickNodeEditDialogFragment.REQUEST_SAVE, viewLifecycleOwner
        ) { _, bundle ->
            val mode = bundle.getString(QuickNodeEditDialogFragment.KEY_MODE) ?: return@setFragmentResultListener
            val name = bundle.getString(QuickNodeEditDialogFragment.KEY_NAME, "")

            when (mode) {
                "add_folder" -> {
                    val desc = bundle.getString(QuickNodeEditDialogFragment.KEY_DESC, "")
                    viewModel.addFolder(name, desc)
                    Snackbar.make(binding.root, R.string.tc4_quick_folder_added, Snackbar.LENGTH_SHORT).show()
                }
                "add_command", "add_option" -> {
                    val node = buildNodeFromBundle(bundle)
                    viewModel.addNode(node)
                    val resId = if (currentTabName == "commands")
                        R.string.tc4_quick_command_added else R.string.tc4_quick_option_added
                    Snackbar.make(binding.root, resId, Snackbar.LENGTH_SHORT).show()
                }
                "edit" -> {
                    val index = bundle.getInt(QuickNodeEditDialogFragment.KEY_INDEX, -1)
                    if (index < 0) return@setFragmentResultListener
                    val node = buildNodeFromBundle(bundle)
                    viewModel.updateNode(index, node)
                    viewModel.clearSelection()
                    Snackbar.make(binding.root, R.string.tc4_quick_updated, Snackbar.LENGTH_SHORT).show()
                }
            }
        }

        childFragmentManager.setFragmentResultListener(
            CONFIRM_DELETE_KEY, viewLifecycleOwner
        ) { _, bundle ->
            if (bundle.getBoolean("confirmed")) {
                viewModel.deleteSelected()
                Snackbar.make(binding.root, R.string.tc4_quick_deleted, Snackbar.LENGTH_SHORT).show()
            }
        }

        // ========== RecyclerView ==========

        adapter = QuicksNodesAdapter(object : QuicksNodeCallbacks {
            override fun onItemClick(index: Int, item: QuicksNodeItem) {
                // 防止快速点击时 adapter 当前列表尚未完成 DiffUtil 异步更新,
                // 导致使用过时 item 执行错误操作(如将子命令当作文件夹打开)
                val currentItem = viewModel.displayState.value.nodes.getOrNull(index)
                if (currentItem != item) return

                // 选择模式下点击 = 切换选中
                if (viewModel.selectedIndices.value.isNotEmpty()) {
                    viewModel.toggleSelection(index)
                    return
                }
                when (item) {
                    is QuicksNodeItem.CommandItem -> {
                        if (item.command.isNotBlank()) {
                            Global.sendCommand(item.command)
                            mainViewModel.toggleTerminal()
                        }
                    }
                    is QuicksNodeItem.CommandsFolderItem -> {
                        saveScrollState()
                        viewModel.navigateTo(index)
                    }
                    is QuicksNodeItem.OptionItem -> viewModel.toggleOption(index)
                    is QuicksNodeItem.OptionsFolderItem -> {
                        saveScrollState()
                        viewModel.navigateTo(index)
                    }
                }
            }

            override fun onItemLongClick(index: Int) {
                viewModel.toggleSelection(index)
            }
        })
        binding.recyclerView.adapter = adapter
        binding.recyclerView.layoutManager = LinearLayoutManager(requireContext())

        // ========== ButtonGroup 切换 ==========

        binding.commands.setOnClickListener {
            viewModel.switchTab(QuicksViewModel.Tab.COMMANDS)
        }
        binding.options.setOnClickListener {
            viewModel.switchTab(QuicksViewModel.Tab.OPTIONS)
        }

        // ========== import_from_clipboard ==========

        binding.importFromClipboard.setOnClickListener {
            val result = viewModel.importFromClipboard(requireContext())
            if (result.errors.isEmpty()) {
                Snackbar.make(binding.root, getString(R.string.tc4_quick_import_success, result.successCount), Snackbar.LENGTH_SHORT).show()
            } else if (result.successCount > 0) {
                Snackbar.make(
                    binding.root,
                    getString(R.string.tc4_quick_import_partial, result.successCount, result.errors.size),
                    Snackbar.LENGTH_LONG
                ).setAction(R.string.tc4_btn_details) {
                    Snackbar.make(binding.root, result.errors.joinToString("\n"), Snackbar.LENGTH_LONG).show()
                }.show()
            } else {
                Snackbar.make(binding.root, getString(R.string.tc4_quick_import_failed, result.errors.first()), Snackbar.LENGTH_LONG).show()
            }
        }

        // ========== export_to_file / import_from_file ==========

        binding.exportToFile.setOnClickListener {
            exportToFileLauncher.launch("tiny_container_quicks.yaml")
        }

        binding.importFromFile.setOnClickListener {
            importFromFileLauncher.launch(arrayOf("*/*"))
        }

        // ========== add_folder ==========

        binding.addFolder.setOnClickListener {
            QuickNodeEditDialogFragment.newAddFolder(currentTabName)
                .show(childFragmentManager, "add_folder")
        }

        // ========== add ==========

        binding.add.setOnClickListener {
            QuickNodeEditDialogFragment.newAddLeaf(currentTabName)
                .show(childFragmentManager, "add_leaf")
        }

        // ========== Toolbar ==========

        binding.delete.setOnClickListener {
            val names = viewModel.getSelectedRawNodes()
                .mapNotNull { it["name"] as? String }
                .filter { it.isNotEmpty() }
            val message = if (names.isEmpty()) {
                getString(R.string.tc4_quick_delete_selected)
            } else {
                getString(R.string.tc4_quick_delete_named, names.joinToString("\n"))
            }
            ConfirmDialogFragment.show(
                childFragmentManager, CONFIRM_DELETE_KEY,
                title = getString(R.string.tc4_quick_delete_title),
                message = message,
                positiveText = getString(R.string.tc4_btn_delete),
                negativeText = getString(R.string.tc4_btn_cancel)
            )
        }

        binding.copy.setOnClickListener {
            viewModel.exportSelected(requireContext())
            val count = viewModel.selectedIndices.value.size
            Snackbar.make(binding.root, getString(R.string.tc4_quick_copied, count), Snackbar.LENGTH_SHORT).show()
            viewModel.clearSelection()
        }

        binding.edit.setOnClickListener {
            val index = viewModel.getSingleSelectedIndex() ?: return@setOnClickListener
            val config = viewModel.getSingleSelectedConfig() ?: return@setOnClickListener
            val type = config["type"] as? String ?: return@setOnClickListener
            QuickNodeEditDialogFragment.newEdit(currentTabName, index, type, config)
                .show(childFragmentManager, "edit_$index")
        }

        binding.close.setOnClickListener {
            viewModel.clearSelection()
        }

        // ========== 返回键导航 ==========

        backCallback = object : OnBackPressedCallback(false) {
            override fun handleOnBackPressed() {
                // 优先级高于父 Fragment，终端开着时先关终端
                if (mainViewModel.isTerminalOpen.value) {
                    mainViewModel.closeTerminal()
                    return
                }
                val targetLevel = viewModel.pathDepth - 1  // 当前是第几个子文件夹（0-indexed）
                if (targetLevel >= 0) viewModel.navigateToBreadcrumb(targetLevel - 1)  // 回到上一级
            }
        }
        requireActivity().onBackPressedDispatcher.addCallback(viewLifecycleOwner, backCallback)

        // 页面切换时通过生命周期控制回调启用状态
        lifecycle.addObserver(LifecycleEventObserver { _, event ->
            when (event) {
                Lifecycle.Event.ON_RESUME -> backCallback.isEnabled = viewModel.pathDepth > 0
                Lifecycle.Event.ON_PAUSE -> backCallback.isEnabled = false
                else -> {}
            }
        })

        // ========== 注入 config & 初始同步 ==========

        viewLifecycleOwner.lifecycleScope.launch {
            parentVM.configState.collect { config ->
                if (config != null) {
                    viewModel.attach(config, save = { parentVM.saveConfig() })
                    val active = if (viewModel.currentTab.value == QuicksViewModel.Tab.COMMANDS)
                        binding.commands else binding.options
                    (active as android.widget.Checkable).isChecked = true
                }
            }
        }

        // ========== 观察状态 ==========

        viewLifecycleOwner.lifecycleScope.launch {
            viewModel.displayState.collect { state ->
                binding.name.text = state.headerTitle
                binding.description.text = state.headerDesc.trim()
                renderBreadcrumbs(state.breadcrumbs)

                val isEmpty = state.nodes.isEmpty()
                binding.recyclerView.visibility = if (isEmpty) View.GONE else View.VISIBLE
                binding.emptyHint.visibility = if (isEmpty) View.VISIBLE else View.GONE
                if (isEmpty) {
                    binding.emptyHint.text = when (viewModel.currentTab.value) {
                        QuicksViewModel.Tab.COMMANDS -> getString(R.string.tc4_quick_commands_empty)
                        QuicksViewModel.Tab.OPTIONS -> getString(R.string.tc4_quick_options_empty)
                    }
                }

                backCallback.isEnabled = viewModel.pathDepth > 0

                val depth = state.breadcrumbs.size
                val diff = depth - prevBreadcrumbDepth

                // 回退时恢复滚动位置：pop 出对应层级数的（位置+偏移），支持多级跳回，取最深的一个
                if (diff < 0 && scrollPositions.isNotEmpty()) {
                    val popCount = minOf(-diff, scrollPositions.size)
                    var restoredPos = RecyclerView.NO_POSITION
                    var restoredOff = 0
                    repeat(popCount) {
                        val p = scrollPositions.pop()
                        restoredPos = p.position
                        restoredOff = p.offset
                    }
                    adapter.submitList(state.nodes) {
                        if (restoredPos >= 0) {
                            (binding.recyclerView.layoutManager as LinearLayoutManager)
                                .scrollToPositionWithOffset(restoredPos, restoredOff)
                        }
                    }
                } else {
                    adapter.submitList(state.nodes)
                }
                prevBreadcrumbDepth = depth
            }
        }

        viewLifecycleOwner.lifecycleScope.launch {
            viewModel.selectedIndices.collect { sel ->
                val selecting = sel.isNotEmpty()
                val single = sel.size == 1

                binding.buttonGroup.isEnabled = !selecting
                binding.importFromClipboard.isEnabled = !selecting
                binding.exportToFile.isEnabled = !selecting
                binding.importFromFile.isEnabled = !selecting
                binding.addFolder.isEnabled = !selecting
                binding.add.isEnabled = !selecting
                setChipGroupEnabled(!selecting)

                binding.toolbar.visibility = if (selecting) View.VISIBLE else View.GONE
                binding.edit.visibility = if (single) View.VISIBLE else View.GONE
                binding.close.visibility = if (!single) View.VISIBLE else View.GONE
            }
        }

        // ========== ButtonGroup 图标颜色 ==========

        viewLifecycleOwner.lifecycleScope.launch {
            viewModel.currentTab.collect { tab ->
                // 切 tab 时重置滚动状态
                scrollPositions.clear()
                prevBreadcrumbDepth = 0
                val isCommands = tab == QuicksViewModel.Tab.COMMANDS
                setTabButtonStyle(isCommands)
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    // ===================== 辅助方法 =====================

    private fun saveScrollState() {
        val lm = binding.recyclerView.layoutManager as LinearLayoutManager
        val pos = lm.findFirstVisibleItemPosition()
        val offset = lm.findViewByPosition(pos)?.top ?: 0
        scrollPositions.push(ScrollPos(pos, offset))
    }

    private fun setTabButtonStyle(isCommands: Boolean) {
        val commandsBtn = binding.commands as MaterialButton
        val optionsBtn = binding.options as MaterialButton

        fun styleButton(btn: MaterialButton, selected: Boolean) {
            val ctx = requireContext()
            if (selected) {
                val bgColor = MaterialColors.getColor(ctx, com.google.android.material.R.attr.colorPrimaryContainer, 0)
                val textColor = MaterialColors.getColor(ctx, com.google.android.material.R.attr.colorOnPrimaryContainer, 0)
                btn.backgroundTintList = withDisabledState(bgColor)
                btn.setTextColor(withDisabledState(textColor))
                btn.iconTint = withDisabledState(textColor)
            } else {
                val bgColor = MaterialColors.getColor(ctx, com.google.android.material.R.attr.colorSurface, 0)
                val textColor = MaterialColors.getColor(ctx, com.google.android.material.R.attr.colorOnSurface, 0)
                btn.backgroundTintList = withDisabledState(bgColor)
                btn.setTextColor(withDisabledState(textColor))
                btn.iconTint = withDisabledState(textColor)
            }
        }
        styleButton(commandsBtn, isCommands)
        styleButton(optionsBtn, !isCommands)
    }
    private fun withDisabledState(enabledColor: Int): ColorStateList {
        val a = (alpha(enabledColor) * 0.38f).toInt()
        val disabledColor = argb(a, red(enabledColor), green(enabledColor), blue(enabledColor))
        return ColorStateList(
            arrayOf(
                intArrayOf(-android.R.attr.state_enabled),
                intArrayOf()
            ),
            intArrayOf(disabledColor, enabledColor)
        )
    }

    private fun setChipGroupEnabled(enabled: Boolean) {
        for (i in 0 until binding.chipGroup.childCount) {
            binding.chipGroup.getChildAt(i).isEnabled = enabled
        }
    }

    private fun renderBreadcrumbs(crumbs: List<BreadcrumbChip>) {
        binding.chipGroup.visibility = View.VISIBLE
        val group = binding.chipGroup
        val existing = group.childCount

        // 移除多余的 chip
        while (group.childCount > crumbs.size) {
            group.removeViewAt(group.childCount - 1)
        }

        for ((i, crumb) in crumbs.withIndex()) {
            val chip = if (i < existing) {
                // 复用已有 chip
                group.getChildAt(i) as com.google.android.material.chip.Chip
            } else {
                // 新增 chip
                com.google.android.material.chip.Chip(requireContext()).also { group.addView(it) }
            }
            chip.text = crumb.label
            chip.isClickable = i < crumbs.lastIndex
            chip.setOnClickListener {
                if (i < crumbs.lastIndex) viewModel.navigateToBreadcrumb(crumb.level)
            }
            if (i == crumbs.lastIndex) {
                chip.visibility = View.INVISIBLE
                chip.text = ""
            } else {
                chip.visibility = View.VISIBLE
            }
        }
    }

    /** 将整份 Quicks 配置写入用户选择的文件 */
    private fun writeExport(uri: Uri) {
        try {
            val yamlText = viewModel.exportAllToYaml()
            requireContext().contentResolver.openOutputStream(uri)?.use { out ->
                out.write(yamlText.toByteArray())
            } ?: throw java.io.IOException("openOutputStream returned null")
            Snackbar.make(binding.root, R.string.tc4_quick_export_file_success, Snackbar.LENGTH_SHORT).show()
        } catch (e: Exception) {
            Snackbar.make(
                binding.root,
                getString(R.string.tc4_quick_export_file_failed, e.message),
                Snackbar.LENGTH_LONG
            ).show()
        }
    }

    /** 读取用户选择的文件并导入整份 Quicks 配置 */
    private fun readImport(uri: Uri) {
        val text = try {
            requireContext().contentResolver.openInputStream(uri)?.use { input ->
                input.readBytes().decodeToString()
            } ?: throw java.io.IOException("openInputStream returned null")
        } catch (e: Exception) {
            Snackbar.make(
                binding.root,
                getString(R.string.tc4_quick_import_failed, e.message),
                Snackbar.LENGTH_LONG
            ).show()
            return
        }

        val result = viewModel.importAllFromYaml(text, requireContext())
        if (result.errors.isEmpty()) {
            Snackbar.make(binding.root, getString(R.string.tc4_quick_import_success, result.successCount), Snackbar.LENGTH_SHORT).show()
        } else if (result.successCount > 0) {
            Snackbar.make(
                binding.root,
                getString(R.string.tc4_quick_import_partial, result.successCount, result.errors.size),
                Snackbar.LENGTH_LONG
            ).setAction(R.string.tc4_btn_details) {
                Snackbar.make(binding.root, result.errors.joinToString("\n"), Snackbar.LENGTH_LONG).show()
            }.show()
        } else {
            Snackbar.make(binding.root, getString(R.string.tc4_quick_import_failed, result.errors.first()), Snackbar.LENGTH_LONG).show()
        }
    }

    @Suppress("UNCHECKED_CAST")
    private fun buildNodeFromBundle(bundle: Bundle): Map<String, Any> {
        val leafType = if (currentTabName == "commands") "command" else "option"
        val type = bundle.getString(QuickNodeEditDialogFragment.KEY_TYPE, leafType)
        val node = mutableMapOf<String, Any>("type" to type, "name" to bundle.getString(QuickNodeEditDialogFragment.KEY_NAME, ""))

        val desc = bundle.getString(QuickNodeEditDialogFragment.KEY_DESC, "")
        if (desc.isNotEmpty()) node["description"] = desc

        val cmd = bundle.getString(QuickNodeEditDialogFragment.KEY_CMD, "")
        if (cmd.isNotEmpty()) node["command"] = cmd

        if (type == "option") {
            bundle.getStringArrayList(QuickNodeEditDialogFragment.KEY_ENV)?.let {
                if (it.isNotEmpty()) node["env"] = it
            }
            bundle.getStringArrayList(QuickNodeEditDialogFragment.KEY_PATH)?.let {
                if (it.isNotEmpty()) node["path"] = it
            }
            bundle.getStringArrayList(QuickNodeEditDialogFragment.KEY_LD_PATH)?.let {
                if (it.isNotEmpty()) node["ld_library_path"] = it
            }
            bundle.getStringArrayList(QuickNodeEditDialogFragment.KEY_LD_PRELOAD)?.let {
                if (it.isNotEmpty()) node["ld_preload"] = it
            }
            bundle.getStringArrayList(QuickNodeEditDialogFragment.KEY_ARGS)?.let {
                if (it.isNotEmpty()) node["args"] = it
            }
            val preHost = bundle.getString(QuickNodeEditDialogFragment.KEY_PRE_HOST, "")
            if (preHost.isNotEmpty()) node["pre_start_host_command"] = preHost
            val postContainer = bundle.getString(QuickNodeEditDialogFragment.KEY_POST_CONTAINER, "")
            if (postContainer.isNotEmpty()) node["post_start_container_command"] = postContainer
            val postEndHost = bundle.getString(QuickNodeEditDialogFragment.KEY_POST_END_HOST, "")
            if (postEndHost.isNotEmpty()) node["post_end_host_command"] = postEndHost
        }
        return node
    }
}

// ===================== 回调接口 =====================

interface QuicksNodeCallbacks {
    fun onItemClick(index: Int, item: QuicksNodeItem)
    fun onItemLongClick(index: Int)
}

// ===================== 适配器（ListAdapter + 异步 DiffUtil） =====================

private class QuicksNodesAdapter(
    private val callbacks: QuicksNodeCallbacks
) : ListAdapter<QuicksNodeItem, RecyclerView.ViewHolder>(DIFF_CALLBACK) {

    companion object {
        private const val TYPE_COMMAND = 0
        private const val TYPE_FOLDER = 1
        private const val TYPE_OPTION = 2

        private val DIFF_CALLBACK = object : DiffUtil.ItemCallback<QuicksNodeItem>() {
            override fun areItemsTheSame(old: QuicksNodeItem, new: QuicksNodeItem) =
                old.index == new.index
            override fun areContentsTheSame(old: QuicksNodeItem, new: QuicksNodeItem) =
                old == new
        }
    }

    override fun getItemViewType(position: Int): Int = when (currentList[position]) {
        is QuicksNodeItem.CommandItem -> TYPE_COMMAND
        is QuicksNodeItem.CommandsFolderItem -> TYPE_FOLDER
        is QuicksNodeItem.OptionItem -> TYPE_OPTION
        is QuicksNodeItem.OptionsFolderItem -> TYPE_FOLDER
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        return when (viewType) {
            TYPE_COMMAND -> {
                val b = Tc4QuickCommandItemBinding.inflate(inflater, parent, false)
                CommandVH(b, callbacks)
            }
            TYPE_FOLDER -> {
                val b = Tc4QuickFolderItemBinding.inflate(inflater, parent, false)
                FolderVH(b, callbacks)
            }
            TYPE_OPTION -> {
                val b = Tc4QuickOptionItemBinding.inflate(inflater, parent, false)
                OptionVH(b, callbacks)
            }
            else -> error("unknown view type: $viewType")
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        val item = currentList[position]
        val count = currentList.size
        when (holder) {
            is CommandVH -> holder.bind(item as QuicksNodeItem.CommandItem, position, count)
            is FolderVH -> holder.bind(item, position, count)
            is OptionVH -> holder.bind(item as QuicksNodeItem.OptionItem, position, count)
        }
    }
}

// ===================== ViewHolders =====================

private class CommandVH(
    private val binding: Tc4QuickCommandItemBinding,
    private val callbacks: QuicksNodeCallbacks
) : ListItemViewHolder(binding.root) {

    fun bind(item: QuicksNodeItem.CommandItem, position: Int, itemCount: Int) {
        super.bind(position, itemCount)
        binding.name.text = item.name
        binding.description.text = item.description.trim()
        binding.description.visibility = if (item.description.isEmpty()) View.GONE else View.VISIBLE
        binding.itemCard.setCardBackgroundColor(
            if (item.isSelected) MaterialColors.getColor(
                itemView.context, com.google.android.material.R.attr.colorSecondaryContainer, 0
            ) else MaterialColors.getColor(
                itemView.context, com.google.android.material.R.attr.colorSurfaceContainerHigh, 0
            )
        )
        itemView.setOnClickListener { callbacks.onItemClick(item.index, item) }
        itemView.setOnLongClickListener {
            callbacks.onItemLongClick(item.index)
            true
        }
    }
}

private class FolderVH(
    private val binding: Tc4QuickFolderItemBinding,
    private val callbacks: QuicksNodeCallbacks
) : ListItemViewHolder(binding.root) {

    fun bind(item: QuicksNodeItem, position: Int, itemCount: Int) {
        super.bind(position, itemCount)
        binding.name.text = item.name
        binding.description.text = item.description.trim()
        binding.description.visibility = if (item.description.isEmpty()) View.GONE else View.VISIBLE
        binding.itemCard.setCardBackgroundColor(
            if (item.isSelected) MaterialColors.getColor(
                itemView.context, com.google.android.material.R.attr.colorSecondaryContainer, 0
            ) else MaterialColors.getColor(
                itemView.context, com.google.android.material.R.attr.colorSurfaceContainerHigh, 0
            )
        )
        itemView.setOnClickListener { callbacks.onItemClick(item.index, item) }
        itemView.setOnLongClickListener {
            callbacks.onItemLongClick(item.index)
            true
        }
    }
}

private class OptionVH(
    private val binding: Tc4QuickOptionItemBinding,
    private val callbacks: QuicksNodeCallbacks
) : ListItemViewHolder(binding.root) {

    fun bind(item: QuicksNodeItem.OptionItem, position: Int, itemCount: Int) {
        super.bind(position, itemCount)
        binding.name.text = item.name
        binding.description.text = item.description.trim()
        binding.description.visibility = if (item.description.isEmpty()) View.GONE else View.VISIBLE
        binding.enabled.setOnCheckedChangeListener(null)
        binding.enabled.isChecked = item.enabled
        binding.enabled.setOnCheckedChangeListener { _, _ ->
            callbacks.onItemClick(item.index, item)
        }
        binding.itemCard.setCardBackgroundColor(
            if (item.isSelected) MaterialColors.getColor(
                itemView.context, com.google.android.material.R.attr.colorSecondaryContainer, 0
            ) else MaterialColors.getColor(
                itemView.context, com.google.android.material.R.attr.colorSurfaceContainerHigh, 0
            )
        )
        itemView.setOnClickListener {
            callbacks.onItemClick(item.index, item)
        }
        itemView.setOnLongClickListener {
            callbacks.onItemLongClick(item.index)
            true
        }
    }
}
