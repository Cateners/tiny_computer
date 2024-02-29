/*
 * Copyright (c) 2021  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.ui.about

import android.content.res.AssetManager
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import com.example.tiny_computer.R
import com.example.tiny_computer.databinding.FragmentLicenseBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class LicenseFragment : Fragment() {

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        val binding = FragmentLicenseBinding.inflate(inflater, container, false)
        loadLicenses(binding.licenseText, resources.assets)
        return binding.root
    }

    override fun onResume() {
        super.onResume()
        requireActivity().setTitle(R.string.title_license)
    }


    // These are relative to assets directory
    private val licenseFiles = listOf(
            "license/GPL-3.0.txt",
            "license/Apache-2.0.txt",
            "license/BSD-libjpeg-turbo.txt",
            "license/sshlib.txt",
            "license/X11.txt",
    )

    private fun loadLicenses(tv: TextView, assets: AssetManager) {
        lifecycleScope.launch(Dispatchers.IO) {
            var combinedText = ""

            licenseFiles.forEach {
                val reader = assets.open(it).reader()
                val text = reader.readText()
                reader.close()

                combinedText += text
                combinedText += "\n\n------------------------------------------------------------------------------\n\n"
            }

            withContext(Dispatchers.Main) {
                tv.text = combinedText
            }
        }
    }
}