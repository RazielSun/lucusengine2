@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"
set "RUN_DIR=%ROOT_DIR%\bin"

set "LOG_DIR=%SCRIPT_DIR%logs"
set "TEST_LIST_FILE=%SCRIPT_DIR%tests.list"
set "APP=%ROOT_DIR%\bin\app.exe"
if defined TICKS (
    set "TEST_TICKS=%TICKS%"
) else (
    set "TEST_TICKS=5"
)

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

if not exist "%TEST_LIST_FILE%" (
    echo [ERROR] Missing test list: %TEST_LIST_FILE%
    exit /b 1
)

if not exist "%APP%" (
    echo [ERROR] App not found: %APP%
    exit /b 1
)

if not exist "%RUN_DIR%" (
    echo [ERROR] Run directory not found: %RUN_DIR%
    exit /b 1
)

for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%I"
set "LOG_FILE=%LOG_DIR%\run_tests_win_%STAMP%.log"
set "LATEST_LOG=%LOG_DIR%\run_tests_win_latest.log"

break > "%LOG_FILE%"

>> "%LOG_FILE%" echo == Lucus Tests ==
>> "%LOG_FILE%" echo ROOT_DIR:  %ROOT_DIR%
>> "%LOG_FILE%" echo RUN_DIR:   %RUN_DIR%
>> "%LOG_FILE%" echo APP:       %APP%
>> "%LOG_FILE%" echo TEST_LIST: %TEST_LIST_FILE%
>> "%LOG_FILE%" echo TICKS:     %TEST_TICKS%
>> "%LOG_FILE%" echo LOG_FILE:  %LOG_FILE%
>> "%LOG_FILE%" echo.

echo == Lucus Tests ==
echo Log file: %LOG_FILE%
echo.

for /f "usebackq delims=" %%T in ("%TEST_LIST_FILE%") do (
    set "TEST_SCRIPT=%%T"
    if not "!TEST_SCRIPT!"=="" (
        if not "!TEST_SCRIPT:~0,1!"=="#" (
            echo [RUN ] !TEST_SCRIPT!
            >> "%LOG_FILE%" echo [RUN ] !TEST_SCRIPT!
            "%APP%" --directory "%RUN_DIR%" --script "!TEST_SCRIPT!" --ticks "%TEST_TICKS%" >> "%LOG_FILE%" 2>&1
            if errorlevel 1 (
                echo [FAIL] !TEST_SCRIPT!
                echo [FAIL] Full log: %LOG_FILE%
                >> "%LOG_FILE%" echo [FAIL] !TEST_SCRIPT!
                copy /Y "%LOG_FILE%" "%LATEST_LOG%" >nul
                exit /b 1
            )
            echo [ OK ] !TEST_SCRIPT!
            >> "%LOG_FILE%" echo [ OK ] !TEST_SCRIPT!
            >> "%LOG_FILE%" echo.
        )
    )
)

echo [ OK ] All tests passed
>> "%LOG_FILE%" echo [ OK ] All tests passed
copy /Y "%LOG_FILE%" "%LATEST_LOG%" >nul
exit /b 0
