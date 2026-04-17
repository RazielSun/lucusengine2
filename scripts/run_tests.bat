@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

if not defined RUN_DIR set "RUN_DIR=%ROOT_DIR%\bin"
set "LOG_DIR=%SCRIPT_DIR%logs"
if not defined TEST_LIST_FILE set "TEST_LIST_FILE=%SCRIPT_DIR%tests.list"
if not defined APP set "APP=%ROOT_DIR%\bin\app.exe"
for %%I in ("%RUN_DIR%") do set "RUN_DIR=%%~fI"
for %%I in ("%TEST_LIST_FILE%") do set "TEST_LIST_FILE=%%~fI"
for %%I in ("%APP%") do set "APP=%%~fI"
if defined TICKS (
    set "TEST_TICKS=%TICKS%"
) else (
    set "TEST_TICKS=10"
)

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

if not exist "%APP%" (
    echo [ERROR] App not found: %APP%
    exit /b 1
)

if not exist "%RUN_DIR%" (
    echo [ERROR] Run directory not found: %RUN_DIR%
    exit /b 1
)

if not exist "%RUN_DIR%\content" (
    mklink /J "%RUN_DIR%\content" "%ROOT_DIR%\content" >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] Run content directory not found and could not create junction: %RUN_DIR%\content
        exit /b 1
    )
)

set "SCRIPTS_DIR=%RUN_DIR%\content\scripts"
if not defined TEST_DIR set "TEST_DIR=%SCRIPTS_DIR%\tests"
for %%I in ("%TEST_DIR%") do set "TEST_DIR=%%~fI"

for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%I"
set "LOG_FILE=%LOG_DIR%\run_tests_win_%STAMP%.log"
set "LATEST_LOG=%LOG_DIR%\run_tests_win_latest.log"
set "RUN_TESTS_FILE=%LOG_DIR%\run_tests_win_%STAMP%.tests"

type nul > "%LOG_FILE%"

>> "%LOG_FILE%" echo == Lucus Tests ==
>> "%LOG_FILE%" echo ROOT_DIR:  %ROOT_DIR%
>> "%LOG_FILE%" echo RUN_DIR:   %RUN_DIR%
>> "%LOG_FILE%" echo APP:       %APP%
>> "%LOG_FILE%" echo TEST_LIST: %TEST_LIST_FILE%
>> "%LOG_FILE%" echo TEST_DIR:  %TEST_DIR%
>> "%LOG_FILE%" echo TICKS:     %TEST_TICKS%
>> "%LOG_FILE%" echo LOG_FILE:  %LOG_FILE%
>> "%LOG_FILE%" echo.
copy /Y "%LOG_FILE%" "%LATEST_LOG%" >nul

echo == Lucus Tests ==
echo Log file: %LOG_FILE%
echo.

if exist "%TEST_LIST_FILE%" (
    echo [INFO] Loading tests from list
    >> "%LOG_FILE%" echo [INFO] Loading tests from list
    type nul > "%RUN_TESTS_FILE%"

    for /f "usebackq delims=" %%T in ("%TEST_LIST_FILE%") do (
        set "TEST_SCRIPT=%%T"
        if not "!TEST_SCRIPT!"=="" (
            if not "!TEST_SCRIPT:~0,1!"=="#" (
                >> "%RUN_TESTS_FILE%" echo !TEST_SCRIPT!
            )
        )
    )
) else (
    if exist "%TEST_DIR%\" (
        echo [INFO] Discovering tests from directory
        >> "%LOG_FILE%" echo [INFO] Discovering tests from directory
        type nul > "%RUN_TESTS_FILE%"

        for /f "delims=" %%T in ('dir /b /s /a-d "%TEST_DIR%\*.as" ^| sort') do (
            set "TEST_SCRIPT=%%T"
            set "REL_TEST_SCRIPT=!TEST_SCRIPT:%SCRIPTS_DIR%\=!"
            if /I not "!REL_TEST_SCRIPT!"=="!TEST_SCRIPT!" (
                set "REL_TEST_SCRIPT=!REL_TEST_SCRIPT:\=/!"
                >> "%RUN_TESTS_FILE%" echo !REL_TEST_SCRIPT!
            ) else (
                echo [ERROR] Test file is outside scripts directory: %%T
                >> "%LOG_FILE%" echo [ERROR] Test file is outside scripts directory: %%T
                copy /Y "%LOG_FILE%" "%LATEST_LOG%" >nul
                exit /b 1
            )
        )
    ) else (
        echo [ERROR] No test source found. Checked list "%TEST_LIST_FILE%" and directory "%TEST_DIR%"
        >> "%LOG_FILE%" echo [ERROR] No test source found. Checked list "%TEST_LIST_FILE%" and directory "%TEST_DIR%"
        copy /Y "%LOG_FILE%" "%LATEST_LOG%" >nul
        exit /b 1
    )
)

set "FOUND_TESTS="
for /f "usebackq delims=" %%T in ("%RUN_TESTS_FILE%") do (
    set "FOUND_TESTS=1"
    set "TEST_SCRIPT=%%T"

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

if not defined FOUND_TESTS (
    echo [ERROR] No tests found
    >> "%LOG_FILE%" echo [ERROR] No tests found
    copy /Y "%LOG_FILE%" "%LATEST_LOG%" >nul
    exit /b 1
)

echo [ OK ] All tests passed
>> "%LOG_FILE%" echo [ OK ] All tests passed
copy /Y "%LOG_FILE%" "%LATEST_LOG%" >nul
exit /b 0
