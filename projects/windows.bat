@echo off
REM
REM   Set VS vars and run a command
REM

if DEFINED VSINSTALLDIR GOTO :done

for %%e in (Professional, Community) do (
    for /l %%v in (2025, -1, 2017) do (
        set VS=%%v
        IF EXIST  "%PROGRAMFILES(x86)%\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvarsall.bat" call "%PROGRAMFILES(x86)%\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvarsall.bat" %1
        IF EXIST  "%PROGRAMFILES(x86)%\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvarsall.bat" goto :done
        IF EXIST "%PROGRAMFILES%\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvarsall.bat" call "%PROGRAMFILES(x86)%\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvarsall.bat" %1
        IF EXIST "%PROGRAMFILES%\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvarsall.bat" goto :done
    )
)

set e=
for /l %%v in (14, -1, 9) do (
    set VS=%%v
    IF EXIST "%PROGRAMFILES(x86)%\Microsoft Visual Studio %%v.0\VC\vcvarsall.bat" call "%PROGRAMFILES(x86)%\Microsoft Visual Studio %%v.0\VC\vcvarsall.bat" %1
    IF EXIST "%PROGRAMFILES(x86)%\Microsoft Visual Studio %%v.0\VC\vcvarsall.bat" goto :done
    IF EXIST "%PROGRAMFILES%\Microsoft Visual Studio %%v.0\VC\vcvarsall.bat" call "%PROGRAMFILES%\Microsoft Visual Studio %%v.0\VC\vcvarsall.bat" %1
    IF EXIST "%PROGRAMFILES%\Microsoft Visual Studio %%v.0\VC\vcvarsall.bat" goto :done
)

:done

@echo.
@echo Ran: "%PROGRAMFILES(x86)%\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvarsall.bat" %1
@echo.
@echo Using Visual Studio %VS% (v%VisualStudioVersion%) from %VSINSTALLDIR%
@echo.
@echo %2 %3 %4 %5 %6 %7 %8 %9
%2 %3 %4 %5 %6 %7 %8 %9
