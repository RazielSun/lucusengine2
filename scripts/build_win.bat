@echo off
setlocal

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

set BIN_DST="%ROOT_DIR%\bin"

call "%~dp0.build.bat"

call "%~dp0compile_dx12.bat"
if errorlevel 1 exit /b 1
echo [OK] Compile shaders

set CONTENT_SRC="%ROOT_DIR%\content"
set CONTENT_DST="%BIN_DST%\content"

mklink /J "%CONTENT_DST%" "%CONTENT_SRC%"
if errorlevel 1 exit /b 1
echo [OK] Link content folder

@REM call "%~dp0run_tests.bat"
@REM if errorlevel 1 exit /b 1

"%BIN_DST%\app.exe"
if errorlevel 1 exit /b 1

exit /b 0
