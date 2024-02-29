/*
 * Copyright (c) 2021  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.util

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.util.AttributeSet
import android.util.TypedValue
import androidx.appcompat.widget.AppCompatSpinner
import com.google.android.material.elevation.ElevationOverlayProvider

/**
 * This class extends spinner to handle some quirks and add some utility features.
 */
class SpinnerEx(context: Context, attrs: AttributeSet? = null) : AppCompatSpinner(context, attrs) {

    init {
        setupElevationOverlay()
    }


    /**
     * Popup window of the Spinner does not support elevation overlay
     * which makes it hard to differentiate between popup & rest of the controls in dark theme.
     *
     * So we manually apply the overlay to popup background.
     */
    private fun setupElevationOverlay() {
        // Elevation is hardcoded to 16dp because we don't have access to popup
        val popupElevation = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                                                       16F,
                                                       resources.displayMetrics)

        val overlay = ElevationOverlayProvider(context)
                .compositeOverlayWithThemeSurfaceColorIfNeeded(popupElevation)

        val background = popupBackground
        if (background is GradientDrawable)
            background.setColor(overlay)
        else
            background.setTint(overlay)
    }
}