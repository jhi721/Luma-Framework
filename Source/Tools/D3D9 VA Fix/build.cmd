@echo off
rem Builds the x86 d3d9.dll proxy into Binaries\Win32-Tools\D3D9 VA Fix\ (needs the VS x86 build tools).
setlocal
set "OUT=%~dp0..\..\..\Binaries\Win32-Tools\D3D9 VA Fix"
if not exist "%OUT%\obj" mkdir "%OUT%\obj"
call "C:\Program Files\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
cl /nologo /std:c++20 /EHsc /O2 /MT /W3 /LD "%~dp0d3d9_memlog.cpp" /Fo"%OUT%\obj\\" /Fe"%OUT%\d3d9.dll" /link psapi.lib user32.lib dxguid.lib > "%OUT%\build.log" 2>&1
set RESULT=%errorlevel%
echo build exit %RESULT% (log: %OUT%\build.log)
exit /b %RESULT%
