/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.ui.vnc.gl

import android.opengl.GLES20.GL_COMPILE_STATUS
import android.opengl.GLES20.GL_FRAGMENT_SHADER
import android.opengl.GLES20.GL_LINK_STATUS
import android.opengl.GLES20.GL_VALIDATE_STATUS
import android.opengl.GLES20.GL_VERTEX_SHADER
import android.opengl.GLES20.glAttachShader
import android.opengl.GLES20.glCompileShader
import android.opengl.GLES20.glCreateProgram
import android.opengl.GLES20.glCreateShader
import android.opengl.GLES20.glDeleteProgram
import android.opengl.GLES20.glDeleteShader
import android.opengl.GLES20.glGetProgramInfoLog
import android.opengl.GLES20.glGetProgramiv
import android.opengl.GLES20.glGetShaderInfoLog
import android.opengl.GLES20.glGetShaderiv
import android.opengl.GLES20.glLinkProgram
import android.opengl.GLES20.glShaderSource
import android.opengl.GLES20.glValidateProgram
import android.util.Log

object ShaderCompiler {

    private const val TAG = "ShaderCompiler"

    fun compileShader(type: Int, shaderText: String): Int {
        val shaderObjectId = glCreateShader(type)
        if (shaderObjectId == 0) {
            Log.e(TAG, "Could not create shader object")
            return 0
        }
        glShaderSource(shaderObjectId, shaderText)
        glCompileShader(shaderObjectId)
        Log.d(TAG, glGetShaderInfoLog(shaderObjectId))

        val status = intArrayOf(0)
        glGetShaderiv(shaderObjectId, GL_COMPILE_STATUS, status, 0)
        if (status[0] == 0) {
            Log.e(TAG, "Shader compilation failed")
            glDeleteShader(shaderObjectId)
            return 0
        }

        return shaderObjectId
    }

    fun linkProgram(vertexShaderId: Int, fragmentShaderId: Int): Int {
        val programId = glCreateProgram()
        if (programId == 0) {
            Log.e(TAG, "Could not create program object")
            return 0
        }

        glAttachShader(programId, vertexShaderId)
        glAttachShader(programId, fragmentShaderId)
        glLinkProgram(programId)
        Log.d(TAG, glGetProgramInfoLog(programId))

        val status = intArrayOf(0)
        glGetProgramiv(programId, GL_LINK_STATUS, status, 0)
        if (status[0] == 0) {
            Log.e(TAG, "Program linking failed")
            glDeleteProgram(programId)
            return 0
        }

        return programId
    }

    fun validateProgram(programId: Int): Boolean {
        glValidateProgram(programId)
        val status = intArrayOf(0)
        glGetProgramiv(programId, GL_VALIDATE_STATUS, status, 0)
        Log.d(TAG, "Program [" + programId + "] validation result: " + status[0])
        Log.d(TAG, "Program [" + programId + "] validation log: " + glGetProgramInfoLog(programId))
        return status[0] != 0
    }

    fun buildProgram(vertexShaderSource: String, fragmentShaderSource: String): Int {
        val vertexShaderId = compileShader(GL_VERTEX_SHADER, vertexShaderSource)
        val fragmentShaderId = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource)

        if (vertexShaderId == 0 || fragmentShaderId == 0)
            return 0

        return linkProgram(vertexShaderId, fragmentShaderId)
    }
}