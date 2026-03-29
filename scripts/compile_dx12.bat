@echo off
setlocal EnableDelayedExpansion

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"

set "SRC_DIR=%ROOT_DIR%\shaders"
set "OUT_DIR=%ROOT_DIR%\bin\shaders"

set "PATH=%ROOT_DIR%\scripts\tools\dxc\bin\x64;%ROOT_DIR%\scripts\tools\slang\bin;%PATH%"

where slangc >nul 2>nul || (
    echo slangc not found
    exit /b 1
)

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo Compiling Slang shaders...
echo Source: %SRC_DIR%
echo Output: %OUT_DIR%
echo.

for /R "%SRC_DIR%" %%F in (*.slang) do (
    set "FILE=%%F"
    set "TEST=!FILE:\common\=!"

    if /I "!TEST!"=="!FILE!" (

        set "REL=%%F"
        set "REL=!REL:%SRC_DIR%\=!"
        set "BASE=!REL:.slang=!"

        set "OUT_VS=%OUT_DIR%\!BASE!.vert.dxil"
        set "OUT_PS=%OUT_DIR%\!BASE!.frag.dxil"

        for %%D in ("!OUT_VS!") do (
            if not exist "%%~dpD" mkdir "%%~dpD"
        )

        echo [VS] %%F ^> !OUT_VS!
        slangc "%%F" -entry vsMain -profile vs_6_0 -target dxil -D TARGET_DX12=1 -o "!OUT_VS!" -I "%SRC_DIR%"
        if errorlevel 1 exit /b 1

        echo [PS] %%F ^> !OUT_PS!
        slangc "%%F" -entry psMain -profile ps_6_0 -target dxil -D TARGET_DX12=1 -o "!OUT_PS!" -I "%SRC_DIR%"
        if errorlevel 1 exit /b 1

        echo.
    )
)

echo Done.
exit /b 0