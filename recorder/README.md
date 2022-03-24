
# Running Recorder on Target PC

First, manually load the FMPDRV.EXE TSR:
```
C:\REALMAGC\FMPDRV.EXE
```
-- OR --
```
LOADHIGH C:\REALMAGC\FMPDRV.EXE
```

Note: Oddly Return to Zork seems to not have enough memory if `LOADHIGH` is used. You may need to play around with this.

Start the data recorder.
```
RECORDER.COM C:\recorder.log
```

IMPORTANT: You MUST use the full path of the file the recorder outputs to!

Start the game:
```
CD \RTZ-RM
RTZ
```
-- OR --
```
CD \HORDE
HORDE
```


# Anylzing the Recorded Log

Once you have captured what you want, the `ANALYZE` command must then be used to view the recorder log file.
(`C:\recorder.log` in the above example)


The 'analyze.c' file compiles for both DOS (`BUILD.BAT`) and Linux (`Makefile`). 


