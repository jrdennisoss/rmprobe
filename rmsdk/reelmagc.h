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

#ifndef REELMAGC_H_INCLUDED
#define REELMAGC_H_INCLUDED

#include <i86.h>
#include <stdint.h>

typedef unsigned (*rm_driver_cb_t)(unsigned /* command */, unsigned /* handle */, unsigned /* param1 */, unsigned /* param2 */);
typedef uint8_t rm_handle_t;
#define RM_HANDLE_INVALID (0)
#define RM_GLOBAL         (0)


/* ReelMagic "Direct-Drive" Functions */
uint8_t rm_probe_fmpdrv_int(void);
void rm_set_fmpdrv_int(const uint8_t intnum);
uint16_t rm_call_rmdevsys(const uint8_t function, const uint16_t subfunction, const uint16_t param1, const uint16_t param2);
uint32_t rm_call_fmpdrv(const uint8_t command,const rm_handle_t handle,const uint16_t subfunc,const uint16_t param1,const uint16_t param2);

static inline uint32_t
rm_call_fmpdrv_ptr(
  const uint8_t command,
  const rm_handle_t handle,
  const uint16_t subfunc,
  const void __far * const ptr) {
  return rm_call_fmpdrv(command, handle, subfunc,
    FP_OFF(ptr), FP_SEG(ptr));
}

static inline void
rm_set(const rm_handle_t handle, const uint16_t type, const uint16_t param1, const uint16_t param2) {
  rm_call_fmpdrv(0x9, handle, type, param1, param2);
}
static inline void
rm_set_ptr(const rm_handle_t handle, const uint16_t type, void __far * const ptr) {
  rm_call_fmpdrv_ptr(0x9, handle, type, ptr);
}

static inline uint32_t
rm_get(const rm_handle_t handle, const uint16_t type) {
  return rm_call_fmpdrv(0xA, handle, type, 0, 0);
}
static inline void __far *
rm_get_ptr(const rm_handle_t handle, const uint16_t type) {
  const uint32_t rv = rm_get(handle, type);
  return MK_FP(rv >> 16, rv & 0xffff);
}


/* ReelMagic Driver Abstraction Functions */
uint16_t rm_get_version(void);
void rm_get_fmpdrv_exe_path(char * const output);

void rm_reset(void);
void rm_register_driver_callback(rm_driver_cb_t cbfunc, uint16_t cbmode);
#define RMCBMODE_REGISTER (0)
#define RMCBMODE_STACK    (0x2000)

rm_handle_t rm_open(const void __far * const filename, uint16_t flags);
#define RMOPEN_FILE          (0x0001)
#define RMOPEN_STREAM        (0x0002)
#define RMOPEN_ALWAYSSUCCEED (0x0100)
#define RMOPEN_STRLENPREFIX  (0x1000)

void rm_close(rm_handle_t handle);

void rm_play(rm_handle_t handle, uint16_t flags);
#define RMPLAY_STOP_ON_FINISH (0)
#define RMPLAY_PAUSE_ON_FINISH (1)
#define RMPLAY_PLAY_IN_LOOP (4)

void rm_pause(rm_handle_t handle);

uint16_t rm_get_stream_types(rm_handle_t handle);
#define RMSTREAMTYPES_OK(STREAMTYPES) (STREAMTYPES)
#define RMSTREAMTYPES_HASAUDIO(STREAMTYPES) (STREAMTYPES & 1)
#define RMSTREAMTYPES_HASVIDEO(STREAMTYPES) (STREAMTYPES & 2)

uint16_t rm_get_play_status(rm_handle_t handle);
#define RMSTATUS_ISPLAYING(STATUS) ((STATUS & 3) == 0)
#define RMSTATUS_ISPAUSED(STATUS)   (STATUS & 1)
#define RMSTATUS_ISSTOPPED(STATUS)  (STATUS & 2)

uint32_t rm_get_bytes_decoded(rm_handle_t handle);

void rm_set_zorder(rm_handle_t handle, uint16_t value);
#define RMZORDER_ABOVEVGA  (2)
#define RMZORDER_BEHINDVGA (4)

void rm_set_magic_key(rm_handle_t handle, uint16_t k1, uint16_t k2);

void rm_waitplay(const rm_handle_t handle, const unsigned sleep_interval);

void rm_submit_buffer(rm_handle_t handle, const void __far * const buf, const unsigned size);





int rm_init(void); /* 0 = failed; non-zero = success */





/* Other Non-ReelMagic Functions */
uint16_t rm_call_vbios(const uint8_t function, const uint8_t param1, const uint16_t param2, const uint16_t param3, const uint16_t param4);


#endif /* #ifndef REELMAGC_H_INCLUDED */
