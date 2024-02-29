/*
 * Copyright (c) 2022  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

#ifndef AVNC_CURSOR_H
#define AVNC_CURSOR_H


/******************************************************************************
 * Some servers (e.g TigerVNC) may not send the cursor immediately after
 * connection. To provide consistent experience to users, we use a default
 * cursor as fallback.
 *****************************************************************************/

const uint16_t DefaultCursorWidth = 10;
const uint16_t DefaultCursorHeight = 16;
const uint16_t DefaultCursorXHot = 1;
const uint16_t DefaultCursorYHot = 1;

const uint32_t DefaultCursorBuffer[DefaultCursorWidth * DefaultCursorHeight]
        = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x00FFFFFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x00FFFFFF, 0x00FFFFFF, 0, 0, 0, 0,
           0, 0, 0, 0, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0, 0, 0, 0, 0, 0, 0, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF,
           0x00FFFFFF, 0, 0, 0, 0, 0, 0, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0, 0, 0, 0, 0,
           0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0, 0, 0, 0, 0x00FFFFFF, 0x00FFFFFF,
           0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0, 0, 0, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF,
           0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0, 0, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF,
           0x00FFFFFF, 0x00FFFFFF, 0, 0, 0, 0, 0, 0x00FFFFFF, 0x00FFFFFF, 0, 0x00FFFFFF, 0x00FFFFFF, 0, 0, 0, 0, 0,
           0x00FFFFFF, 0, 0, 0, 0x00FFFFFF, 0x00FFFFFF, 0, 0, 0, 0, 0, 0, 0, 0, 0x00FFFFFF, 0x00FFFFFF, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0x00FFFFFF, 0x00FFFFFF, 0, 0, 0, 0, 0, 0, 0, 0, 0x00FFFFFF, 0x00FFFFFF, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0};

const uint8_t DefaultCursorMask[DefaultCursorWidth * DefaultCursorHeight]
        = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0,
           0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1,
           1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
           0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0,
           0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0};


/******************************************************************************
 * Cursor management
 *****************************************************************************/

/**
 * Wrapper for cursor information.
 *
 * rfbClient struct does not maintain all cursor related information inside it.
 * Things like xHot, yHot are passed only via the cursor shape callback.
 * This wrapper holds all information necessary to render the cursor.
 */
struct Cursor {
    uint8_t *buffer;
    uint8_t *mask;
    uint8_t *scratchBuffer; //Used during rendering
    uint16_t width;
    uint16_t height;
    uint16_t xHot;
    uint16_t yHot;
};

//Only 4-byte pixels are currently supported
const uint8_t PixelBytes = 4;

/**
 * Creates a new CursorData, initialized with default cursor info.
 */
Cursor *newCursor() {
    auto cursor = (Cursor *) malloc(sizeof(Cursor));
    if (cursor) {
        cursor->buffer = (uint8_t *) DefaultCursorBuffer;
        cursor->mask = (uint8_t *) DefaultCursorMask;
        cursor->scratchBuffer = (uint8_t *) malloc(DefaultCursorWidth * DefaultCursorHeight * PixelBytes);
        cursor->width = DefaultCursorWidth;
        cursor->height = DefaultCursorHeight;
        cursor->xHot = DefaultCursorXHot;
        cursor->yHot = DefaultCursorYHot;
    }
    return cursor;
}

void freeCursorBuffers(Cursor *cursor) {
    if (cursor) {
        free(cursor->scratchBuffer);
        if (cursor->buffer != (uint8_t *) DefaultCursorBuffer) free(cursor->buffer);
        if (cursor->mask != (uint8_t *) DefaultCursorMask) free(cursor->mask);
    }
}

void freeCursor(Cursor *cursor) {
    freeCursorBuffers(cursor);
    free(cursor);
}

void updateCursor(Cursor *cursor, uint8_t *buffer, uint8_t *mask, uint16_t width, uint16_t height,
                  uint16_t xHot, uint16_t yHot) {

    freeCursorBuffers(cursor);
    cursor->buffer = buffer;
    cursor->mask = mask;
    cursor->scratchBuffer = (uint8_t *) malloc(width * height * PixelBytes);
    cursor->width = width;
    cursor->height = height;
    cursor->xHot = xHot;
    cursor->yHot = yHot;
}

#endif //AVNC_CURSOR_H