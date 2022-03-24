/*
 *  Copyright (C) 2022 Jon Dennis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdint.h>

#define printfi(...) {printf_indent(_indent); printf(__VA_ARGS__);}

static int _indent = 0;
static void
printf_indent(int count) {
  while (count--) printf("   ");
}

int
main(int argc, char *argv[]) {
  FILE * fp;
  uint16_t buf[65];

  if (argc != 2) {
    printf("Usage: analyze RECORDED.LOG\n");
    return 1;
  }

  fp = fopen(argv[1], "rb");
  if (fp == NULL) {
    printf("Failed to open %s\n", argv[1]);
    return 1;
  }

  while (fread(buf, 5 << 1, 1, fp)) {
    switch(buf[0]) {
    case 0x0000: /* RECORDER.C record_fmpdrv_call_enter() */
      printfi(">> driver_call(%02Xh, %02Xh, %04Xh, %04Xh, %04Xh)\n",
        buf[2] >> 8, buf[2] & 0xFF, buf[3], buf[1], buf[4]);
      ++_indent;
      break;
    case 0x0001: /* RECORDER.C record_fmpdrv_call_enter() */
      if (!fread(&buf[5], (65 - 5) << 1, 1, fp)) break;
      printfi("Filename: %.*s\n", 128, (char*)&buf[1]);
      break;
    case 0xFFFF: /* RECORDER.C record_fmpdrv_call_exit() */
      if (_indent) --_indent;
      printfi("<< driver_call() = %04X%04Xh bx=%04Xh cx=%04Xh\n",
        buf[4], buf[1], buf[2], buf[3]);
      break;

    case 0x2F00: /* RECORDER.C record_rmdevsys_call_enter() */
      printfi(">> RMDEV.SYS ax=%04Xh bx=%04Xh cx=%04Xh dx=%04Xh\n",
        buf[1],buf[2],buf[3],buf[4]);
      ++_indent;
      break;
    case 0x2FFF: /* RECORDER.C record_rmdevsys_call_exit() */
      if (_indent) --_indent;
      printfi("<< RMDEV.SYS ax=%04Xh bx=%04Xh cx=%04Xh dx=%04Xh\n",
        buf[1],buf[2],buf[3],buf[4]);
      break;

    case 0x0B00: /* RECORDER.C record_callback_enter() */
      printfi("<> callback_reg(ax=%04Xh bx=%04Xh cx=%04Xh dx=%04Xh)\n",
        buf[1],buf[2],buf[3],buf[4]);
      break;
    case 0x0B01: /* RECORDER.C record_callback_enter() */
      printfi("   callback_stk(%04Xh, %04Xh, %04Xh, %04Xh)\n",
        buf[1],buf[2],buf[3],buf[4]);
      break;
    case 0x0BFF: /* RECORDER.C record_callback_exit() */
      if (_indent) --_indent;
      printfi("<< callback_reg(ax=%04Xh bx=%04Xh cx=%04Xh dx=%04Xh)\n",
        buf[1],buf[2],buf[3],buf[4]);
      break;
    case 0x0BFE: /* RECORDER.C record_callback_exit() */
      printfi("callback_stk(%04Xh, %04Xh, %04Xh, %04Xh)\n",
        buf[1],buf[2],buf[3],buf[4]);
      break;

    default:
      printfi("UNKNOWN %04Xh ax=%04Xh bx=%04Xh cx=%04Xh dx=%04Xh\n",
        buf[0],buf[1],buf[2],buf[3],buf[4]);
      break;
    }
  }

  fclose(fp);

  return 0;
}
