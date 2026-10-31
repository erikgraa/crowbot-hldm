@echo off
rem Builds a 32-bit crowbot.dll (Half-Life is x86). Requires MSVC Build Tools.
setlocal
set VCVARS=
for %%E in (BuildTools Community Professional Enterprise) do (
  if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars32.bat"
)
if "%VCVARS%"=="" (echo vcvars32.bat not found & exit /b 1)
call "%VCVARS%" >nul || exit /b 1

cd /d "%~dp0"
if not exist build mkdir build
set INC=/Iinclude /Iinclude\metamod /Iinclude\hlsdk\common /Iinclude\hlsdk\dlls /Iinclude\hlsdk\engine /Iinclude\hlsdk\pm_shared /Iinclude\hlsdk\public

cl /nologo /LD /O2 /EHsc /MT /W3 /D_CRT_SECURE_NO_WARNINGS %INC% ^
  src\h_export.cpp src\meta_api.cpp src\dllapi.cpp src\engine_api.cpp src\bot.cpp src\public_amalgamation.cpp ^
  /Fobuild\   /Febuild\crowbot.dll /link /DEF:src\crowbot.def /MACHINE:X86 || exit /b 1
  echo Built build\crowbot.dll
