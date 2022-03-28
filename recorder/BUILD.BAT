PATH C:\WATCOM\BINW;C:\WINDOWS;C:\DOS
SET INCLUDE=C:\WATCOM\H
SET WATCOM=C:\WATCOM
SET EDPATH=C:\WATCOM\EDDAT
SET WIPFC=C:\WATCOM\WIPFC

WCL /bcl=com /fe=recorder /fm=recorder.map /3 recorder.c
WCL /bcl=com /fe=hit80 /3 hit80.c
WCL /bcl=com /fe=analyze /3 analyze.c


