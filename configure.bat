@echo off
rem ################################################################################
rem #
rem # Configure scipt for Petit-Ami
rem #
rem # Sets up the complete Petit-Ami project.
rem #
rem ################################################################################

rem
rem Set default variables
rem
set compiler=gcc
set bits=32
set host=windows

rem
rem Determine if needed programs exist. The only fatal one is grep, because we
rem need that to run this script. The rest will impact the running of various
rem test and build scripts.
rem

where /q grep || echo *** No grep was found
where /q diff || echo *** No diff was found
where /q sed || echo *** No sed was found
where /q rm || echo *** No rm was found
where /q cp || echo *** No cp was found
where /q mv || echo *** No mv was found
where /q flip || echo *** No flip was found
where /q ls || echo *** No ls was found
where /q gzip || echo *** No zip was found
where /q tar || echo *** No tar was found
where /q cpp || echo *** No cpp was found

rem
rem Now check for gcc. Output scary message for no compiler found, but
rem otherwise do nothing. rem Its up to the user to find a compiler.
rem
where /q gcc
if %errorlevel% neq 0 (

    echo *** No gcc compiler was found
    goto :compiler_check_done

)

set compiler=gcc

:compiler_check_done

rem
rem Check all arguments. Note that we don't attempt to check or fix bad choices
rem on the users part. We figure they know what they are doing.
rem

for %%x in (%*) do (

    if "%%x" == "--help" (

        echo "Configure program for Petit-AMi"
        exit /b 1

    ) else (

        echo *** Option not recognized
        echo Terminating
        exit /b 1

    )

)

echo Configure completed!
