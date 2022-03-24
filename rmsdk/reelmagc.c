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


#include <i86.h>
#include <dos.h>
#include <stdlib.h>
#include <string.h>


#include "logf.h"
#include "reelmagc.h"


static uint8_t         _fmpdrv_int = 0x00; /* 0x00 = not found */
static rm_driver_cb_t  _driver_cb_func = NULL;
static uint16_t        _driver_cb_mode = 0;


/*
  callback stuff here
*/
void
rm_set_cb(rm_driver_cb_t cbfunc, uint16_t cbmode) {
  _driver_cb_func = cbfunc;
  _driver_cb_mode = cbmode;
}

static void __watcall
rm_driver_callback(unsigned pax, unsigned pdx, unsigned pbx, unsigned pcx) {
  unsigned rv;
  unsigned __far *callargs;
  unsigned __far *prsvregs;
  void     __far *prsvdssi;

  callargs = MK_FP(pdx, pbx);
  prsvregs = MK_FP(pdx, pcx);
  prsvdssi = MK_FP(prsvregs[1], prsvregs[3]);

  logfi("rm_driver_callback(%04Xh, %04Xh, %04Xh, %04Xh)\n",
    (unsigned)callargs[0],
    (unsigned)callargs[1],
    (unsigned)callargs[2],
    (unsigned)callargs[3]
  );
  logf_addindent();
  logf_current_segments();

  /*
  logfi("AX=%04Xh BX=%04Xh CX=%04Xh DX=%04Xh\n",
    prsvregs[7],
    prsvregs[6],
    prsvregs[5],
    prsvregs[4]);

  logfi("DS=%04Xh ES=%04Xh SI=%04Xh DI=%04Xh\n",
    prsvregs[1],
    prsvregs[0],
    prsvregs[3],
    prsvregs[2]);

  _flogf_hexdump("prsvdssi", prsvdssi, 16);
  */

  rv = 0;
  if (_driver_cb_func) {
    switch(_driver_cb_mode) {
    case 0x2000: /* stack parameters */
      rv = (*_driver_cb_func)(callargs[0], callargs[1], callargs[2], callargs[3]);
      break;

    default: /* unknown ? */
      logfi("WARNING: Unknown callback mode: %04Xh. Defaulting to register.\n", _driver_cb_mode);
    case 0: /* register paramters */
      rv = (*_driver_cb_func)(prsvregs[6] >> 8, prsvregs[6] & 0xff, prsvregs[7], prsvregs[4]);
      break;
    }
  }

  prsvregs[7] = rv; /* AX */
  prsvregs[6] = 0x0000; /* BX */
  prsvregs[5] = 0x0000; /* CX */
  prsvregs[4] = 0x0000; /* DX */

  logf_delindent();
}

/* this is SUPER paranoid, but I dont wanna take any chances... after all, this is a test tool... 
   all this bullshit ensures that the state (segments, registers, etc) absolutely does not change 
   from the time we go into "driver_callback()" to the moment we return back to the caller...

   The Watcom compiler does not seem to restore the DS even on a far call...
*/
static void __far *
build_driver_callback_shim(void) {
  uint16_t rv_seg;
  uint16_t rv_off;

  __asm {
    JMP shimgetter
    getra:
      POP BX
      JMP BX
    getaboveshimoffset:
      CALL getra
      RET
    drvcbshim:
      PUSHF
      PUSH BP
      PUSH AX
      PUSH BX
      PUSH CX
      PUSH DX
      PUSH SI
      PUSH DI
      PUSH DS
      PUSH ES
      CALL loaddses /* restore the DS from earlier... also put the same value in ES for good measure */
      MOV  AX, DS  /* for stack unfuckers */
      MOV  BX, SP
      ADD  BX, 24  /* 10 word pushes + return seg + off */
      MOV  CX, SP
      MOV  DX, SS 

      CALL rm_driver_callback

      POP ES
      POP DS
      POP DI
      POP SI
      POP DX
      POP CX
      POP BX
      POP AX
      POP BP
      POPF
      RETF



    getabovedsptr:
      CALL getra
      RET
    dsval:
      DW 0  /* what a fine fucking place to preserve the actual data segment... :-( */
    loaddses:
      PUSH BX
      MOV BX, CS
      MOV DS, BX
      CALL getabovedsptr
      MOV ES, [BX+1]
      MOV DS, [BX+1]
      POP BX
      RET
    stords:
      PUSH BX
      PUSH ES
      MOV BX, CS
      MOV ES, BX
      CALL getabovedsptr
      MOV ES:[BX+1], DS
      POP ES
      POP BX
      RET
    shimgetter:
      CALL stords /* store the DS value in the code segment for retrival later */
      MOV rv_seg, CS
      PUSH BX
      CALL getaboveshimoffset
      ADD BX, 1 /* the RET in getaboveshimoffset is 1 byte of instructions...  this is so hacky! what the hell am i missing in watcom for asm labels !? */
      MOV rv_off, BX
      POP BX
  };

  return MK_FP(rv_seg, rv_off);
}




/*
  ReelMagic driver call stuff here
*/

uint8_t
rm_probe_fmpdrv_int(void) {
  uint16_t intnum;
  char _far * _far *ivt_entry;

  ivt_entry = NULL; /* note: NULL on a far pointer in real mode is the IVT base address */

  for (intnum = 0x80; intnum <= 0xFF; ++intnum) {
    if (ivt_entry[intnum] == NULL) continue;
    if (_fstrncmp(&ivt_entry[intnum][3], "FMPDriver", 9) == 0) {
      return intnum; /* success -- found the ReelMagic driver @ intnum */
    }
  }

  return 0; /* failed -- driver not found */
}

void
set_fmpdrv_int(const uint8_t intnum) {
  _fmpdrv_int = intnum;
}

uint16_t
rm_call_rmdevsys(
  const uint8_t function,
  const uint16_t subfunction,
  const uint16_t param1,
  const uint16_t param2) {

  static union REGS r;

  logfi("ENTER rm_call_rmdevsys(%02Xh, %04Xh, %04Xh, %04Xh)\n",
    (unsigned)function, (unsigned)subfunction, (unsigned)param1, (unsigned)param2);
  logf_addindent();

  r.w.ax = 0x9800 | function;
  r.w.bx = subfunction;
  r.w.cx = param1;
  r.w.dx = param2;

  #ifdef __386__
    int386(0x2F, &r, &r);
  #else
    int86(0x2F, &r, &r);
  #endif

  logf_delindent();
  logfi("EXIT rm_call_rmdevsys(%02Xh, %04Xh, %04Xh, %04Xh) = %04Xh %04Xh %04Xh %04Xh\n",
    (unsigned)function, (unsigned)subfunction, (unsigned)param1, (unsigned)param2,
    (unsigned)r.w.ax, (unsigned)r.w.bx, (unsigned)r.w.cx, (unsigned)r.w.dx);

  return r.w.ax;
}

uint32_t
rm_call_fmpdrv(
  const uint8_t command,
  const uint8_t handle,
  const uint16_t subfunc,
  const uint16_t param1,
  const uint16_t param2) {

  uint32_t rv;
  static union REGS r;

  if (!_fmpdrv_int) logf_abort("FMPDRV.EXE is unreachable!\n");

  logfi("ENTER rm_call_fmpdrv(%02Xh, %02Xh, %04Xh, %04Xh, %04Xh)\n",
    command, handle, subfunc, param1, param2);
  logf_addindent();

  r.w.bx   = command;
  r.w.bx <<= 8;
  r.w.bx  |= handle;
  r.w.cx = subfunc;
  r.w.ax = param1;
  r.w.dx = param2;

  #ifdef __386__
    int386(_fmpdrv_int, &r, &r);
  #else
    int86(_fmpdrv_int, &r, &r);
  #endif

  rv = r.w.dx;
  rv <<= 16;
  rv |= r.w.ax;

  logf_delindent();
  logfi("EXIT rm_call_fmpdrv(%02Xh, %02Xh, %04Xh, %04Xh, %04Xh) = %04X%04Xh (%04Xh)\n",
    command, handle, subfunc, param1, param2, r.w.dx, r.w.ax, r.w.bx);
  return rv;
}




/*
  Driver call abstraction functions here...
*/

uint16_t
rm_get_version(void) {
  return rm_call_rmdevsys(0,1,0,0);
}

void
rm_get_fmpdrv_exe_path(char * const output) {
  output[0] = '\0'; /*defensive */
  rm_call_rmdevsys(3, FP_OFF(output), 0, FP_SEG(output));
}

void
rm_reset(void) {
  rm_call_fmpdrv(0xE, 0, 0, 0, 0);
}

void
rm_register_driver_callback(rm_driver_cb_t cbfunc, uint16_t cbmode) {
  rm_set_cb(cbfunc, cbmode);
  rm_call_fmpdrv_ptr(0xB, 0, cbmode, build_driver_callback_shim());
}

rm_handle_t
rm_open(const void * const filename, uint16_t flags) {
  return rm_call_fmpdrv_ptr(1, 0, flags, filename);
}

void
rm_close(rm_handle_t handle) {
  rm_call_fmpdrv(2, handle, 0, 0, 0);
}

void
rm_play(rm_handle_t handle, uint16_t flags) {
  rm_call_fmpdrv(0x3, handle, flags, 0,0);
}

void rm_pause(rm_handle_t handle) {
  rm_call_fmpdrv(0x4, handle, 0, 0,0);
}

uint16_t
rm_get_stream_types(rm_handle_t handle) {
  return rm_get(handle, 0x202);
}

uint16_t
rm_get_play_status(rm_handle_t handle) {
  return rm_get(handle, 0x204);
}

void
rm_set_zorder(rm_handle_t handle, uint16_t value) {
  rm_set(handle, 0x40E, value, 0);
}

void
rm_set_magic_key(rm_handle_t handle, uint16_t k1, uint16_t k2) {
  rm_set(handle, 0x210, k1, k2);
}

void
rm_waitplay(const rm_handle_t handle, const unsigned sleep_interval) {
  uint16_t status;

  logfi("%s waiting on media handle #%u play to complete.\n", sleep_interval ? "Sleep" : "Busy", handle);
  if (sleep_interval) {
    logfi("Sleep interval is %u\n", sleep_interval);
  }
  else {
    /* this gets WAY TOO noisy! shut off all logging while busy waiting */
    logf_setmute(1);
  }

  /* poll this thing... */
  if (sleep_interval)
    while (RMSTATUS_ISPLAYING(status = rm_get_play_status(handle)))
      sleep(sleep_interval);
  else
    while (RMSTATUS_ISPLAYING(status = rm_get_play_status(handle)));

  /* restore things */
  logf_setmute(0);

  logfi("Done waiting on media handle #%u Last driver call returned: %04Xh\n", handle, (unsigned)status);
}

int
rm_init(void) {
  uint16_t ver;
  char loc[256];

  /* first make sure that RMDEV.SYS or MPGDEV.SYS is loaded */
  if (rm_call_rmdevsys(0,0,0,0) != 0x524D) {
    logfi("ERROR: RMDEV.SYS or MPGDEV.SYS Not Found!\n");
    return 0; /* failed */
  }

  /* check the driver version */
  ver = rm_get_version();
  logfi("RealMagic Driver Version Detected: %u.%u\n", (unsigned)(ver>>8), (unsigned)(ver&0xFF));

  /* check the TSR location */
  rm_get_fmpdrv_exe_path(loc);
  logfi("RealMagic Driver is Located at: %sFMPDRV.EXE\n", loc);

  /* probe the interrupt vector table for FMPDRV.EXE residence */
  set_fmpdrv_int(rm_probe_fmpdrv_int());
  if (!_fmpdrv_int) {
    logfi("ERROR: FMPDRV.EXE Not Resident!\n");
    return 0; /* failed */
  }
  logfi("FMPDRV.EXE Detected Resident at INT %02Xh\n", (unsigned)_fmpdrv_int);

  return 1; /* success */
}










/*
  other useful misc functions
*/

uint16_t
rm_call_vbios(
  const uint8_t  function,
  const uint8_t  param1,
  const uint16_t param2,
  const uint16_t param3,
  const uint16_t param4) {

  static union REGS r; /* note: static as int86() does not accept a far REGS struct! */

  logfi("ENTER rm_call_vbios(%02Xh, %02Xh, %04Xh, %04Xh, %04Xh)\n",
    (unsigned)function, (unsigned)param1, (unsigned)param2, (unsigned)param3, (unsigned)param4);
  logf_addindent();

  r.w.ax = (function<<8) | param1;
  r.w.bx = param2;
  r.w.cx = param3;
  r.w.dx = param4;

  #ifdef __386__
    int386(0x10, &r, &r);
  #else
    int86(0x10, &r, &r);
  #endif

  logf_delindent();
  logfi("EXIT rm_call_vbios(%02Xh, %02Xh, %04Xh, %04Xh, %04Xh) = %04Xh %04Xh %04Xh %04Xh\n",
    (unsigned)function, (unsigned)param1, (unsigned)param2, (unsigned)param3, (unsigned)param4,
    (unsigned)r.w.ax, (unsigned)r.w.bx, (unsigned)r.w.cx, (unsigned)r.w.dx);

  return r.w.ax;
}

