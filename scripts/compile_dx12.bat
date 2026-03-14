@echo off
setlocal

set "ROOT_DIR=%~dp0.."

set BIN_DST="%ROOT_DIR%\bin"
set SHADERS_DST="%BIN_DST%\shaders"

if not exist "%SHADERS_DST%" mkdir "%SHADERS_DST%"
xcopy "%ROOT_DIR%\shaders\dx12\*.hlsl" "%SHADERS_DST%\" /Y