@echo OFF

echo Incrementing build number...
SET build_file=include\version_build.inc
FOR /f "skip=2 tokens=3,4 delims= " %%d IN (%build_file%) DO (
    SET /A b=%%e
    SET v=%%d
)
SET /a build=%b%+1
echo // The build number is automatically rewritten every build. > %build_file%
echo #if !defined(WINTOX_VERSION) >> %build_file%
echo     #define WINTOX_VERSION "%v:~1% %build%" >> %build_file%
echo #endif >> %build_file%

echo Commencing build #%build%...
..\spcomp wintox_bhop.sp && ^
copy /Y wintox_bhop.smx ..\..\plugins > NUL && ^
copy /Y wintox_bhop.smx build\addons\sourcemod\plugins > NUL && ^
copy /Y wintox_bhop.sp +include build\addons\sourcemod\scripting > NUL

if %ERRORLEVEL% == 0 (
    echo Successfully built and copied plug-in to sourcemod\plugins
    
    echo Reloading plug-in server-side...
    FOR /f "tokens=1 delims=:" %%d IN ('ping %computername% -4 -n 1 ^| find /i "reply"') DO (
        FOR /F "tokens=3 delims= " %%g IN ("%%d") DO set ip=%%g
    )
    clircon -P"marshmallows" -a%ip% "sm plugins unload wintox_bhop;sm plugins load wintox_bhop"
    echo Plug-in reloaded!
    
    echo Compressing build archive...
    pushd build
    ..\7za a -y -tzip wintox.zip addons > NUL
    if %ERRORLEVEL% == 0 echo Successfully compressed plugin into build\wintox.zip
    popd
)