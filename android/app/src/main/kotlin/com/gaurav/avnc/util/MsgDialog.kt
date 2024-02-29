package com.gaurav.avnc.util

import android.app.Dialog
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.FragmentManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder


object MsgDialog {

    /**
     * Shows a dialog with given title & message,
     */
    fun show(manager: FragmentManager, title: CharSequence, msg: CharSequence) {
        val fragment = MsgDialogFragment()
        val args = Bundle(2)

        args.putCharSequence("title", title)
        args.putCharSequence("msg", msg)
        fragment.arguments = args

        fragment.show(manager, null)
    }

    class MsgDialogFragment : DialogFragment() {
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            return MaterialAlertDialogBuilder(requireContext())
                    .setTitle(requireArguments().getCharSequence("title"))
                    .setMessage(requireArguments().getCharSequence("msg"))
                    .setPositiveButton(android.R.string.ok) { _, _ -> /* Let it dismiss */ }
                    .create()
        }
    }
}