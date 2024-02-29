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

#include "zrlepalettehelper.h"
#include <assert.h>
#include <string.h>

#define ZRLE_HASH(pix) (((pix) ^ ((pix) >> 17)) & 4095)

void zrlePaletteHelperInit(zrlePaletteHelper *helper)
{
  memset(helper->palette, 0, sizeof(helper->palette));
  memset(helper->index, 255, sizeof(helper->index));
  memset(helper->key, 0, sizeof(helper->key));
  helper->size = 0;
}

void zrlePaletteHelperInsert(zrlePaletteHelper *helper, zrle_U32 pix)
{
  if (helper->size < ZRLE_PALETTE_MAX_SIZE) {
    int i = ZRLE_HASH(pix);

    while (helper->index[i] != 255 && helper->key[i] != pix)
      i++;
    if (helper->index[i] != 255) return;

    helper->index[i] = helper->size;
    helper->key[i] = pix;
    helper->palette[helper->size] = pix;
  }
  helper->size++;
}

int zrlePaletteHelperLookup(zrlePaletteHelper *helper, zrle_U32 pix)
{
  int i = ZRLE_HASH(pix);

  assert(helper->size <= ZRLE_PALETTE_MAX_SIZE);
  
  while (helper->index[i] != 255 && helper->key[i] != pix)
    i++;
  if (helper->index[i] != 255) return helper->index[i];

  return -1;
}
