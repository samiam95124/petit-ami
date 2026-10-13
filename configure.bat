@echo off
rem ############################################################################
rem #
rem # Configure script for Petit-Ami (Windows)
rem #
rem # Checks that the tools and the development libraries the build needs are
rem # on the machine, and installs the libraries that are missing. The build
rem # is by MinGW gcc, usually from MSYS2, and the libraries come from MSYS2's
rem # pacman: when pacman is on the path the missing ones are installed, and
rem # otherwise the pacman command to run is printed.
rem #
rem # The required set is what the default "make" target links against. The
rem # tests need nothing beyond it on Windows.
rem #
rem # Options:
rem #
rem # --check   Report what is missing and install nothing.
rem # --help    Print this.
rem #
rem ############################################################################

setlocal EnableDelayedExpansion

set check=0
for %%x in (%*) do (

    if "%%x" == "--check" (

        set check=1

    ) else if "%%x" == "--help" (

        echo Configure program for Petit-Ami
        echo.
        echo configure [--check]
        echo.
        echo Checks for the tools and development libraries the build needs and
        echo installs the missing libraries. --check reports and installs nothing.
        exit /b 0

    ) else (

        echo *** Option not recognized: %%x
        echo Terminating
        exit /b 1

    )

)

rem
rem Determine if needed programs exist. The only fatal one is gcc, because we
rem need that to compile Petit-Ami. The rest will impact the running of various
rem test and build scripts.
rem
set missingtools=
for %%t in (grep diff sed rm cp mv ls tar make ar) do (

    where /q %%t || (

        echo *** No %%t was found
        set missingtools=!missingtools! %%t

    )

)

where /q gcc
if %errorlevel% neq 0 (

    echo *** Aborting: cannot complete without a gcc compiler
    exit /b 1

)

rem
rem Find the MSYS2 package prefix from the compiler's target: i686 for a 32 bit
rem toolchain, x86_64 otherwise. A UCRT64 shell's packages are
rem mingw-w64-ucrt-x86_64-*, and the printed names want that prefix instead.
rem
set prefix=mingw-w64-x86_64
for /f "delims=" %%m in ('gcc -dumpmachine') do set machine=%%m
echo %machine% | findstr /b /c:"i686" >nul && set prefix=mingw-w64-i686

rem
rem The libraries the build needs: a header each, and the package it comes in.
rem Each header is probed by compiling an include of it, so the check is of
rem the compiler's own include path, whatever toolchain it is.
rem
set probe=%TEMP%\ami_configure_probe.c
set missingreq=
call :checkheader openssl/ssl.h %prefix%-openssl
call :checkheader png.h %prefix%-libpng
call :checkheader zlib.h %prefix%-zlib
if exist "%probe%" del "%probe%"

rem
rem Install the libraries that are missing, or say what they are
rem
if "%missingreq%" == "" (

    echo All required libraries are installed

) else (

    echo Required libraries not installed:%missingreq%
    where /q pacman
    if !errorlevel! neq 0 (

        echo Install them from an MSYS2 shell with:
        echo     pacman -S --needed%missingreq%

    ) else if "%check%" == "1" (

        echo Install them with:
        echo     pacman -S --needed%missingreq%

    ) else (

        pacman -S --needed --noconfirm%missingreq%

    )

)

if not "%missingreq%" == "" if "%check%" == "1" (

    echo Configure found missing libraries
    exit /b 1

)

echo Configure completed!
exit /b 0

rem
rem Check a header compiles: %1 the header, %2 the package that carries it.
rem Adds the package to the missing list when it does not.
rem
:checkheader
echo #include ^<%1^> > "%probe%"
gcc -fsyntax-only "%probe%" >nul 2>&1
if %errorlevel% neq 0 (

    echo *** No %1 was found: package %2
    set missingreq=%missingreq% %2

)
exit /b 0
