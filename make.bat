@echo OFF

REM Handle build options
REM Game type
SET gametype=invalid
IF /i "%~1" == "bhop" (
    SET gametype=bhop
) ELSE (
    IF /i "%~1" == "surf" (
        SET gametype=surf
    ) ELSE (
        echo Unknown gametype. Possible options are "bhop" or "surf"
        EXIT /b 2
    )
)
SHIFT

SET /A debug=0
SET list=

:loop_args
IF NOT "%~1" == "" (
    IF "%~1" == "-debug" SET /A debug=1
    IF "%~1" == "-l" SET list="-l"
    
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
IF %debug%==1 (
    echo ######## Commencing debug build #%build%...
) ELSE (
    echo ######## Commencing build #%build%...
)
..\spcomp wintox_%gametype%.sp %list% %debug_define%

IF NOT "%list%" == "" EXIT /b %ERRORLEVEL%
IF NOT %ERRORLEVEL% == 0 EXIT /b %ERRORLEVEL% ELSE GOTO success_build 

:success_build
echo ######## Successfully built %gametype%

copy /Y wintox_%gametype%.smx ..\..\plugins > NUL
copy /Y wintox_%gametype%.smx build\addons\sourcemod\plugins > NUL
copy /Y wintox_%gametype%.sp build\addons\sourcemod\scripting\wintox > NUL
copy /Y wintox build\addons\sourcemod\scripting\wintox\wintox > NUL
copy /Y include build\addons\sourcemod\scripting\ > NUL

REM Open version_build.inc, increment the build number, rewrite version_build.inc
echo ######## Incrementing next build number...
SET /a build=%b%+1
echo // The build number is automatically rewritten every build. > %build_file%
echo #if !defined(WINTOX_VERSION) >> %build_file%
echo     #define WINTOX_VERSION "%v:~1% %build%" >> %build_file%
echo #endif >> %build_file%

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