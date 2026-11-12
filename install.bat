@echo off
:: ================================================
::  Spectra - compilation script (Windows)
::  Epitech Paris - EIP 2025-2026
:: ================================================

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
set "CACHE_DIR=%SCRIPT_DIR%.cache"
set BUILD_PRESET="default"

if not exist %BUILD_DIR% mkdir %BUILD_DIR%
if not exist %CACHE_DIR% mkdir %CACHE_DIR%

if "%2"=="" set BUILD_PRESET="default"
if /I "%2"=="default" set BUILD_PRESET="default"
if /I "%2"=="release" set BUILD_PRESET="release"
if /I "%2"=="all-formats" set BUILD_PRESET="all-formats"
if /I "%2"=="dev" set BUILD_PRESET="dev"
if /I "%2"=="debug" set BUILD_PRESET="debug"
if /I "%2"=="-h" goto usage
if /I "%2"=="--help" goto usage

if "%1"=="" goto default
if /I "%1"=="all" goto default
if /I "%1"=="config" goto config
if /I "%1"=="build" goto build
if /I "%1"=="test" goto test
if /I "%1"=="re" goto rebuild
if /I "%1"=="-h" goto usage
if /I "%1"=="--help" goto usage
goto unknown

:usage
echo Usage: install.bat ACTION [PRESET]
echo ACTIONS
echo   all				config + build
echo   config			cmake config only
echo   build			cmake build only
echo   test             run automated tests
echo   test --coverage  run automated tests and show coverage
echo   re				cache delete + remake
echo PRESETS
echo   default			recommended (for dev or use)
echo   release			with standard release features
echo   all-formats		builds plugin in all availlable formats
echo   dev				most debug features
echo   debug			all debug features + performance profiling
goto end

:config
echo [INFO] Configuring cmake
cmake -DPROFILER_ENABLE_COVERAGE=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S %SCRIPT_DIR% -B "%BUILD_DIR%" -DPRESET_NAME=%BUILD_PRESET%
if errorlevel 1 (
    echo [ERROR] cmake configuration failed
    exit /b 1
)
echo [OK] cmake configured
exit /b 0

:ensure_normal_config
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    call :config
    exit /b !errorlevel!
)
findstr /C:"PROFILER_ENABLE_COVERAGE:BOOL=ON" "%BUILD_DIR%\CMakeCache.txt" >nul
if not errorlevel 1 (
    call :config
    exit /b !errorlevel!
)
exit /b 0

:ensure_coverage_config
if not exist "%BUILD_DIR%\CMakeCache.txt" goto configure_coverage
findstr /C:"PROFILER_ENABLE_COVERAGE:BOOL=ON" "%BUILD_DIR%\CMakeCache.txt" >nul
if errorlevel 1 goto configure_coverage
exit /b 0

:configure_coverage
echo [INFO] Configuring coverage
cmake -DPROFILER_BUILD_TESTS=ON -DPROFILER_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S "%~dp0." -B "%BUILD_DIR%"
if errorlevel 1 (
    echo [ERROR] Coverage configuration failed
    exit /b 1
)
exit /b 0

:build
call :ensure_normal_config
if errorlevel 1 exit /b 1
echo [INFO] Building project
cmake --build "%BUILD_DIR%" -j8
if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)
echo [OK] Done building project
goto end

:test
if /I "%2"=="--coverage" goto test_coverage
call :ensure_normal_config
if errorlevel 1 exit /b 1
echo [INFO] Building automated tests
cmake --build "%BUILD_DIR%" --target ProfilerTests --config Release
if errorlevel 1 (
    echo [ERROR] Test build failed
    exit /b 1
)
echo [INFO] Running automated tests
ctest --test-dir "%BUILD_DIR%" -C Release --output-on-failure
if errorlevel 1 (
    echo [ERROR] Tests failed
    exit /b 1
)
echo [INFO] Test summary
"%BUILD_DIR%\Tests\Release\ProfilerTests.exe" --quiet
if errorlevel 1 (
    echo [ERROR] Test summary failed
    exit /b 1
)
echo [OK] Tests passed
goto end

:test_coverage
call :ensure_coverage_config
if errorlevel 1 exit /b 1
echo [INFO] Running automated tests with coverage
cmake --build "%BUILD_DIR%" --target ProfilerCoverage --config Debug -- /m:1
if errorlevel 1 (
    echo [ERROR] Coverage tests failed
    exit /b 1
)
echo [OK] Coverage report: %BUILD_DIR%\coverage\html\index.html
goto end

:rebuild
echo [INFO] Cleaning build and cache directories
if exist "%BUILD_DIR%" (
    rmdir /s /q "%BUILD_DIR%"
    echo [OK] Removed build directory
)
if exist "%CACHE_DIR%" (
    rmdir /s /q "%CACHE_DIR%"
    echo [OK] Removed cache directory
)
call :config
if errorlevel 1 exit /b 1
call :build
if errorlevel 1 exit /b 1
goto end

:default
call :build
if errorlevel 1 exit /b 1
goto end

:unknown
echo [ERROR] Unknown option: %1
echo.
call :usage
exit /b 1

:end
endlocal
exit /b 0
