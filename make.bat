@echo OFF

SET spcomp=.\spcomp_longlines.exe
SET spinclude=..\include

REM Essentially, our first argument is the command, so "report" generates a report
IF /i "%~1" == "report" (
    PUSHD .\reports\
    python gen_pivotal_report.py
    POPD
    EXIT /b 0
)

REM Handle build options
REM Game type
SET gametype=invalid
IF /i "%~1" == "bhop" (
    SET gametype=bhop
) ELSE (
    IF /i "%~1" == "surf" (
        SET gametype=surf
    ) ELSE (
        echo Unknown gametype or command. Possible options are "bhop" or "surf"
        EXIT /b 2
    )
)
SHIFT

SET /A increment=1
SET /A debug=0
SET list=

:loop_args
IF NOT "%~1" == "" (
    IF "%~1" == "-debug" SET /A debug=1
    IF "%~1" == "-l" SET list="-l"
    IF "%~1" == "-noinc" SET /A increment=0
    
    SHIFT
    GOTO :loop_args
)

IF %debug%==1 SET "debug_define=WINTOX_DEBUG=1"

REM Grab the current build number
SET build_file=wintox\version_build.inc
FOR /f "skip=2 tokens=3,4 delims= " %%d IN (%build_file%) DO (
    SET /A b=%%e
    SET v=%%d
)

REM Compile the plug-in and copy it to the server plug-ins dir and our build dir
SET /a build=%b%+1
IF %debug%==1 (
    echo ######## Commencing debug build #%build%...
) ELSE (
    echo ######## Commencing build #%build%...
)
%spcomp% wintox_%gametype%.sp %list% %debug_define% -i%spinclude%

IF NOT "%list%" == "" EXIT /b %ERRORLEVEL%
IF NOT %ERRORLEVEL% == 0 EXIT /b %ERRORLEVEL% ELSE GOTO success_build 

:success_build
echo ######## Successfully built %gametype%

copy /Y wintox_%gametype%.smx ..\..\plugins > NUL 2> NUL
copy /Y wintox_%gametype%.smx build\addons\sourcemod\plugins > NUL 2> NUL
copy /Y wintox_%gametype%.sp build\addons\sourcemod\scripting\wintox > NUL 2> NUL
copy /Y wintox build\addons\sourcemod\scripting\wintox\wintox > NUL 2> NUL
copy /Y include build\addons\sourcemod\scripting\ > NUL 2> NUL

REM Open version_build.inc, increment the build number, rewrite version_build.inc
IF %increment%==1 (
    echo ######## Incrementing next build number...
    CALL :write_increment
) ELSE (
    echo ######## NOT incrementing next build number...
)

echo ######## Reloading plug-in server-side...
FOR /f "tokens=1 delims=:" %%d IN ('ping %computername% -4 -n 1 ^| find /i "reply"') DO (
    FOR /F "tokens=3 delims= " %%g IN ("%%d") DO set ip=%%g
)
clircon -P"marshmallows" -a%ip% "sm plugins unload wintox_%gametype%;sm plugins load wintox_%gametype%"

echo ######## Compressing build archive...
pushd build
..\7za a -y -tzip wintox.zip addons > NUL
if %ERRORLEVEL% == 0 echo ######## Successfully compressed plugin into build\wintox.zip
popd

GOTO end

:write_increment
echo // This file is automatically rewritten every build. > %build_file%
echo #if !defined(WINTOX_VERSION) >> %build_file%
echo     #define WINTOX_VERSION "%v:~1% %build%" >> %build_file%
echo #endif >> %build_file%
GOTO :EOF

:end