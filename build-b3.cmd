@echo off
rem build-b3.cmd -- build the B3 worktree with the upstream recipe, into build-b3\.
rem Usage: build-b3.cmd [config-dir]
setlocal
set HERE=%~dp0
set BUILDDIR=%~1
if "%BUILDDIR%"=="" set BUILDDIR=%HERE%build-b3
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo VCVARS-FAILED & exit /b 1 )
cmake -S "%HERE%." -B "%BUILDDIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 ( echo CONFIGURE-FAILED & exit /b 1 )
cmake --build "%BUILDDIR%"
if errorlevel 1 ( echo BUILD-FAILED & exit /b 1 )
echo BUILD-OK
