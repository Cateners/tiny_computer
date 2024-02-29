/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.util

import android.content.Context
import android.util.Log
import android.view.View
import android.widget.AdapterView
import android.widget.SimpleAdapter
import android.widget.Spinner
import androidx.core.view.isInvisible
import androidx.core.view.isVisible
import androidx.databinding.BindingAdapter
import androidx.databinding.InverseBindingAdapter
import androidx.databinding.InverseBindingListener


@BindingAdapter("isVisible")
fun visibilityAdapter(view: View, isVisible: Boolean) {
    view.isVisible = isVisible
}

@BindingAdapter("isInvisible")
fun invisibilityAdapter(view: View, isInvisible: Boolean) {
    view.isInvisible = isInvisible
}

/**************************************************************************************************
 * Spinner value binding
 * These allows the Spinner to be populated using data-binding in XMl layouts.
 * There are 4 attributes used for this:
 *
 * app:value         => Reference to the backing field which actually stores the selected value.
 *                      This can be used with two-way binding to update the backing field whenever
 *                      selection changes in Spinner.
 *                      For now, Only String & Int types are supported for backing field.
 *
 * app:values        => String Array, holds possible values of app:value
 *
 * app:valueLabels   => String Array, Optional. If provided, these labels will be shown in the UI
 *                      instead of raw app:values
 *
 * app:valueDescriptions => String Array, Optional. Short descriptions of values, shown in Spinner
 *                          Popup below each label.
 *
 *
 *************************************************************************************************/
private fun prepareEntryMap(labels: Array<String>, descriptions: Array<String>?) = mutableListOf<Map<String, *>>().apply {
    for (i in labels.indices)
        add(mapOf("label" to labels[i], "description" to descriptions?.getOrNull(i)))
}

private class SpinnerAdapter(context: Context, labels: Array<String>, descriptions: Array<String>?, val values: Array<String>)
    : SimpleAdapter(context, prepareEntryMap(labels, descriptions), android.R.layout.simple_list_item_1,
                    arrayOf("label", "description"), intArrayOf(android.R.id.text1, android.R.id.text2))

@BindingAdapter("valueLabels", "valueDescriptions", "values", "value", requireAll = false)
fun spinnerValueAdapter(spinner: Spinner, labels: Array<String>?, descriptions: Array<String>?, values: Array<String>?, value: Any?) {
    if (spinner.adapter == null && values != null) {
        check(labels == null || labels.size == values.size)
        check(descriptions == null || descriptions.size == values.size)

        val adapter = SpinnerAdapter(spinner.context, labels ?: values, descriptions, values)
        if (descriptions != null)
            adapter.setDropDownViewResource(android.R.layout.simple_list_item_2)

        spinner.adapter = adapter
    }

    if (spinner.adapter != null && value != null) {
        val adapter = spinner.adapter as SpinnerAdapter
        val index = adapter.values.indexOf(value.toString())
        if (index != -1)
            spinner.setSelection(index)
        else
            Log.e("SpinnerValue", "value: $value not found in adapter values: ${adapter.values}")
    }
}

@BindingAdapter("valueAttrChanged")
fun spinnerValueChangedAdapter(spinner: Spinner, valueChange: InverseBindingListener) {
    spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
        override fun onItemSelected(p: AdapterView<*>, v: View?, pos: Int, id: Long) = valueChange.onChange()
        override fun onNothingSelected(parent: AdapterView<*>?) {}
    }
}

@InverseBindingAdapter(attribute = "value")
fun spinnerValueInverseAdapter(spinner: Spinner): String {
    return (spinner.adapter as SpinnerAdapter).values[spinner.selectedItemPosition]
}

@InverseBindingAdapter(attribute = "value")
fun spinnerValueInverseAdapterInt(spinner: Spinner) = spinnerValueInverseAdapter(spinner).toInt()

