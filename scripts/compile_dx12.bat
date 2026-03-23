@echo off
setlocal EnableDelayedExpansion

set "ROOT_DIR=%~dp0.."
set "SRC_DIR=%ROOT_DIR%\shaders"
set "OUT_DIR=%ROOT_DIR%\bin\shaders"
set "SLANGC=%ROOT_DIR%\tools\slang\bin\slangc.exe"

if not exist "%SLANGC%" (
    echo slangc not found: %SLANGC%
    exit /b 1
)

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo Compiling Slang shaders...
echo Source: %SRC_DIR%
echo Output: %OUT_DIR%
echo.

for /R "%SRC_DIR%" %%F in (*.slang) do (
    set "FILE=%%F"
    set "REL=%%F"
    set "REL=!REL:%SRC_DIR%\=!"
    set "BASE=!REL:.slang=!"

    set "OUT_VS=%OUT_DIR%\!BASE!.vert.dxil"
    set "OUT_PS=%OUT_DIR%\!BASE!.frag.dxil"

    for %%D in ("!OUT_VS!") do (
        if not exist "%%~dpD" mkdir "%%~dpD"
    )

    echo [VS] %%F ^> !OUT_VS!
    "%SLANGC%" "%%F" -entry vsMain -profile vs_6_0 -target dxil -o "!OUT_VS!"
    if errorlevel 1 exit /b 1

    echo [PS] %%F ^> !OUT_PS!
    "%SLANGC%" "%%F" -entry psMain -profile ps_6_0 -target dxil -o "!OUT_PS!"
    if errorlevel 1 exit /b 1

    echo.
)

echo Done.
exit /b 0