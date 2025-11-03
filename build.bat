@echo off
setlocal enabledelayedexpansion

REM Resolve repo root (script is at root)
set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%" >nul
set "ROOT=%CD%"

REM CMake build directory
set "BUILD_BASE=%ROOT%\build"
set "BUILD_DIR=%BUILD_BASE%"

REM Configure Qt path if not provided
if not defined CMAKE_PREFIX_PATH (
    set "CMAKE_PREFIX_PATH=D:\Qt\6.9.3\mingw_64"
    echo [info] CMAKE_PREFIX_PATH not set. Using default: "!CMAKE_PREFIX_PATH!"
    echo        Edit build.bat to change this path if needed.
)

REM build dir will be finalized per generator below

REM Normalize Qt path to forward slashes for CMake and compose Qt6_DIR
set "QTPREFIX=%CMAKE_PREFIX_PATH%"
set "QTPREFIX_FWD=%QTPREFIX:\=/%"
set "QT6_DIR_FWD=%QTPREFIX_FWD%/lib/cmake/Qt6"

REM Detect Qt kit type (MinGW vs MSVC) from CMAKE_PREFIX_PATH
echo %CMAKE_PREFIX_PATH% | find /I "mingw" >nul
if %errorlevel%==0 goto use_mingw

REM Assume MSVC/VS Qt kit – try Visual Studio generator first
set "BUILD_DIR=%ROOT%\build-vs"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%" >nul 2>&1
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QTPREFIX_FWD%" -DQt6_DIR="%QT6_DIR_FWD%"
if errorlevel 1 goto try_nmake

echo [info] Configured with Visual Studio generator.
goto build

:use_mingw
echo [info] Detected MinGW Qt kit from CMAKE_PREFIX_PATH: "%CMAKE_PREFIX_PATH%"
call :setup_mingw
if errorlevel 1 goto mingw_toolchain_error
echo [info] Trying MinGW Makefiles generator...
set "BUILD_DIR=%ROOT%\build-mingw"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%" >nul 2>&1
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="%QTPREFIX_FWD%" -DQt6_DIR="%QT6_DIR_FWD%"
if errorlevel 1 goto error
echo [info] Configured with MinGW Makefiles generator.
goto build

:mingw_toolchain_error
echo [error] Could not locate MinGW toolchain (g++.exe / mingw32-make.exe). Ensure Qt Tools MinGW is installed and in PATH.
goto error

:try_nmake
REM Attempt to configure environment for NMake
call :setup_msvc
if errorlevel 1 (
    echo [error] Failed to locate MSVC toolchain for NMake.
    goto error
)

echo [info] Trying NMake Makefiles generator...
set "BUILD_DIR=%ROOT%\build-nmake"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%" >nul 2>&1
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "NMake Makefiles" -DCMAKE_PREFIX_PATH="%QTPREFIX_FWD%" -DQt6_DIR="%QT6_DIR_FWD%"
if errorlevel 1 goto error

echo [info] Configured with NMake generator.

:build
cmake --build "%BUILD_DIR%" --target differ --config Release
if errorlevel 1 goto error

echo [success] Build completed.
set "EXITCODE=0"
goto end

:setup_msvc
REM Find Visual Studio installation using vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    REM Try ProgramFiles if ProgramFiles(x86) is not defined (rare)
    set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
)
if not exist "%VSWHERE%" (
    echo [warn] vswhere not found. Skipping MSVC environment setup.
    exit /b 1
)
for /f "usebackq tokens=* delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINST=%%i"
if not defined VSINST (
    echo [warn] Visual Studio with C++ tools not found.
    exit /b 1
)
set "VCVARS64=%VSINST%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS64%" (
    echo [warn] vcvars64.bat not found at "%VCVARS64%".
    exit /b 1
)
call "%VCVARS64%"
exit /b 0

:setup_mingw
REM Try to locate MinGW bin folder and add to PATH (needed for g++ and mingw32-make)
set "MINGW_BIN="
REM 1) Common Qt Tools location relative to kit
for /d %%d in ("%CMAKE_PREFIX_PATH%\..\..\Tools\mingw*") do if exist "%%~fd\bin\g++.exe" set "MINGW_BIN=%%~fd\bin"
if defined MINGW_BIN goto found_mingw
REM 2) Kit bin itself (some distributions bundle toolchain here)
if exist "%CMAKE_PREFIX_PATH%\bin\g++.exe" set "MINGW_BIN=%CMAKE_PREFIX_PATH%\bin"
if defined MINGW_BIN goto found_mingw
REM 3) Typical global install paths (best-effort)
for /d %%d in ("%ProgramFiles%\mingw-w64*\*\mingw64\bin") do if exist "%%~fd\g++.exe" set "MINGW_BIN=%%~fd"
if not defined MINGW_BIN exit /b 1
:found_mingw
set "PATH=%MINGW_BIN%;%PATH%"
echo [info] Using MinGW bin: "%MINGW_BIN%"
REM Verify mingw32-make exists
where mingw32-make.exe >nul 2>&1
if errorlevel 1 echo [warn] mingw32-make.exe not found in PATH; CMake may try make. Ensure MinGW is correctly installed.
exit /b 0

:error
set "EXITCODE=1"

echo.
echo [hint] Ensure the following are installed and configured:
echo   - CMake 3.21+ in PATH
echo   - Qt 6.2+ and set CMAKE_PREFIX_PATH to its root (contains lib\cmake\Qt6)
echo   - If using MSVC: Visual Studio 2022 C++ toolchain (or run from VS Dev Cmd prompt)
echo   - If using MinGW: Qt Tools MinGW toolchain installed and in PATH (g++, mingw32-make)

echo [fail] Build failed.

:end
popd >nul
endlocal & exit /b %EXITCODE%