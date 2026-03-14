@echo off
setlocal

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

call "%~dp0.build.bat"

call "%~dp0compile_dx12.bat"
echo [OK] Compile shaders

set CONTENT_SRC="%ROOT_DIR%\content"
set CONTENT_DST="%BIN_DST%\content"

mklink /J "%CONTENT_DST%" "%CONTENT_SRC%"
echo [OK] Link content folder

"%ROOT_DIR%\bin\app.exe"

exit /b 0