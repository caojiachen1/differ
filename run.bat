@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%" >nul
set "ROOT=%CD%"
REM Attempt to locate the executable across known build dirs
set "BUILD_VS=%ROOT%\build-vs"
set "BUILD_NM=%ROOT%\build-nmake"
set "BUILD_MG=%ROOT%\build-mingw"
set "BUILD_DEF=%ROOT%\build"

set "EXE_VS=%BUILD_VS%\bin\Release\differ.exe"
set "EXE_NMAKE=%BUILD_NM%\bin\differ.exe"
set "EXE_MINGW=%BUILD_MG%\bin\differ.exe"
set "EXE_DEF_VS=%BUILD_DEF%\bin\Release\differ.exe"
set "EXE_DEF_NM=%BUILD_DEF%\bin\differ.exe"

if exist "%EXE_VS%" (
    echo [info] Running %EXE_VS%
    "%EXE_VS%"
    set "EXITCODE=%ERRORLEVEL%"
    goto end
)

if exist "%EXE_NMAKE%" (
    echo [info] Running %EXE_NMAKE%
    "%EXE_NMAKE%"
    set "EXITCODE=%ERRORLEVEL%"
    goto end
)

if exist "%EXE_MINGW%" (
    echo [info] Running %EXE_MINGW%
    "%EXE_MINGW%"
    set "EXITCODE=%ERRORLEVEL%"
    goto end
)

echo [info] Executable not found. Building first...
call "%ROOT%\build.bat"
if errorlevel 1 goto error

if exist "%EXE_VS%" (
    "%EXE_VS%"
    set "EXITCODE=%ERRORLEVEL%"
    goto end
)
if exist "%EXE_NMAKE%" (
    "%EXE_NMAKE%"
    set "EXITCODE=%ERRORLEVEL%"
    goto end
)
if exist "%EXE_MINGW%" (
    "%EXE_MINGW%"
    set "EXITCODE=%ERRORLEVEL%"
    goto end
)
if exist "%EXE_DEF_VS%" (
    "%EXE_DEF_VS%"
    set "EXITCODE=%ERRORLEVEL%"
    goto end
)
if exist "%EXE_DEF_NM%" (
    "%EXE_DEF_NM%"
    set "EXITCODE=%ERRORLEVEL%"
    goto end
)

echo [error] Executable still not found after build.
set "EXITCODE=1"
goto end

:error
set "EXITCODE=1"

echo.
echo [hint] Try running build.bat manually to see detailed errors.

echo [fail] Run failed.

:end
popd >nul
endlocal & exit /b %EXITCODE%
