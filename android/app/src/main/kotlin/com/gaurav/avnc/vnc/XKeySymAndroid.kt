/*
 * Copyright (c) 2021  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.vnc

import android.view.KeyEvent

/**
 * Implements mapping between [KeyEvent] key codes & X KeySyms.
 */
object XKeySymAndroid {

    /**
     * Returns X KeySym for given [keyCode].
     * Returns 0 if no mapping is found.
     */
    fun getKeySymForAndroidKeyCode(keyCode: Int): Int {
        if (keyCode >= 0 && keyCode < AndroidKeyCodeToXKeySym.size)
            return AndroidKeyCodeToXKeySym[keyCode]
        else
            return 0
    }

    fun updateKeyMap(keyCode: Int, xKeySym: Int) {
        if (keyCode >= 0 && keyCode < AndroidKeyCodeToXKeySym.size)
            AndroidKeyCodeToXKeySym[keyCode] = xKeySym
    }

    /**
     * Lookup table for X KeySym.
     *
     * Each index represents a keycode from [KeyEvent] and
     * value at that index represents the corresponding X KeySym.
     */
    private val AndroidKeyCodeToXKeySym = intArrayOf(
            0,                                  //  KEYCODE_UNKNOWN = 0
            0,                                  //  KEYCODE_SOFT_LEFT = 1
            0,                                  //  KEYCODE_SOFT_RIGHT = 2
            0,                                  //  KEYCODE_HOME = 3
            0,                                  //  KEYCODE_BACK = 4
            0,                                  //  KEYCODE_CALL = 5
            0,                                  //  KEYCODE_ENDCALL = 6
            XKeySym.XK_0,                       //  KEYCODE_0 = 7
            XKeySym.XK_1,                       //  KEYCODE_1 = 8
            XKeySym.XK_2,                       //  KEYCODE_2 = 9
            XKeySym.XK_3,                       //  KEYCODE_3 = 10
            XKeySym.XK_4,                       //  KEYCODE_4 = 11
            XKeySym.XK_5,                       //  KEYCODE_5 = 12
            XKeySym.XK_6,                       //  KEYCODE_6 = 13
            XKeySym.XK_7,                       //  KEYCODE_7 = 14
            XKeySym.XK_8,                       //  KEYCODE_8 = 15
            XKeySym.XK_9,                       //  KEYCODE_9 = 16
            XKeySym.XK_asterisk,                //  KEYCODE_STAR = 17
            XKeySym.XK_numbersign,              //  KEYCODE_POUND = 18
            XKeySym.XK_Up,                      //  KEYCODE_DPAD_UP = 19
            XKeySym.XK_Down,                    //  KEYCODE_DPAD_DOWN = 20
            XKeySym.XK_Left,                    //  KEYCODE_DPAD_LEFT = 21
            XKeySym.XK_Right,                   //  KEYCODE_DPAD_RIGHT = 22
            0,                                  //  KEYCODE_DPAD_CENTER = 23
            XKeySym.XF86XK_AudioRaiseVolume,    //  KEYCODE_VOLUME_UP = 24
            XKeySym.XF86XK_AudioLowerVolume,    //  KEYCODE_VOLUME_DOWN = 25
            0,                                  //  KEYCODE_POWER = 26
            0,                                  //  KEYCODE_CAMERA = 27
            0,                                  //  KEYCODE_CLEAR = 28
            XKeySym.XK_a,                       //  KEYCODE_A = 29
            XKeySym.XK_b,                       //  KEYCODE_B = 30
            XKeySym.XK_c,                       //  KEYCODE_C = 31
            XKeySym.XK_d,                       //  KEYCODE_D = 32
            XKeySym.XK_e,                       //  KEYCODE_E = 33
            XKeySym.XK_f,                       //  KEYCODE_F = 34
            XKeySym.XK_g,                       //  KEYCODE_G = 35
            XKeySym.XK_h,                       //  KEYCODE_H = 36
            XKeySym.XK_i,                       //  KEYCODE_I = 37
            XKeySym.XK_j,                       //  KEYCODE_J = 38
            XKeySym.XK_k,                       //  KEYCODE_K = 39
            XKeySym.XK_l,                       //  KEYCODE_L = 40
            XKeySym.XK_m,                       //  KEYCODE_M = 41
            XKeySym.XK_n,                       //  KEYCODE_N = 42
            XKeySym.XK_o,                       //  KEYCODE_O = 43
            XKeySym.XK_p,                       //  KEYCODE_P = 44
            XKeySym.XK_q,                       //  KEYCODE_Q = 45
            XKeySym.XK_r,                       //  KEYCODE_R = 46
            XKeySym.XK_s,                       //  KEYCODE_S = 47
            XKeySym.XK_t,                       //  KEYCODE_T = 48
            XKeySym.XK_u,                       //  KEYCODE_U = 49
            XKeySym.XK_v,                       //  KEYCODE_V = 50
            XKeySym.XK_w,                       //  KEYCODE_W = 51
            XKeySym.XK_x,                       //  KEYCODE_X = 52
            XKeySym.XK_y,                       //  KEYCODE_Y = 53
            XKeySym.XK_z,                       //  KEYCODE_Z = 54
            XKeySym.XK_comma,                   //  KEYCODE_COMMA = 55
            XKeySym.XK_period,                  //  KEYCODE_PERIOD = 56
            XKeySym.XK_Alt_L,                   //  KEYCODE_ALT_LEFT = 57
            XKeySym.XK_Alt_R,                   //  KEYCODE_ALT_RIGHT = 58
            XKeySym.XK_Shift_L,                 //  KEYCODE_SHIFT_LEFT = 59
            XKeySym.XK_Shift_R,                 //  KEYCODE_SHIFT_RIGHT = 60
            XKeySym.XK_Tab,                     //  KEYCODE_TAB = 61
            XKeySym.XK_space,                   //  KEYCODE_SPACE = 62
            0,                                  //  KEYCODE_SYM = 63
            0,                                  //  KEYCODE_EXPLORER = 64
            0,                                  //  KEYCODE_ENVELOPE = 65
            XKeySym.XK_Return,                  //  KEYCODE_ENTER = 66
            XKeySym.XK_BackSpace,               //  KEYCODE_DEL = 67
            XKeySym.XK_grave,                   //  KEYCODE_GRAVE = 68
            XKeySym.XK_minus,                   //  KEYCODE_MINUS = 69
            XKeySym.XK_equal,                   //  KEYCODE_EQUALS = 70
            XKeySym.XK_bracketleft,             //  KEYCODE_LEFT_BRACKET = 71
            XKeySym.XK_bracketright,            //  KEYCODE_RIGHT_BRACKET = 72
            XKeySym.XK_backslash,               //  KEYCODE_BACKSLASH = 73
            XKeySym.XK_semicolon,               //  KEYCODE_SEMICOLON = 74
            XKeySym.XK_apostrophe,              //  KEYCODE_APOSTROPHE = 75
            XKeySym.XK_slash,                   //  KEYCODE_SLASH = 76
            XKeySym.XK_at,                      //  KEYCODE_AT = 77
            0,                                  //  KEYCODE_NUM = 78
            0,                                  //  KEYCODE_HEADSETHOOK = 79
            0,                                  //  KEYCODE_FOCUS = 80
            XKeySym.XK_plus,                    //  KEYCODE_PLUS = 81
            XKeySym.XK_Menu,                    //  KEYCODE_MENU = 82
            0,                                  //  KEYCODE_NOTIFICATION = 83
            0,                                  //  KEYCODE_SEARCH = 84
            0,                                  //  KEYCODE_MEDIA_PLAY_PAUSE = 85
            0,                                  //  KEYCODE_MEDIA_STOP = 86
            0,                                  //  KEYCODE_MEDIA_NEXT = 87
            0,                                  //  KEYCODE_MEDIA_PREVIOUS = 88
            0,                                  //  KEYCODE_MEDIA_REWIND = 89
            0,                                  //  KEYCODE_MEDIA_FAST_FORWARD = 90
            0,                                  //  KEYCODE_MUTE = 91
            XKeySym.XK_Page_Up,                 //  KEYCODE_PAGE_UP = 92
            XKeySym.XK_Page_Down,               //  KEYCODE_PAGE_DOWN = 93
            0,                                  //  KEYCODE_PICTSYMBOLS = 94
            0,                                  //  KEYCODE_SWITCH_CHARSET = 95
            0,                                  //  KEYCODE_BUTTON_A = 96
            0,                                  //  KEYCODE_BUTTON_B = 97
            0,                                  //  KEYCODE_BUTTON_C = 98
            0,                                  //  KEYCODE_BUTTON_X = 99
            0,                                  //  KEYCODE_BUTTON_Y = 100
            0,                                  //  KEYCODE_BUTTON_Z = 101
            0,                                  //  KEYCODE_BUTTON_L1 = 102
            0,                                  //  KEYCODE_BUTTON_R1 = 103
            0,                                  //  KEYCODE_BUTTON_L2 = 104
            0,                                  //  KEYCODE_BUTTON_R2 = 105
            0,                                  //  KEYCODE_BUTTON_THUMBL = 106
            0,                                  //  KEYCODE_BUTTON_THUMBR = 107
            0,                                  //  KEYCODE_BUTTON_START = 108
            0,                                  //  KEYCODE_BUTTON_SELECT = 109
            0,                                  //  KEYCODE_BUTTON_MODE = 110
            XKeySym.XK_Escape,                  //  KEYCODE_ESCAPE = 111
            XKeySym.XK_Delete,                  //  KEYCODE_FORWARD_DEL = 112
            XKeySym.XK_Control_L,               //  KEYCODE_CTRL_LEFT = 113
            XKeySym.XK_Control_R,               //  KEYCODE_CTRL_RIGHT = 114
            XKeySym.XK_Caps_Lock,               //  KEYCODE_CAPS_LOCK = 115
            XKeySym.XK_Scroll_Lock,             //  KEYCODE_SCROLL_LOCK = 116
            XKeySym.XK_Super_L,                 //  KEYCODE_META_LEFT = 117
            XKeySym.XK_Super_R,                 //  KEYCODE_META_RIGHT = 118
            0,                                  //  KEYCODE_FUNCTION = 119
            XKeySym.XK_Sys_Req,                 //  KEYCODE_SYSRQ = 120
            XKeySym.XK_Break,                   //  KEYCODE_BREAK = 121
            XKeySym.XK_Home,                    //  KEYCODE_MOVE_HOME = 122
            XKeySym.XK_End,                     //  KEYCODE_MOVE_END = 123
            XKeySym.XK_Insert,                  //  KEYCODE_INSERT = 124
            0,                                  //  KEYCODE_FORWARD = 125
            0,                                  //  KEYCODE_MEDIA_PLAY = 126
            0,                                  //  KEYCODE_MEDIA_PAUSE = 127
            0,                                  //  KEYCODE_MEDIA_CLOSE = 128
            0,                                  //  KEYCODE_MEDIA_EJECT = 129
            0,                                  //  KEYCODE_MEDIA_RECORD = 130
            XKeySym.XK_F1,                      //  KEYCODE_F1 = 131
            XKeySym.XK_F2,                      //  KEYCODE_F2 = 132
            XKeySym.XK_F3,                      //  KEYCODE_F3 = 133
            XKeySym.XK_F4,                      //  KEYCODE_F4 = 134
            XKeySym.XK_F5,                      //  KEYCODE_F5 = 135
            XKeySym.XK_F6,                      //  KEYCODE_F6 = 136
            XKeySym.XK_F7,                      //  KEYCODE_F7 = 137
            XKeySym.XK_F8,                      //  KEYCODE_F8 = 138
            XKeySym.XK_F9,                      //  KEYCODE_F9 = 139
            XKeySym.XK_F10,                     //  KEYCODE_F10 = 140
            XKeySym.XK_F11,                     //  KEYCODE_F11 = 141
            XKeySym.XK_F12,                     //  KEYCODE_F12 = 142
            XKeySym.XK_Num_Lock,                //  KEYCODE_NUM_LOCK = 143
            XKeySym.XK_KP_0,                    //  KEYCODE_NUMPAD_0 = 144
            XKeySym.XK_KP_1,                    //  KEYCODE_NUMPAD_1 = 145
            XKeySym.XK_KP_2,                    //  KEYCODE_NUMPAD_2 = 146
            XKeySym.XK_KP_3,                    //  KEYCODE_NUMPAD_3 = 147
            XKeySym.XK_KP_4,                    //  KEYCODE_NUMPAD_4 = 148
            XKeySym.XK_KP_5,                    //  KEYCODE_NUMPAD_5 = 149
            XKeySym.XK_KP_6,                    //  KEYCODE_NUMPAD_6 = 150
            XKeySym.XK_KP_7,                    //  KEYCODE_NUMPAD_7 = 151
            XKeySym.XK_KP_8,                    //  KEYCODE_NUMPAD_8 = 152
            XKeySym.XK_KP_9,                    //  KEYCODE_NUMPAD_9 = 153
            XKeySym.XK_KP_Divide,               //  KEYCODE_NUMPAD_DIVIDE = 154
            XKeySym.XK_KP_Multiply,             //  KEYCODE_NUMPAD_MULTIPLY = 155
            XKeySym.XK_KP_Subtract,             //  KEYCODE_NUMPAD_SUBTRACT = 156
            XKeySym.XK_KP_Add,                  //  KEYCODE_NUMPAD_ADD = 157
            XKeySym.XK_KP_Decimal,              //  KEYCODE_NUMPAD_DOT = 158
            XKeySym.XK_KP_Separator,            //  KEYCODE_NUMPAD_COMMA = 159
            XKeySym.XK_KP_Enter,                //  KEYCODE_NUMPAD_ENTER = 160
            XKeySym.XK_KP_Equal,                //  KEYCODE_NUMPAD_EQUALS = 161
            0,                                  //  KEYCODE_NUMPAD_LEFT_PAREN = 162
            0,                                  //  KEYCODE_NUMPAD_RIGHT_PAREN = 163
            XKeySym.XF86XK_AudioMute,           //  KEYCODE_VOLUME_MUTE = 164
            0,                                  //  KEYCODE_INFO = 165
            0,                                  //  KEYCODE_CHANNEL_UP = 166
            0,                                  //  KEYCODE_CHANNEL_DOWN = 167
            0,                                  //  KEYCODE_ZOOM_IN = 168
            0,                                  //  KEYCODE_ZOOM_OUT = 169
            0,                                  //  KEYCODE_TV = 170
            0,                                  //  KEYCODE_WINDOW = 171
            0,                                  //  KEYCODE_GUIDE = 172
            0,                                  //  KEYCODE_DVR = 173
            0,                                  //  KEYCODE_BOOKMARK = 174
            0,                                  //  KEYCODE_CAPTIONS = 175
            0,                                  //  KEYCODE_SETTINGS = 176
            0,                                  //  KEYCODE_TV_POWER = 177
            0,                                  //  KEYCODE_TV_INPUT = 178
            0,                                  //  KEYCODE_STB_POWER = 179
            0,                                  //  KEYCODE_STB_INPUT = 180
            0,                                  //  KEYCODE_AVR_POWER = 181
            0,                                  //  KEYCODE_AVR_INPUT = 182
            0,                                  //  KEYCODE_PROG_RED = 183
            0,                                  //  KEYCODE_PROG_GREEN = 184
            0,                                  //  KEYCODE_PROG_YELLOW = 185
            0,                                  //  KEYCODE_PROG_BLUE = 186
            0,                                  //  KEYCODE_APP_SWITCH = 187
            0,                                  //  KEYCODE_BUTTON_1 = 188
            0,                                  //  KEYCODE_BUTTON_2 = 189
            0,                                  //  KEYCODE_BUTTON_3 = 190
            0,                                  //  KEYCODE_BUTTON_4 = 191
            0,                                  //  KEYCODE_BUTTON_5 = 192
            0,                                  //  KEYCODE_BUTTON_6 = 193
            0,                                  //  KEYCODE_BUTTON_7 = 194
            0,                                  //  KEYCODE_BUTTON_8 = 195
            0,                                  //  KEYCODE_BUTTON_9 = 196
            0,                                  //  KEYCODE_BUTTON_10 = 197
            0,                                  //  KEYCODE_BUTTON_11 = 198
            0,                                  //  KEYCODE_BUTTON_12 = 199
            0,                                  //  KEYCODE_BUTTON_13 = 200
            0,                                  //  KEYCODE_BUTTON_14 = 201
            0,                                  //  KEYCODE_BUTTON_15 = 202
            0,                                  //  KEYCODE_BUTTON_16 = 203
            0,                                  //  KEYCODE_LANGUAGE_SWITCH = 204

            /*  We currently have no mapping for rest of the key codes.
                So these are commented to reduce the lookup table size.

            0,                                  //  KEYCODE_MANNER_MODE = 205
            0,                                  //  KEYCODE_3D_MODE = 206
            0,                                  //  KEYCODE_CONTACTS = 207
            0,                                  //  KEYCODE_CALENDAR = 208
            0,                                  //  KEYCODE_MUSIC = 209
            0,                                  //  KEYCODE_CALCULATOR = 210
            0,                                  //  KEYCODE_ZENKAKU_HANKAKU = 211
            0,                                  //  KEYCODE_EISU = 212
            0,                                  //  KEYCODE_MUHENKAN = 213
            0,                                  //  KEYCODE_HENKAN = 214
            0,                                  //  KEYCODE_KATAKANA_HIRAGANA = 215
            0,                                  //  KEYCODE_YEN = 216
            0,                                  //  KEYCODE_RO = 217
            0,                                  //  KEYCODE_KANA = 218
            0,                                  //  KEYCODE_ASSIST = 219
            0,                                  //  KEYCODE_BRIGHTNESS_DOWN = 220
            0,                                  //  KEYCODE_BRIGHTNESS_UP = 221
            0,                                  //  KEYCODE_MEDIA_AUDIO_TRACK = 222
            0,                                  //  KEYCODE_SLEEP = 223
            0,                                  //  KEYCODE_WAKEUP = 224
            0,                                  //  KEYCODE_PAIRING = 225
            0,                                  //  KEYCODE_MEDIA_TOP_MENU = 226
            0,                                  //  KEYCODE_11 = 227
            0,                                  //  KEYCODE_12 = 228
            0,                                  //  KEYCODE_LAST_CHANNEL = 229
            0,                                  //  KEYCODE_TV_DATA_SERVICE = 230
            0,                                  //  KEYCODE_VOICE_ASSIST = 231
            0,                                  //  KEYCODE_TV_RADIO_SERVICE = 232
            0,                                  //  KEYCODE_TV_TELETEXT = 233
            0,                                  //  KEYCODE_TV_NUMBER_ENTRY = 234
            0,                                  //  KEYCODE_TV_TERRESTRIAL_ANALOG = 235
            0,                                  //  KEYCODE_TV_TERRESTRIAL_DIGITAL = 236
            0,                                  //  KEYCODE_TV_SATELLITE = 237
            0,                                  //  KEYCODE_TV_SATELLITE_BS = 238
            0,                                  //  KEYCODE_TV_SATELLITE_CS = 239
            0,                                  //  KEYCODE_TV_SATELLITE_SERVICE = 240
            0,                                  //  KEYCODE_TV_NETWORK = 241
            0,                                  //  KEYCODE_TV_ANTENNA_CABLE = 242
            0,                                  //  KEYCODE_TV_INPUT_HDMI_1 = 243
            0,                                  //  KEYCODE_TV_INPUT_HDMI_2 = 244
            0,                                  //  KEYCODE_TV_INPUT_HDMI_3 = 245
            0,                                  //  KEYCODE_TV_INPUT_HDMI_4 = 246
            0,                                  //  KEYCODE_TV_INPUT_COMPOSITE_1 = 247
            0,                                  //  KEYCODE_TV_INPUT_COMPOSITE_2 = 248
            0,                                  //  KEYCODE_TV_INPUT_COMPONENT_1 = 249
            0,                                  //  KEYCODE_TV_INPUT_COMPONENT_2 = 250
            0,                                  //  KEYCODE_TV_INPUT_VGA_1 = 251
            0,                                  //  KEYCODE_TV_AUDIO_DESCRIPTION = 252
            0,                                  //  KEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP = 253
            0,                                  //  KEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN = 254
            0,                                  //  KEYCODE_TV_ZOOM_MODE = 255
            0,                                  //  KEYCODE_TV_CONTENTS_MENU = 256
            0,                                  //  KEYCODE_TV_MEDIA_CONTEXT_MENU = 257
            0,                                  //  KEYCODE_TV_TIMER_PROGRAMMING = 258
            0,                                  //  KEYCODE_HELP = 259
            0,                                  //  KEYCODE_NAVIGATE_PREVIOUS = 260
            0,                                  //  KEYCODE_NAVIGATE_NEXT = 261
            0,                                  //  KEYCODE_NAVIGATE_IN = 262
            0,                                  //  KEYCODE_NAVIGATE_OUT = 263
            0,                                  //  KEYCODE_STEM_PRIMARY = 264
            0,                                  //  KEYCODE_STEM_1 = 265
            0,                                  //  KEYCODE_STEM_2 = 266
            0,                                  //  KEYCODE_STEM_3 = 267
            0,                                  //  KEYCODE_DPAD_UP_LEFT = 268
            0,                                  //  KEYCODE_DPAD_DOWN_LEFT = 269
            0,                                  //  KEYCODE_DPAD_UP_RIGHT = 270
            0,                                  //  KEYCODE_DPAD_DOWN_RIGHT = 271
            0,                                  //  KEYCODE_MEDIA_SKIP_FORWARD = 272
            0,                                  //  KEYCODE_MEDIA_SKIP_BACKWARD = 273
            0,                                  //  KEYCODE_MEDIA_STEP_FORWARD = 274
            0,                                  //  KEYCODE_MEDIA_STEP_BACKWARD = 275
            0,                                  //  KEYCODE_SOFT_SLEEP = 276
            0,                                  //  KEYCODE_CUT = 277
            0,                                  //  KEYCODE_COPY = 278
            0,                                  //  KEYCODE_PASTE = 279
            0,                                  //  KEYCODE_SYSTEM_NAVIGATION_UP = 280
            0,                                  //  KEYCODE_SYSTEM_NAVIGATION_DOWN = 281
            0,                                  //  KEYCODE_SYSTEM_NAVIGATION_LEFT = 282
            0,                                  //  KEYCODE_SYSTEM_NAVIGATION_RIGHT = 283
            0,                                  //  KEYCODE_ALL_APPS = 284
            0,                                  //  KEYCODE_REFRESH = 285
            0,                                  //  KEYCODE_THUMBS_UP = 286
            0,                                  //  KEYCODE_THUMBS_DOWN = 287
            0                                   //  KEYCODE_PROFILE_SWITCH = 288

             */
    )
}