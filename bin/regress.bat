@echo off
rem Windows wrapper: the regression script is bash, so hand it over with the arguments
bash %~dp0regress %*
