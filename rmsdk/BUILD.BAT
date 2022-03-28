PATH C:\WATCOM\BINW;C:\WINDOWS;C:\DOS
SET INCLUDE=C:\WATCOM\H
SET WATCOM=C:\WATCOM
SET EDPATH=C:\WATCOM\EDDAT
SET WIPFC=C:\WATCOM\WIPFC


REM  /3 allows for 386 instructions
REM  /s disable stack overflow check
REM  /zu tells the compiler to NOT assume that SS and DS will always be the same!
REM  /ms explicitly sets the memory model to small... critically important to understand for a project like this
REM  /zdf generates code that allows DS register to point to shit outside "DGROUP"

WCL /bcl=dos /fe=player /3 /s /zu /ms /zdf player.c reelmagc.c logf.c
WCL /bcl=dos /fe=streamer /3 /s /zu /ms /zdf streamer.c reelmagc.c logf.c

