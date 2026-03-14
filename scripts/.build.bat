@echo off
setlocal

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

set "CMAKE_DIR=%ROOT_DIR%\cmake"
set "BUILD_DIR=%ROOT_DIR%\build\win-clang"
set "CONFIG=%~1"

if "%CONFIG%"=="" set "CONFIG=Debug"

set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist %VSWHERE% (
	echo "Couldn't find vswhere, you need visual studio 15.2 or later"
	exit /b 1
)

echo %VSWHERE%

for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set VSINSTALL=%%i
)

if exist "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" (
  call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
) else (
	echo "Couldn't find VC"
	exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo == Lucus Build ==
echo ROOT_DIR:  %ROOT_DIR%
echo CMAKE_DIR: %CMAKE_DIR%
echo BUILD_DIR: %BUILD_DIR%
echo CONFIG:    %CONFIG%
echo VSINSTALL: %VSINSTALL%
echo.

cmake -S "%CMAKE_DIR%" -B "%BUILD_DIR%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=%CONFIG% ^
  -DCMAKE_C_COMPILER=clang-cl ^
  -DCMAKE_CXX_COMPILER=clang-cl ^
  -DCMAKE_LINKER=link.exe

if errorlevel 1 (
    echo [ERROR] CMake configure failed
    exit /b 1
)

cmake --build "%BUILD_DIR%" --config %CONFIG%
if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)

echo.
echo [OK] Build finished successfully
exit /b 0