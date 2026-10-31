@echo off
rem verify_lang.bat --- build and run the language resource checker
setlocal
set "ROOT=%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

cd /d "%~dp0"
cl /nologo /utf-8 /EHsc /O1 verify_lang.cpp /link /SUBSYSTEM:CONSOLE user32.lib || exit /b 1

set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=%ROOT%\build-x64\Typaste.exe"
verify_lang.exe "%TARGET%"
