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
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <i86.h>
#include <dos.h>

static FILE * _logout = NULL;
static int _stdouton = 1;
static void
logf(const char * const fmt, ...) {
  va_list vl;

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

static uint8_t _fmp_driver_int = 0x00; /* 0x00 = not found */
static uint8_t
find_fmpdriver(void) {
  uint16_t intnum;
  char _far * _far *ivt_entry;

  ivt_entry = NULL; /* note: NULL on a far pointer in real mode is the IVT base address */

  for (intnum = 0x80; intnum <= 0xFF; ++intnum) {
    if (ivt_entry[intnum] == NULL) continue;
    if (_fstrncmp(&ivt_entry[intnum][3], "FMPDriver", 9) == 0) {
      _fmp_driver_int = intnum;
      return intnum; //success -- found the ReelMagic driver @ intnum
    }
  }

  return 0; //failed -- driver not found
}

static void __far __stdcall
driver_callback(unsigned p1, unsigned p2, unsigned p3, unsigned p4) {
  logf("driver_callback(%04Xh, %04Xh, %04Xh, %04Xh)\n",
    (unsigned)p1,
    (unsigned)p2,
    (unsigned)p3,
    (unsigned)p4
  );
}

static uint16_t
call_vbios(
  const uint8_t  function,
  const uint8_t  param1,
  const uint16_t param2,
  const uint16_t param3,
  const uint16_t param4) {

  union REGS r;

  logf("ENTER call_vbios(%02Xh, %02Xh, %04Xh, %04Xh, %04Xh)\n",
    (unsigned)function, (unsigned)param1, (unsigned)param2, (unsigned)param3, (unsigned)param4);

  r.x.ax = (function<<8) | param1;
  r.x.bx = param2;
  r.x.cx = param3;
  r.x.dx = param4;

  #ifdef __386__
    int386(0x10, &r, &r);
  #else
    int86(0x10, &r, &r);
  #endif

  logf("EXIT call_vbios(%02Xh, %02Xh, %04Xh, %04Xh, %04Xh) = %04Xh %04Xh %04Xh %04Xh\n",
    (unsigned)function, (unsigned)param1, (unsigned)param2, (unsigned)param3, (unsigned)param4,
    (unsigned)r.x.ax, (unsigned)r.x.bx, (unsigned)r.x.cx, (unsigned)r.x.dx);

  return r.x.ax;
}

static uint16_t
call_rmdevsys(
  const uint8_t function,
  const uint16_t subfunction,
  const uint16_t param1,
  const uint16_t param2) {

  union REGS r;

  logf("ENTER call_rmdevsys(%02Xh, %04Xh, %04Xh, %04Xh)\n",
    (unsigned)function, (unsigned)subfunction, (unsigned)param1, (unsigned)param2);

  r.x.ax = 0x9800 | function;
  r.x.bx = subfunction;
  r.x.cx = param1;
  r.x.dx = param2;

  #ifdef __386__
    int386(0x2F, &r, &r);
  #else
    int86(0x2F, &r, &r);
  #endif

  logf("EXIT call_rmdevsys(%02Xh, %04Xh, %04Xh, %04Xh) = %04Xh %04Xh %04Xh %04Xh\n",
    (unsigned)function, (unsigned)subfunction, (unsigned)param1, (unsigned)param2,
    (unsigned)r.x.ax, (unsigned)r.x.bx, (unsigned)r.x.cx, (unsigned)r.x.dx);

  return r.x.ax;
}

static uint32_t
call_fmpdriver(
  const uint8_t command,
  const uint8_t media_handle,
  const uint16_t subfunc,
  const uint16_t param1,
  const uint16_t param2) {

  uint32_t rv;
  union REGS r;

  logf("ENTER driver_call(%02Xh, %02Xh, %04Xh, %04Xh, %04Xh)\n",
    command, media_handle, subfunc, param1, param2);

  r.x.bx   = command;
  r.x.bx <<= 8;
  r.x.bx  |= media_handle;
  r.x.cx = subfunc;
  r.x.ax = param1;
  r.x.dx = param2;

  #ifdef __386__
    int386(_fmp_driver_int, &r, &r);
  #else
    int86(_fmp_driver_int, &r, &r);
  #endif

  rv = r.x.dx;
  rv <<= 16;
  rv |= r.x.ax;

  logf("EXIT driver_call(%02Xh, %02Xh, %04Xh, %04Xh, %04Xh) = %04X%04Xh\n",
    command, media_handle, subfunc, param1, param2, r.x.dx, r.x.ax);
  return rv;
}

static uint32_t
call_fmpdriver_ptr(
  const uint8_t command,
  const uint8_t media_handle,
  const uint16_t subfunc,
  void _far * const ptr) {
  return call_fmpdriver(command, media_handle, subfunc,
    FP_OFF(ptr), FP_SEG(ptr));
}

static void
waitplay(const uint8_t media_handle, const unsigned sleep_interval) {
  FILE * orig_logout;
  int orig_stdouton;
  uint32_t driver_call_result;

  orig_logout = _logout;
  orig_stdouton = _stdouton;

  logf("%s waiting on media handle #%u play to complete.\n", sleep_interval ? "Sleep" : "Busy", media_handle);
  if (sleep_interval)
    logf("Sleep interval is %u\n", sleep_interval);
  else {
    /* this gets WAY TOO noisy! shut off all logging while busy waiting */
    _logout = NULL;
    _stdouton = 0;
  }

  /* poll this thing... */
  if (sleep_interval)
    while (((driver_call_result = call_fmpdriver(0xa, media_handle, 0x204, 0,0)) & 3) == 0)
      sleep(sleep_interval);
  else
    while (((driver_call_result = call_fmpdriver(0xa, media_handle, 0x204, 0,0)) & 3) == 0);

  /* restore things */
  _logout = orig_logout;
  _stdouton = orig_stdouton;

  logf("Done waiting on media handle #%u Last driver call returned: %04X%04Xh\n", media_handle, (unsigned)(driver_call_result>>16), (unsigned)(driver_call_result));
}

static uint32_t  _last_call_rv = 0;
static uint8_t   _last_vga_mode = 0;
static uint8_t   _hand[5] = {0};
static char      _strbuf[512] = {'\0'};
static int
process_script_line(char *line) {
  unsigned command;
  unsigned media_handle;
  unsigned subfunc;
  unsigned param1;
  unsigned param2;
  void _far * ptr;
  char *strstart;
  char *strend;

  for (;line[0] == ' ';++line); /* skip and leading spaces */
  if (line[0] == '#') return 1; /* success - comment */
  if (line[0] == '\r') return 1; /* success - empty line */
  if (line[0] == '\n') return 1; /* success - empty line */
  if (strncasecmp(line, "CALLRM ", 7) == 0) {
    if (sscanf(line+7, "%X %X %X %X %X", &command, &media_handle, &subfunc, &param1, &param2) == 5) {
      _last_call_rv = call_fmpdriver(command, media_handle, subfunc, param1, param2);
    }
    else if (sscanf(line+7, "%X HAND%X %X %X %X", &command, &media_handle, &subfunc, &param1, &param2) == 5) {
      _last_call_rv = call_fmpdriver(command, _hand[media_handle], subfunc, param1, param2);
    }
    else if (sscanf(line+7, "%X %X %X ", &command, &media_handle, &subfunc) == 3) {
      strstart = strchr(line+7, '\"');
      if (strstart != NULL) {
        strend = strchr(++strstart, '\"');
        if (strend == NULL) goto bad_command;
        (*strend) = '\0';
        ptr = strstart;
      }
      else if (strstr(line+7, "CBFUNC") != NULL) {
        ptr = &driver_callback;
      }
      else if (strstr(line+7, "STRBUF") != NULL) {
        ptr = &_strbuf;
      }
      else if (strstr(line+7, "NULL") != NULL) {
        ptr = NULL;
      }
      else goto bad_command;
      _last_call_rv = call_fmpdriver_ptr(command, media_handle, subfunc, ptr);
    }
    else {
      goto bad_command;
    }
  }
  else if (strncasecmp(line, "CALLRMS ", 8) == 0) {
    if (sscanf(line+8, "%X %X %X %X", &command, &subfunc, &param1, &param2) != 4)
      goto bad_command;
    _last_call_rv = call_rmdevsys(command, subfunc, param1, param2);
  }
  else if (strncasecmp(line, "CALLVBIOS ", 10) == 0) {
    if (sscanf(line+10, "%X %X %X %X %X", &command, &media_handle, &subfunc, &param1, &param2) != 5)
      goto bad_command;
    _last_call_rv = call_vbios(command, media_handle, subfunc, param1, param2);
  }
  else if (strncasecmp(line, "STORVGAMODE", 11) == 0) {
    _last_vga_mode = call_vbios(0xf,0,0,0,0);
    logf("Current VGA Mode %02Xh Stored\n", _last_vga_mode);
  }
  else if (strncasecmp(line, "LOADVGAMODE", 11) == 0) {
    _last_vga_mode = call_vbios(0x0,_last_vga_mode,0,0,0);
    logf("Saved VGA Mode %02Xh Restored\n", _last_vga_mode);
  }
  else if (strncasecmp(line, "STORHAND ", 9) == 0) {
    if (sscanf(line+9, "%X", &param1) != 1) goto bad_command;
    if (param1 >= (sizeof(_hand) / sizeof(_hand[0]))) goto bad_command;
    if ((_last_call_rv & 0xFF) == 0) {
      logf("Last call failed!\n");
      return -1; /* failure */
    }
    _hand[param1] = _last_call_rv & 0xFF;
    logf("Stored handle %u as HAND%u\n", _hand[param1], param1);
  }
  else if (strncasecmp(line, "SLEEP ", 6) == 0) {
    if (sscanf(line+6, "%u", &param1) != 1)
      goto bad_command;
    logf("Sleeping for %u seconds...", param1);
    sleep(param1);
    logf("\n");
  }
  else if (strncasecmp(line, "WAITPLAY ", 9) == 0) {
    param1 = 0;
    if (sscanf(line+9, "HAND%u %u", &media_handle, &param1) >= 1) {
      media_handle = _hand[media_handle];
    }
    else if (sscanf(line+9, "%u %u", &media_handle, &param1) >= 1) {
      /**/
    }
    else goto bad_command;
    waitplay(media_handle, param1);
  }
  else if (strncasecmp(line, "STRBUFADDR", 10) == 0) {
    logf("STRBUF is at [%04X:%04X]\n", (unsigned)FP_SEG(_strbuf), (unsigned)FP_OFF(_strbuf));
  }
  else if (strncasecmp(line, "STRBUFPRINT", 11) == 0) {
    logf("STRBUF Contents: %s\n", _strbuf);
  }
  else if (strncasecmp(line, "STRBUFSETX ", 11) == 0) {
    if (sscanf(line+11, "%X %X", &param1, &param2) != 2)
      goto bad_command;
    _strbuf[param1] = (char)param2;
  }
  else if (strncasecmp(line, "STRBUFSETS ", 11) == 0) {
    if (strchr(line, '\r') != NULL) strchr(line, '\r')[0] = '\0';
    if (strchr(line, '\n') != NULL) strchr(line, '\n')[0] = '\0';
    strcpy(_strbuf, line+11);
  }
  else if (strncasecmp(line, "MUTESTDOUT ", 11) == 0) {
    if (sscanf(line+11, "%X", &param1) != 1)
      goto bad_command;
    fflush(stdout);
    _stdouton = param1 ? 0 : 1;
    logf("STDOUT is %s\n", _stdouton ? "On" : "Off");
  }
  else if (strncasecmp(line, "EXIT", 4) == 0) {
    return 0;
  }
  else if (strncasecmp(line, "QUIT", 4) == 0) {
    return 0;
  }
  else {
    goto bad_command;
  }

  return 1; /* success */

bad_command:
  logf("Bad Command: %s\n", line);
  return -1; /* failure */
}

static int
run_script(FILE * const fh) {
  static char linebuf[256];
  char *line;
  while ((line = fgets(linebuf, sizeof(linebuf), fh)) != NULL) {
    switch(process_script_line(line)) {
    case 0: return 0;
    case -1: return -1;
    }
  }
  return -1;
}

static void
log_rmdriver_version(void) {
  uint16_t ver;
  ver = call_rmdevsys(0,1,0,0);
  logf("RealMagic Driver Version Detected: %u.%u\n", (unsigned)(ver>>8), (unsigned)(ver&0xFF));
}

static void
log_rmdriver_location(void) {
  char loc[256];
  loc[0] = '\0';
  call_rmdevsys(3, FP_OFF(loc), 0, FP_SEG(loc));
  logf("RealMagic Driver is Located at: %sFMPDRV.EXE\n", loc);
}

static void
openlogout(const char * const basepath) {
  char filename[256];

  snprintf(filename, sizeof(filename), "%s%sRMPROBE.LOG",
    (basepath == NULL) ? "" : basepath, (basepath == NULL) ? "" : "\\");

  logf("Writing to logfile '%s'...\n", filename);
  _logout = fopen(filename, "w");
}

static FILE *
opencmdin(const char * const basepath) {
  char filename[256];

  snprintf(filename, sizeof(filename), "%s%sCOMMANDS.TXT",
    (basepath == NULL) ? "" : basepath, (basepath == NULL) ? "" : "\\");

  logf("Opening command file '%s'...\n", filename);
  return fopen(filename, "r");
}

int
main(int argc, char *argv[]) {
  int rv;
  FILE *cmdin;

  cmdin = stdin;

  if (argc == 2) {
    cmdin = opencmdin(argv[1]);
    if (cmdin == NULL) {
      logf("Failed to open command file!\n");
      return 1;
    }
    openlogout(argv[1]);
  }
  else {
    openlogout(NULL);
  }

  /* first we need to check if the driver is loaded */
  if (call_rmdevsys(0,0,0,0) != 0x524D) {
    logf("ERROR: RMDEV.SYS Not Found!\n");
    return 1;
  }
  log_rmdriver_version();
  log_rmdriver_location();
  if (!find_fmpdriver()) {
    logf("ERROR: FMPDRV.EXE Not Resident!\n");
    return 1;
  }
  logf("FMPDRV.EXE Detected Resident at INT %02Xh\n", (unsigned)_fmp_driver_int);


  logf("\nMPEG READY\n\n");
  rv = run_script(cmdin);

  if (_logout != NULL) fclose(_logout);

  return rv ? 0 : 1;
}


