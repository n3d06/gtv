@echo off
rem Build gotiengviet (x64, MSVC) va chay toan bo test.
setlocal
pushd "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
for /f "delims=" %%i in ('.\vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set "VSDIR=%%i"
popd
if not defined VSDIR (echo Khong tim thay MSVC & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul || exit /b 1

cd /d "%~dp0"
if not exist build\test mkdir build\test
set CFLAGS=/nologo /utf-8 /W4 /WX
set NOCRT=/O1 /GS- /Zl
set SRC=telex engine hook emit log main selftest tray ui settings listedit autostart datafile exclude macro update nocrt
set LIBS=kernel32.lib user32.lib gdi32.lib shell32.lib advapi32.lib comctl32.lib uxtheme.lib dwmapi.lib winhttp.lib
rem Phien ban trong exe (de kiem tra cap nhat); khong co GTV_VERSION thi la 0.0.0-dev
set VERDEF=
if defined GTV_VERSION set VERDEF=/DGTV_VERSION=\"%GTV_VERSION%\"
rem Common Controls 6: nut va o chon theo giao dien Windows 10/11
set MANIFEST=/MANIFEST:EMBED /MANIFESTDEPENDENCY:"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"

rem ---- Test (co CRT, chi de chay test) ----
cl %CFLAGS% /O2 /Fo:build\test\ /Fe:build\test\test_telex.exe src\telex.c tests\test_telex.c || exit /b 1
cl %CFLAGS% /O2 /Fo:build\test\ /Fe:build\test\test_engine.exe src\telex.c src\engine.c tests\test_engine.c user32.lib advapi32.lib || exit /b 1
cl %CFLAGS% /O2 /Fo:build\test\ /Fe:build\test\test_exclude.exe src\datafile.c src\exclude.c tests\test_exclude.c shell32.lib user32.lib || exit /b 1
cl %CFLAGS% /O2 /Fo:build\test\ /Fe:build\test\test_macro.exe src\datafile.c src\macro.c tests\test_macro.c shell32.lib user32.lib || exit /b 1
cl %CFLAGS% /O2 /Fo:build\test\ /Fe:build\test\test_update.exe src\update.c tests\test_update.c winhttp.lib shell32.lib user32.lib || exit /b 1
build\test\test_telex.exe || exit /b 1
build\test\test_engine.exe || exit /b 1
build\test\test_exclude.exe || exit /b 1
build\test\test_macro.exe || exit /b 1
build\test\test_update.exe || exit /b 1

rem Loi Telex phai link duoc khi khong co CRT
cl %CFLAGS% %NOCRT% /c /Fo:build\test\ src\telex.c tests\nocrt_check.c || exit /b 1
link /nologo /NODEFAULTLIB /ENTRY:entry /SUBSYSTEM:CONSOLE /OUT:build\test\nocrt_check.exe build\test\nocrt_check.obj build\test\telex.obj || exit /b 1
build\test\nocrt_check.exe || (echo nocrt_check that bai & exit /b 1)

rem ---- Exe bo go: khong CRT, subsystem Windows ----
set OBJS=
set CSRC=
for %%f in (%SRC%) do call set "CSRC=%%CSRC%% src\%%f.c" & call set "OBJS=%%OBJS%% build\%%f.obj"
cl %CFLAGS% %NOCRT% %VERDEF% /c /Fo:build\ %CSRC% || exit /b 1
rc /nologo /fo build\gotiengviet.res src\gotiengviet.rc || exit /b 1
link /nologo /NODEFAULTLIB /ENTRY:start /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF %MANIFEST% /OUT:build\gotiengviet.exe %OBJS% build\gotiengviet.res %LIBS% || exit /b 1
echo Da build build\gotiengviet.exe

rem ---- Ban cai (neu da cai Inno Setup 6) ----
set "ISCC="
for %%p in ("%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" "%ProgramFiles%\Inno Setup 6\ISCC.exe" "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe") do if exist %%p set "ISCC=%%~p"
if not defined ISCC (echo Bo qua ban cai: chua cai Inno Setup 6 & exit /b 0)
set "ISSDEF="
if defined GTV_VERSION set "ISSDEF=/DAppVersion=%GTV_VERSION%"
"%ISCC%" /Q %ISSDEF% installer\gotiengviet.iss || exit /b 1
echo Da build build\installer
