@echo OFF

..\spcomp wintox_bhop.sp && ^
copy /Y wintox_bhop.smx ..\..\plugins > NUL && ^
copy /Y wintox_bhop.smx build\addons\sourcemod\plugins > NUL && ^
copy /Y wintox_bhop.sp +include build\addons\sourcemod\scripting > NUL

if %ERRORLEVEL% == 0 (
    echo Successfully built and copied plug-in to sourcemod\plugins
    pushd build
    ..\7za a -y -tzip wintox.zip addons > NUL
    if %ERRORLEVEL% == 0 echo Successfully compressed plugin into build\wintox.zip
    popd
)