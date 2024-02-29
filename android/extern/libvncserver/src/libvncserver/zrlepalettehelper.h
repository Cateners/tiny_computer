/*
 * Copyright (C) 2002 RealVNC Ltd.  All Rights Reserved.
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

/*
 * The PaletteHelper class helps us build up the palette from pixel data by
 * storing a reverse index using a simple hash-table
 */

#ifndef __ZRLE_PALETTE_HELPER_H__
#define __ZRLE_PALETTE_HELPER_H__

#include "zrletypes.h"

#define ZRLE_PALETTE_MAX_SIZE 127

typedef struct {
  zrle_U32  palette[ZRLE_PALETTE_MAX_SIZE];
  zrle_U8   index[ZRLE_PALETTE_MAX_SIZE + 4096];
  zrle_U32  key[ZRLE_PALETTE_MAX_SIZE + 4096];
  int       size;
} zrlePaletteHelper;

void zrlePaletteHelperInit  (zrlePaletteHelper *helper);
void zrlePaletteHelperInsert(zrlePaletteHelper *helper,
			     zrle_U32           pix);
int  zrlePaletteHelperLookup(zrlePaletteHelper *helper,
			     zrle_U32           pix);

#endif /* __ZRLE_PALETTE_HELPER_H__ */
