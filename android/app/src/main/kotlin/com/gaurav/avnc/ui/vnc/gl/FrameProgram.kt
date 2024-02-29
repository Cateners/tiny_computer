/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.ui.vnc.gl

import android.opengl.GLES20.GL_CLAMP_TO_EDGE
import android.opengl.GLES20.GL_LINEAR
import android.opengl.GLES20.GL_TEXTURE0
import android.opengl.GLES20.GL_TEXTURE_2D
import android.opengl.GLES20.GL_TEXTURE_MAG_FILTER
import android.opengl.GLES20.GL_TEXTURE_MIN_FILTER
import android.opengl.GLES20.GL_TEXTURE_WRAP_S
import android.opengl.GLES20.GL_TEXTURE_WRAP_T
import android.opengl.GLES20.glActiveTexture
import android.opengl.GLES20.glBindTexture
import android.opengl.GLES20.glGenTextures
import android.opengl.GLES20.glGetAttribLocation
import android.opengl.GLES20.glGetUniformLocation
import android.opengl.GLES20.glTexParameteri
import android.opengl.GLES20.glUniform1i
import android.opengl.GLES20.glUniformMatrix4fv
import android.opengl.GLES20.glUseProgram
import android.util.Log
import com.example.tiny_computer.BuildConfig

/**
 * Represents the GL program used for Frame rendering.
 *
 * NOTE: It must be instantiated in an OpenGL context.
 */
class FrameProgram {

    companion object {
        // Attribute constants
        const val A_POSITION = "a_Position"
        const val A_TEXTURE_COORDINATES = "a_TextureCoordinates"

        // Uniform constants
        const val U_PROJECTION = "u_Projection"
        const val U_TEXTURE_UNIT = "u_TextureUnit"
    }

    val program = ShaderCompiler.buildProgram(Shaders.VERTEX_SHADER, Shaders.FRAGMENT_SHADER)
    val aPositionLocation = glGetAttribLocation(program, A_POSITION)
    val aTextureCoordinatesLocation = glGetAttribLocation(program, A_TEXTURE_COORDINATES)
    val uProjectionLocation = glGetUniformLocation(program, U_PROJECTION)
    val uTexUnitLocation = glGetUniformLocation(program, U_TEXTURE_UNIT)
    val textureId = createTexture()
    var validated = false


    fun setUniforms(projectionMatrix: FloatArray) {
        glUniformMatrix4fv(uProjectionLocation, 1, false, projectionMatrix, 0)
        glActiveTexture(GL_TEXTURE0)
        glBindTexture(GL_TEXTURE_2D, textureId)
        glUniform1i(uTexUnitLocation, 0)
    }

    private fun createTexture(): Int {
        val texturesObjects = intArrayOf(0)
        glGenTextures(1, texturesObjects, 0)
        if (texturesObjects[0] == 0) {
            Log.e("Texture", "Could not generate texture.")
            return 0
        }

        glBindTexture(GL_TEXTURE_2D, texturesObjects[0])
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)
        glBindTexture(GL_TEXTURE_2D, 0)
        return texturesObjects[0]
    }

    fun validate() {
        if (BuildConfig.DEBUG && !validated) {
            ShaderCompiler.validateProgram(program)
            validated = true
        }
    }

    fun useProgram() {
        glUseProgram(program)
    }
}