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


#include "logf.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>

static FILE * _logout = NULL;
static int _stdouton = 1;
static int _muted = 0;

void
logf(const char * const fmt, ...) {
  va_list vl;
  int i;

  if (_muted) return;

  if (_stdouton) {
    va_start(vl, fmt);
    vfprintf(stdout, fmt, vl);
    va_end(vl);
    fflush(stdout);
  }

  if (_logout != NULL) {
    va_start(vl, fmt);
    vfprintf(_logout, fmt, vl);
    va_end(vl);
    fflush(_logout);
  }

}

void
logf_flushabort(void) {
  logfi("Aborting!\n");
  logf_closelogfile();
  abort();
}

void
logf_setstdouton(const int val) {
  _stdouton = val;
}

void
logf_setmute(const int val) {
  _muted = val;
}

void
logf_openlogfile(const char * filename) {
  logf_closelogfile(); /* defensive */

  _logout = fopen(filename, "w");
  if (_logout == NULL) {
    logf_abort("Failed to open log file: %s\n", filename);
  }
  logfi("Logfile '%s' opened for writing...\n", filename);
}

void
logf_closelogfile(void) {
  if (!_logout) return;
  fclose(_logout);
  _logout = NULL;
}



/* indent stuff */
static int _indent_count = 0;
void
logf_printindent(void) {
  int i;
  for (i = 0; i < _indent_count; ++i)
    logf("  ");
}

void
logf_addindent(void) {
  ++_indent_count;
}

void
logf_delindent(void) {
  if (_indent_count)
    --_indent_count;
}

void
logf_hexdump(const char * const name, const void * const vptr, unsigned size) {
  const uint8_t *ptr;
  ptr = vptr;
  logfi("%s:", name);
  for (; size--; ++ptr) {
    logf(" %02X", (unsigned)*ptr);
  }
  logf("\n");
}

void
_flogf_hexdump(const char * const name, const void __far * const vptr, unsigned size) {
  const uint8_t __far *ptr;
  ptr = vptr;
  logfi("%s:", name);
  for (; size--; ++ptr) {
    logf(" %02X", (unsigned)*ptr);
  }
  logf("\n");
}

void
logf_current_segments(void) {
  uint16_t code_seg;
  uint16_t data_seg;
  uint16_t extra_seg;
  uint16_t stack_seg;
  __asm {
    MOV code_seg,  CS;
    MOV data_seg,  DS;
    MOV extra_seg, ES;
    MOV stack_seg, SS;
  };
  logfi("CS=%04Xh DS=%04Xh ES=%04Xh SS=%04Xh\n",
    (unsigned)code_seg, (unsigned)data_seg, (unsigned)extra_seg, (unsigned)stack_seg);
}

#include <i86.h>
void
logf_ivt_registered(void) {
  uint16_t intnum;
  void _far * _far *ivt_entry;

  ivt_entry = NULL;

  logfi("---Registered Interrupts---\n")
  logfi("INT    HANDLER\n");
  for (intnum = 0x00; intnum <= 0xFF; ++intnum) {
    //if (ivt_entry[intnum] == NULL) continue;
    logfi("%02Xh    [%04X:%04X]\n", intnum, FP_SEG(ivt_entry[intnum]), FP_OFF(ivt_entry[intnum]));
  }
  logfi("\n");
}

