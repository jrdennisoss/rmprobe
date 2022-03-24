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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <share.h>
#include <dos.h>

#define EXE_SIZE (12 * 1024)

static char              _filename[64];
static unsigned          _open_fail_counter = 0;
static int               _install_cb_recorder_on_next_exit = 0;
static unsigned          _actual_cb_type;
static void __far *      _actual_cb;
static void __far *      _actual_fmpdrv;
static unsigned          _entry_buf[65];
static unsigned __far *  _callargs; /* putting here to keep in the DS and avoid screwing around with making stack too big */
static unsigned __far *  _prsvregs; /* putting here to keep in the DS and avoid screwing around with making stack too big */

#if 0
static void __far *      _cbret_stack[16];
static unsigned          _cbret_sp = (unsigned)(_cbret_stack + 16);
#endif

static int
open_log(void) {
  static int rv; /* static because compiler thinks ss and ds is the same ! */
  if (_dos_open(_filename, O_WRONLY | O_NOINHERIT | SH_DENYNO, &rv) != 0) {
    ++_open_fail_counter;
    return -1;
  }

  __asm { /* seek to end of file so we append */
    PUSHF
    MOV AH, 0x42   /* seek */
    MOV AL, 2      /* position at end */
    MOV BX, rv     /* handle */
    MOV CX, 0      /* position offset */
    MOV DX, 0      /* position offset */
    INT 0x21
    POPF
  };
  return rv;
}

#define set_entry_buf(TAG) set_entry_buf_regs(pax, pdx, pbx, pcx); _entry_buf[0] = TAG

static void __watcall
set_entry_buf_regs(unsigned pax, unsigned pdx, unsigned pbx, unsigned pcx) {
   _entry_buf[1] = pax; _entry_buf[2] = pbx; _entry_buf[3] = pcx; _entry_buf[4] = pdx;
}

static void __watcall
write_entry_buf_to_log(unsigned words) {
  int fd;
  static unsigned written;
  fd = open_log();
  if (fd == -1) return;
  _dos_write(fd, _entry_buf, words << 1, &written);
  _dos_close(fd);
}


static void __watcall
record_fmpdrv_call_enter(unsigned pax, unsigned pdx, unsigned pbx, unsigned pcx) {
  set_entry_buf(0x0000);
  write_entry_buf_to_log(5);
  switch (pbx>>8) {
  case 0x01:
    _entry_buf[0] = 0x0001;
    _fmemcpy(&_entry_buf[1], MK_FP(pdx, pax), 128);
    write_entry_buf_to_log(65); /* 65 words = 130 bytes */
    break;
  case 0x0B:
    _install_cb_recorder_on_next_exit = 1;
    _actual_cb = MK_FP(pdx, pax);
    _actual_cb_type = pcx;
    break;
  }
}
static void __watcall record_callback_enter(unsigned pax, unsigned pdx, unsigned pbx, unsigned pcx);
static void __watcall record_callback_exit(unsigned pax, unsigned pdx, unsigned pbx, unsigned pcx);
static void __watcall
record_fmpdrv_call_exit(unsigned pax, unsigned pdx, unsigned pbx, unsigned pcx) {
  set_entry_buf(0xFFFF);
  write_entry_buf_to_log(5);
  if (_install_cb_recorder_on_next_exit) {
    _install_cb_recorder_on_next_exit = 0;
    __asm {
      JMP setfmpdrvcb

      getra:
        POP AX
        JMP AX
      getabovecbshimoffset:
        CALL getra
        RET
      cbshim: /* this is the label of the actual call we give the driver for callback */

        /* record the entry */
        PUSH DS
        PUSH ES
        PUSH AX
        PUSH BX
        PUSH CX
        PUSH DX
        MOV AX, CS
        MOV DS, AX
        MOV ES, AX
        MOV AX, SS
        MOV BX, SP
        ADD BX, 16 /* 6 word pushes + return seg + off */
        MOV CX, SP
        CALL record_callback_enter
        POP DX
        POP CX
        POP BX
        POP AX
        POP ES
        POP DS

        /* Need to keep SP as-is for this call because some of these 
           calls use the stack for variable passing...
           So for now, not capturing the exit...
           we need modify the return address to come back here....
        */
        JMP forwardcb

#if 0
        /* Need to keep SP as-is for this call because some of these 
           calls use the stack for variable passing...

           First, preserve the return address onto our auxiliary
           callback return stack ("_cbret_stack")
        */
        ENTER 0, 0
        PUSH DS
        PUSH EAX
        PUSH BX
        MOV AX, CS
        MOV DS, AX
        SUB _cbret_sp, 4
        MOV EAX, [bp+2]
        MOV BX, WORD PTR [_cbret_sp]
        MOV DWORD PTR [BX], EAX
        POP BX
        POP EAX
        POP DS
        LEAVE

        /* now that we have captured the real return address,
           smash it to come back here... 
           Then forward the call to its final destination...
        */
        ADD SP, 4
        PUSH CS
        CALL forwardcb

        /* we are coming back from the final destination...
           now we have to unfuck all the shit we just did
           before we forwarded the call...
        */
        SUB SP, 4
        ENTER 0, 0
        PUSH DS
        PUSH EAX
        PUSH BX
        MOV AX, CS
        MOV DS, AX
        MOV BX, WORD PTR [_cbret_sp]
        MOV EAX, DWORD PTR [BX]
        MOV DWORD PTR [bp+2], EAX
        ADD _cbret_sp, 4
        POP BX
        POP EAX
        POP DS
        LEAVE

        /* hopefully we done things right! */
        RETF
#endif




      getaboveforwardcboffset:
        CALL getra
        RET
      forwardcb:
        DB 0xEA   /* jump far... next four bytes are FAR PTR... */
          DW 0x0000
          DW 0x0000

      setfmpdrvcb:
        PUSHA
        PUSH DS
        PUSH ES

        PUSH DS
        MOV AX, CS
        MOV DS, AX
        CALL getaboveforwardcboffset
        MOV BX, AX
        POP DS

        MOV EAX, _actual_cb
        SHLD EDX, EAX, 0x10
        MOV [BX+2], AX
        MOV [BX+4], DX

        MOV BX, 0x0B00
        MOV CX, _actual_cb_type;
        CALL getabovecbshimoffset
        ADD AX, 1 /* the RET in getabovecbshimoffset is 1 byte of instructions...  this is so hacky! what the hell am i missing in watcom for asm labels !? */
        MOV DX, CS
        PUSHF /* note: IRET will POPF */
        CALL DWORD PTR _actual_fmpdrv

        POP ES
        POP DS
        POPA
    };
  }
}

static void __watcall
record_rmdevsys_call_enter(unsigned pax, unsigned pdx, unsigned pbx, unsigned pcx) {
  set_entry_buf(0x2F00);
  write_entry_buf_to_log(5);
}
static void __watcall
record_rmdevsys_call_exit(unsigned pax, unsigned pdx, unsigned pbx, unsigned pcx) {
  set_entry_buf(0x2FFF);
  write_entry_buf_to_log(5);
}

#define MARSHALL_ARGS() _callargs = MK_FP(pax, pbx); _prsvregs = MK_FP(pax, pcx)
#define PRSV_AX (_prsvregs[3])
#define PRSV_BX (_prsvregs[2])
#define PRSV_CX (_prsvregs[1])
#define PRSV_DX (_prsvregs[0])
static void __watcall
record_callback_enter(unsigned pax, unsigned pdx, unsigned pbx, unsigned pcx) {
  MARSHALL_ARGS();

  _entry_buf[0] = 0x0B00;
  _entry_buf[1] = PRSV_AX; _entry_buf[2] = PRSV_BX; _entry_buf[3] = PRSV_CX; _entry_buf[4] = PRSV_DX;
  write_entry_buf_to_log(5);

  _entry_buf[0] = 0x0B01;
  _entry_buf[1] = _callargs[0]; _entry_buf[2] = _callargs[1]; _entry_buf[3] = _callargs[2]; _entry_buf[4] = _callargs[3];
  write_entry_buf_to_log(5);

}
static void __watcall
record_callback_exit(unsigned pax, unsigned pdx, unsigned pbx, unsigned pcx) {
  MARSHALL_ARGS();

  _entry_buf[0] = 0x0BFF;
  _entry_buf[1] = PRSV_AX; _entry_buf[2] = PRSV_BX; _entry_buf[3] = PRSV_CX; _entry_buf[4] = PRSV_DX;
  write_entry_buf_to_log(5);

  _entry_buf[0] = 0x0BFE;
  _entry_buf[1] = _callargs[0]; _entry_buf[2] = _callargs[1]; _entry_buf[3] = _callargs[2]; _entry_buf[4] = _callargs[3];
  write_entry_buf_to_log(5);
}

static void
install_fmpdrv_recorder(void) {
  unsigned intcall_seg;
  unsigned intcall_off;
  void __far * __far * ivt;
  ivt = NULL;
  _actual_fmpdrv = ivt[0x80];

  __asm {

    JMP setintcallptr

    getra:
      POP AX
      JMP AX
    getaboveintcalloffset:
      CALL getra
      RET
    intcall: /* this is the label of the actual call we place in the IVT */
      DB 0xEB    /* JMP over... */
      DB 0x0B    /* ... the next 11 bytes (needed for ReelMagic detection) */
      DB 0x09    
      DB 'F'
      DB 'M'
      DB 'P'
      DB 'D'
      DB 'r'
      DB 'i'
      DB 'v'
      DB 'e'
      DB 'r'
      DB 0x00

      /* record the entry */
      PUSH DS
      PUSH ES
      PUSHA
      PUSH AX
      MOV AX, CS
      MOV DS, AX
      MOV ES, AX
      POP AX
      CALL record_fmpdrv_call_enter
      POPA
      POP ES
      POP DS

      /* forward the actual call, ensuring not to taint any registers
         the stack is used to place the FAR call address as this is the only
         place we can put this without impacting the segment selectors */
      ENTER 4, 0

      PUSH DS
      PUSH EAX
      MOV AX, CS
      MOV DS, AX
      MOV EAX, _actual_fmpdrv
      MOV [BP-4], EAX
      POP EAX
      POP DS

      PUSHF /* note: can't use an INT call here because these calls can recurse if the driver callback is invoked and the user application calls the drive while in said callback */
      CALL DWORD PTR [bp-4]

      LEAVE

      /* record the exit */
      PUSH DS
      PUSH ES
      PUSHA
      PUSH AX
      MOV AX, CS
      MOV DS, AX
      MOV ES, AX
      POP AX
      CALL record_fmpdrv_call_exit
      POPA
      POP ES
      POP DS

      IRET

    setintcallptr:
      PUSH AX
      MOV AX, CS
      MOV intcall_seg, AX
      CALL getaboveintcalloffset
      ADD AX, 1 /* the RET in getaboveintcalloffset is 1 byte of instructions...  this is so hacky! what the hell am i missing in watcom for asm labels !? */
      MOV intcall_off, AX
      POP AX
  };

  ivt[0x80] = MK_FP(intcall_seg, intcall_off);
  printf("Installed at INT %02Xh\n", 0x80);
}

static void
install_rmdevsys_recorder(void) {
  __asm {
    JMP setintcallptr

    getra:
      POP AX
      JMP AX

    getaboveintcalloffset:
      CALL getra
      RET
    intcall: /* this is the label of the actual call we place in the IVT */
      CMP AH, 0x98
      JNE forward2f

      /* record the entry */
      PUSH DS
      PUSH ES
      PUSHA
      PUSH AX
      MOV AX, CS
      MOV DS, AX
      MOV ES, AX
      POP AX
      CALL record_rmdevsys_call_enter
      POPA
      POP ES
      POP DS

      /* Do it the hard way! -- Dale Gribble */
      PUSHF
      PUSH CS
      CALL forward2f

      /* record the exit */
      PUSH DS
      PUSH ES
      PUSHA
      PUSH AX
      MOV AX, CS
      MOV DS, AX
      MOV ES, AX
      POP AX
      CALL record_rmdevsys_call_exit
      POPA
      POP ES
      POP DS

      IRET

    getaboveforward2foffset:
      CALL getra
      RET
    forward2f:
      DB 0xEA   /* jump far... next four bytes are FAR PTR... */
        DW 0x0000
        DW 0x0000

    setintcallptr:
      PUSHA
      PUSH DS
      PUSH ES

      MOV AH, 0x35  /* get interrupt vector */
      MOV AL, 0x2f
      INT 0x21
      /* ES:BX now is the current INT 2fh handler */

      PUSH BX /* preserve BX to pop into AX below */
      MOV AX, CS
      MOV DS, AX
      CALL getaboveforward2foffset
      MOV BX, AX
      POP AX  /* pop the preserved BX into AX */
      MOV [BX+2], AX
      MOV [BX+4], ES

      CALL getaboveintcalloffset
      MOV DX, AX
      ADD DX, 1 /* the RET in getaboveforward2foffset is 1 byte of instructions...  this is so hacky! what the hell am i missing in watcom for asm labels !? */
      MOV AH, 0x25 /* set interrupt vector */
      MOV AL, 0x2f
      INT 0x21

      POP ES
      POP DS
      POPA
  };

  printf("Installed at INT %02Xh\n", 0x2f);
}


static int
truncate_file(void) {
  int handle;

  if (_dos_creat(_filename, _A_NORMAL, &handle) != 0) return 0; /* failed */
  _dos_close(handle);
  return 1; /* success */
}

static void
dump_stats(void) {
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
  printf("CS=%04Xh DS=%04Xh ES=%04Xh SS=%04Xh\n",
    (unsigned)code_seg, (unsigned)data_seg, (unsigned)extra_seg, (unsigned)stack_seg);
  printf("_filename: %04X:%04X\n", FP_SEG(_filename), FP_OFF(_filename));
}

int
main( int argc, char *argv[] ) {
  int fd;
  fd = open_log();
  if (argc != 2) {
    printf("Usage: Recorder LOGFILE.TXT\n");
    return 1;
  }
  strcpy(_filename,argv[1]);
  if (!truncate_file()) {
    printf("Failed to truncate given log file!\n");
    return 1;
  }
  dump_stats();
  install_fmpdrv_recorder();
  install_rmdevsys_recorder();

  _dos_keep(0, (EXE_SIZE + 15) >> 4);
  return 0;
}
