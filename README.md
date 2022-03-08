
# Overview

This is a quick hacked together DOS tool used to probe and learn how the ReelMagic DOS driver works.


# Usage

Prerequisites:

* Make sure that `RMDEV.SYS` (or `MPGDEV.SYS`) is properly setup in your `CONFIG.SYS` file.
* Manually call `FMPDRV.EXE` to load the ReelMagic driver before running `RMPROBE.EXE`

## Logging

The `RMPROBE.EXE` program write to a log file called `RMPROBE.LOG` in the current working directory. This
way, all output is automatically captured, as well as if VGA is wacked, the output can still be captured.
The log file is cleared each time the program is ran.

## Interactive Mode

The `RMPROBE.EXE` program can be called as-is to open up an interactive prompt which can be used to peek
and poke the ReelMagic driver as well as some other things.

## Test Run Mode

The `RMPROBE.EXE` program can be given an argument of a directory containing a `COMMANDS.TXT` file. It
runs through all the commands in the `COMMANDS.TXT` and logging output goes to another file in the given
directory called `RMPROBE.LOG`.

For example, if the following command is ran:
```
RMPROBE.EXE TESTS\TEST01
```
... then the program will run through all the commands in a file called `TESTS\TEST01\COMMANDS.TXT` and
output will be written to `TESTS\TEST01\RMPROBE.LOG`


# Tests

I have included a battery of tests that I need to be ran on a real ReelMagic setup to further understand
how this thing actually works.

The ReelMagic CD which has the `D:\CORP\SIGLOGO.MPG` file must be inserted for these tests. If the CD-ROM
drive letter is not D, then the `TESTS\*\COMMANDS.TXT` files must be updated to reflect the proper CD-ROM
drive letter.


## Test 01

This test is used to confirm my fundamental understanding of the ReelMagic driver interface. This test
attempts to re-create the basic functionality of `SPLAYER.EXE` 

To run this test:
```
RMPROBE.EXE TESTS\TEST01
```

Observations to record:

* The `RMPROBE.LOG` file located in the test directory.
* Did the video play as expected?
* Did the tool return back to the DOS prompt as expected?
* Did the tool return the video mode back to normal?

## Test 02

Same as Test 01, however, the process sleeps between play status polling.

To run this test:
```
RMPROBE.EXE TESTS\TEST02
```

Same observations as Test 01.

## Test 03

Same as Test 01, however, stdout is not muted.

To run this test:
```
RMPROBE.EXE TESTS\TEST03
```

Same observations as Test 01.

## Test 04

Same as Test 01, however, the VGA mode is not changed

To run this test:
```
RMPROBE.EXE TESTS\TEST04
```

Observations to record:

* Same as Test 01
* Did the DOS command text show on top of the video? Or was only the video visible?

## Test 05
Same as Test 01, however, using RTZ style callback registration.

To run this test:
```
RMPROBE.EXE TESTS\TEST05
```

Same observations as Test 01.


## Test 06
Same as Test 01, however, using The Horde style callback registration.

To run this test:
```
RMPROBE.EXE TESTS\TEST06
```

Same observations as Test 01.

## Test 07

This test attempts to re-create the behavior of the way RTZ calls ReelMagic.

To run this test:
```
RMPROBE.EXE TESTS\TEST07
```

Observations to record:

* The `RMPROBE.LOG` file located in the test directory.
* Did the video play as expected?
* Did the tool return back to the DOS prompt as expected?
* Did the tool return the video mode back to normal?

## Test 08
Same as Test 07, however, "poisoning" what is think is the callback pointer.

To run this test:
```
RMPROBE.EXE TESTS\TEST08
```

Same observations as Test 07.

## Test 09
Same as Test 07, however, not making a call to RMDEV.SYS after pre-setup.

To run this test:
```
RMPROBE.EXE TESTS\TEST09
```

Same observations as Test 07.

## Test 10

This test attempts to re-create the behavior of the way LOTR calls ReelMagic.

To run this test:
```
RMPROBE.EXE TESTS\TEST10
```

Observations to record:

* The `RMPROBE.LOG` file located in the test directory.
* Did the video play as expected?
* Did the tool return back to the DOS prompt as expected?
* Did the tool return the video mode back to normal?
* Did the video pause for a bit after first playing?
* Did the picture freeze or did the screen go black?
* Did the video restart or resume from the pause?

## Test 11

This test attempts to re-create the behavior of the way The Horde calls ReelMagic.

To run this test:
```
RMPROBE.EXE TESTS\TEST11
```

Observations to record:

* The `RMPROBE.LOG` file located in the test directory.
* Did the video play as expected?
* Did the tool return back to the DOS prompt as expected?
* Did the tool return the video mode back to normal?


# Compiling

The `RMPROBE.EXE` program can be compiled by running the `BUILD.BAT` file. The Open Watcom 1.9 DOS C
compiler is required and can be obtained from here: http://www.openwatcom.org/download.php

The releases posted here were compiled using DOSBox release 0.74-3. I gotta admit, using DOSBox for
purposes other than playing retro games seems a bit off, but works well for this! No ReelMagic support is
required on the machine used for compiling this tool; it is just required on the machine running it.


# Commands

Probably best to see `process_script_line()` in `rmprobe.c` but here is some of the usage.

The command syntax for this tool is as follows:

## CALLRM

Make a ReelMagic `driver_call()` as described in `RMDOS_API.md`

Variations:
```
CALLRM X X X X X
CALLRM X HANDX X X X
CALLRM X X X CBFUNC
CALLRM X X X STRBUF
CALLRM X X X NULL
```
The `X`s are hexdecimal numbers


## CALLRMS

Make a call to the resident ReelMagic `RMDEV.SYS` (or `MPGDEV.SYS) file.

```
CALLRMS X X X X
```
The `X`s are hexdecimal numbers


## CALLVBIOS

Make an INT 10h call to the VGA BIOS.

```
CALLVBIOS X X X X X
```
The `X`s are hexdecimal numbers


## STORVGAMODE

Store the current VGA mode. Essentially a `CALLVBIOS F 0 0 0 0` and preserves the result.

## LOADVGAMODE

Loads the previously stored VGA mode. Essentially a `CALLVBIOS 0 LASTMODE 0 0 0`

## STORHAND

Stores the return value of the previous `CALLRM` command into the given variable slot number.

```
STORHAND X
```
The `X` is a decimal number

## SLEEP

Sleeps for a given amount of seconds.

```
SLEEP X
```
The `X` is a decimal number


## WAITPLAY

Waits for play to complete. Functionally polls `CALLRM A MEDIA_HANDLE 204 0 0` for & 3 to be non-zero.

Variations:
```
WAITPLAY X
WAITPLAY X X
WAITPLAY HANDX
WAITPLAY HANDX X
```
The `X` are decimal numbers

## STRBUFADDR

Prints the pointer address of `STRBUF`. See `process_script_line()` for usage.

## STRBUFPRINT

Prints the string contents of `STRBUF`. See `process_script_line()` for usage.

## STRBUFSETX

Set `STRBUF` to the given character value. See `process_script_line()` for usage.

## STRBUFSETS

Set `STRBUF` to the given string value. See `process_script_line()` for usage.

## MUTESTDOUT

Turn on/off stdout. Log output will continue even if stdout is muted

```
MUTESTDOUT X
```
The `X` is either a 0 or 1.


## EXIT / QUIT

Exit/quit the program.


