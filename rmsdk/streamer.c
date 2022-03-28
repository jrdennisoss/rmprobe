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
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "logf.h"
#include "reelmagc.h"



unsigned _callback_count = 0;

struct my_buffer {
  size_t     read_size;
  uint32_t   file_position;
  uint8_t   *ptr;
};

struct my_handle_data {
  uint8_t             pad1[64]; // should cover RTZ user data and buffer structs
  rm_handle_t         rm_handle;
  int                 dos_file_handle;

  uint32_t            current_file_position;
  struct my_buffer    buf1;
  struct my_buffer    buf2;
  struct my_buffer   *current_buf;
  struct my_buffer   *next_buf;
};

#define DMA_BUF_SIZE (0x1000)
static uint8_t _dma_buf[DMA_BUF_SIZE * 2]; //8k of goodness like RTZ does...
static struct my_handle_data _hds[1];

#define CHECK_HD {                                                                \
  if (hd == NULL)                {logfi("NULL hd\n");                  break;}    \
  if (hd->rm_handle != handle)   {logfi("hd handle mismatch!\n");      break;}    \
  if (hd->dos_file_handle == -1) {logfi("-1 dos_file_handle!\n");      break;}    \
}

static int
open_dos_file_handle(const char __far * const filename) {
  uint16_t filename_seg;
  uint16_t filename_off;
  int16_t  rv;

  filename_seg = FP_SEG(filename);
  filename_off = FP_OFF(filename);
  rv = -1;

  __asm {
    PUSH AX
    PUSH DX
    PUSH DS
    MOV AH, 0x3D /* DOS open file */
    MOV AL, 0xC0 /* read only, no inherit, deny none */
    MOV DS, filename_seg
    MOV DX, filename_off
    INT 0x21
    JC done
    MOV rv, AX
    done:
    POP DS
    POP DX
    POP AX
  };

  return (int)rv;
}

static void
close_dos_file_handle(const int file_handle) {
  uint16_t fh;

  if (file_handle < 0) return;
  fh = (uint16_t)(unsigned)file_handle;
  __asm {
    PUSH AX
    PUSH BX
    MOV AH, 0x3E /* DOS close file */
    MOV BX, fh
    INT 0x21
    POP BX
    POP AX
  };
}

static void
set_dos_file_handle_position(struct my_handle_data *hd, const uint32_t file_position) {
  uint16_t fh;
  uint16_t fp_low;
  uint16_t fp_high;


  if (hd->dos_file_handle < 0) return;
  fh = (uint16_t)(unsigned)hd->dos_file_handle;

  fp_low  = file_position;
  fp_high = file_position >> 16;

  __asm {
    PUSHA
    MOV AH, 0x42 /* DOS LSEEK File */
    MOV AL, 0 /* file_position is relative to the beginning of the file */
    MOV BX, fh
    MOV CX, fp_high
    MOV DX, fp_low
    INT 0x21
    POPA
  };

  hd->current_file_position = file_position;
}

static void
read_into_buf(struct my_handle_data *hd, struct my_buffer *buf) {
  uint16_t bytes_read;
  uint16_t file_handle;
  uint16_t buf_seg;
  uint16_t buf_off;

  bytes_read = 0;
  if (hd->dos_file_handle < 0)
    goto read_operation_complete;

  file_handle = (uint16_t)(unsigned)hd->dos_file_handle;
  buf_seg = FP_SEG(buf->ptr);
  buf_off = FP_OFF(buf->ptr);

  __asm {
    PUSHA
    PUSH DS
    MOV AH, 0x3F /* DOS read file */
    MOV BX, file_handle
    MOV CX, DMA_BUF_SIZE
    MOV DS, buf_seg
    MOV DX, buf_off
    INT 0x21
    JC done
    MOV bytes_read, AX
    done:
    POP DS
    POPA
  };

read_operation_complete:
  buf->read_size = bytes_read;
  buf->file_position = hd->current_file_position;
  hd->current_file_position += buf->read_size;
  logfi("bytes read: %u\n", (unsigned) buf->read_size);
  logf_hexdump("post read", buf->ptr, 10);
}

static void
beep_me() {
  __asm {
    PUSH AX
    PUSH DX
    MOV AH, 2
    MOV DL, '!'
    INT 0x21
    MOV AH, 2
    MOV DL, 0x0D
    INT 0x21
    MOV AH, 2
    MOV DL, 0x0A
    INT 0x21
    MOV AH, 2
    MOV DL, 7
    INT 0x21
    POP DX
    POP AX
  };
}

static int _is_playing = 0;
static int _dot_count = 0;
static unsigned
my_callback(unsigned command, unsigned handle, unsigned param1, unsigned param2) {
  struct my_handle_data *hd;
  struct my_buffer *current_buf;
  struct my_buffer *other_buf;
  uint32_t requested_position;
  unsigned rv;

  ++_callback_count;
if (_is_playing) {
  if (_dot_count++ >= 10) {
    _dot_count = 0;
    __asm {
      PUSH AX
      PUSH DX
      MOV AH, 2
      MOV DL, 0x0D
      INT 0x21
      MOV AH, 2
      MOV DL, 0x0A
      INT 0x21
      POP DX
      POP AX
    };
  }
  __asm {
    PUSH AX
    PUSH DX
    MOV AH, 2
    MOV DL, '.'
    INT 0x21
    POP DX
    POP AX
  };
}

  logfi("CBCOUNT: %u\n", _callback_count);
  logfi("ENTER my_callback(%04Xh, %04Xh, %04Xh, %04Xh)\n", command, handle, param1, param2);

  hd = (void*)rm_get_ptr(handle, 0x208);
  rv = 0;

  switch (command) {
  case 4: /* opening new media handle */
    logfi("Opening Handle\n");
    if (handle != 1) {logfi("handle != 1\n"); break;}

    /* init the my_handle_data */
    hd = &_hds[0];
    memset(hd, 0, sizeof(*hd)); /* defensive */
    hd->rm_handle = handle;
    hd->buf1.ptr = &_dma_buf[DMA_BUF_SIZE * 0];
    hd->buf2.ptr = &_dma_buf[DMA_BUF_SIZE * 1];
    hd->current_buf = &hd->buf2;
    hd->next_buf = &hd->buf1;

    /* open the file... */
    hd->dos_file_handle = open_dos_file_handle(MK_FP(param2, param1));
    if (hd->dos_file_handle == -1) {
      logfi("ERROR: Failed to open file: _filename\n");
      break;
    }

    /* tell the caller we are good (i think) */
    rm_set_ptr(handle, 0x208, hd);
    rm_set(handle, 0x302, 1, 0);
    break;

  case 3: /* ??? */
    logfi("Unknown 3\n");
    CHECK_HD;
    break;
  case 1: /* ??? */
    logfi("Unknown 1 -- Filling Buffer(s)\n");
    CHECK_HD;
    if (hd->current_buf->read_size < 1) {
      logfi("Current buffer is empty... Filling it...\n");
      read_into_buf(hd, hd->current_buf);
    }
    else if (hd->next_buf->read_size < 1) {
      logfi("Next buffer is empty... Filling it...\n");
      read_into_buf(hd, hd->next_buf);
    }
    break;
  case 2: /* ??? */
    logfi("Unknown 2 -- Reporting Buffer\n");
    CHECK_HD;

    requested_position   = param2;
    requested_position <<= 16;
    requested_position  |= param1;



    if (requested_position != hd->current_buf->file_position) {
      hd->current_buf->read_size = 0; /* dispose of the current buffer */
      if ((requested_position == hd->next_buf->file_position) && hd->next_buf->read_size) {
        /* next buffer matches the requested criteria...
           rotate the current/next buffers... */
        struct my_buffer *swp;
        swp = hd->current_buf;
        hd->current_buf = hd->next_buf;
        hd->next_buf = swp;
      }
      else {
        /* got a totally unexpected file position... need to reset things... */
        hd->next_buf->read_size = 0;
        set_dos_file_handle_position(hd, requested_position);
        read_into_buf(hd, hd->current_buf);
      }
    }

    if (hd->current_buf->read_size > 0) {
//if (_is_playing)beep_me();
      rm_submit_buffer(handle, hd->current_buf->ptr, hd->current_buf->read_size);
    }
    break;
  case 5: /* closing handle */
    logfi("Closing Handle\n");
    CHECK_HD;
    close_dos_file_handle(hd->dos_file_handle);
    memset(hd, 0, sizeof(*hd)); /* defensive */
    hd->dos_file_handle = -1; /*  defensive */
    rm_set_ptr(handle, 0x208, NULL);
    break;
  }

  logfi("EXIT my_callback(%04Xh, %04Xh, %04Xh, %04Xh)\n", command, handle, param1, param2);
  return rv;
}

static void
run_test(void) {
  uint8_t handle;
  uint16_t stream_types;
  const char * filename;
  unsigned i,j;

  //filename = "C:\\DEV\\FMPVSS00.MPG";
  //filename = "C:\\DEV\\FINTRO01.MPG";
  filename = "C:\\DEV\\SIGLOGO.MPG";

  /* reset and init rm global stuff */
  rm_reset();
  rm_register_driver_callback(&my_callback, RMCBMODE_STACK);
  //rm_register_driver_callback(&my_callback, RMCBMODE_REGISTER);
  //rm_set_magic_key(RM_GLOBAL, 0x7088, 0xC39D);
  rm_set_magic_key(RM_GLOBAL, 0x4041, 0x4004);
  //rm_call_rmdevsys(0x981E, 0x0005, 0x0000, 0x0000); // ???

  /* open our handle */
  handle = rm_open(filename, RMOPEN_STREAM);
  if (handle == RM_HANDLE_INVALID) logf_abort("Open failed!\n");
  stream_types = rm_get_stream_types(handle);
  if (!RMSTREAMTYPES_OK(stream_types)) {
    logf_abort("Error reading file: %s\n", filename);
  }
  logf("Streams Detected:%s%s\n",
    RMSTREAMTYPES_HASVIDEO(stream_types) ?  " video" : "",
    RMSTREAMTYPES_HASAUDIO(stream_types) ?  " audio" : "");
  rm_set_zorder(handle, RMZORDER_BEHINDVGA);
//  rm_get_play_status(handle);
  rm_get(handle, 0x208);
logf_setmute(1);

  /* play the handle */
_is_playing = 1;
  rm_play(handle, RMPLAY_PAUSE_ON_FINISH);

  while (RMSTATUS_ISPLAYING(rm_get_play_status(handle)));

  //sleep(15);

#if 0
  /* poll status do something... */
  while (RMSTATUS_ISPLAYING(rm_get_play_status(handle))) {
    sleep(1);
  }
#endif

  /* close the handle */
  rm_close(handle);
logf_setmute(0);
logfi("CBCOUNT: %u\n", _callback_count);
//logfi("LAST LRP: %u\n", (unsigned)_last_lrp);
}


int
main(int argc, char *argv[]) {
  logf_openlogfile("out.log");
  if (!rm_init()) logf_abort("Failed to initialize ReelMagic");
  logf_current_segments();
  logf_ivt_registered();
  run_test();
  logf_closelogfile();
  return 0;
}


