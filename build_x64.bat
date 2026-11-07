@echo off
rem build_x64.bat --- build Typaste.exe as a x64 Release binary (MSVC, no CMake needed)
rem   Usage: build_x64.bat [Debug]
setlocal

set "ROOT=%~dp0"
set "CFG=%~1"
if "%CFG%"=="" set "CFG=Release"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] Visual Studio Installer was not found.
    exit /b 1
)

for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo [ERROR] The MSVC x64 toolset was not found.
    exit /b 1
)

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

cd /d "%ROOT%"
set "OUT=%ROOT%build-x64"
if not exist "%OUT%\obj" mkdir "%OUT%\obj"

if /I "%CFG%"=="Debug" (
    set "OPT=/Od /Zi /D_DEBUG"
) else (
    set "OPT=/O2 /DNDEBUG"
)

rc /nologo /fo "%OUT%\obj\Typaste_res.res" Typaste_res.rc || goto :error

cl /nologo /utf-8 /W4 /MT /EHsc /DUNICODE /D_UNICODE %OPT% ^
   /Fo"%OUT%\obj\\" /Fd"%OUT%\obj\\" /Fe"%OUT%\Typaste.exe" ^
   Typaste.cpp AutoType.cpp Lang.cpp Settings.cpp Config.cpp Clipboard.cpp Dpi.cpp Util.cpp ^
   /link /SUBSYSTEM:WINDOWS /MANIFEST:NO /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA ^
   user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib shlwapi.lib winmm.lib imm32.lib advapi32.lib ^
   "%OUT%\obj\Typaste_res.res" || goto :error

echo [OK] %OUT%\Typaste.exe
exit /b 0

:error
echo [ERROR] The build failed.
exit /b 1
