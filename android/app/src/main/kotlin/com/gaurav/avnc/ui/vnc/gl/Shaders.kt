/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.ui.vnc.gl

/**
 * Shaders used for rendering framebuffer
 */
object Shaders {
    //language=GLSL
    const val VERTEX_SHADER = """
            uniform mat4 u_Projection;
            attribute vec2 a_Position;
            attribute vec2 a_TextureCoordinates;
            varying vec2 v_TextureCoordinates;
            void main()
            {
               v_TextureCoordinates = a_TextureCoordinates;
               gl_Position = u_Projection * vec4(a_Position, 0, 1);
            }"""

    //language=GLSL
    const val FRAGMENT_SHADER = """
            precision mediump float;
            uniform sampler2D u_TextureUnit;
            varying vec2 v_TextureCoordinates;
            void main()
            {
               gl_FragColor = texture2D(u_TextureUnit, v_TextureCoordinates);
            }"""
}