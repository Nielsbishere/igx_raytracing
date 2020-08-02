@echo off
:loop

rem Go to folder

set dir=%~d1%~p1
set str=%~n1%~x1

cd /d "%dir%"

echo -- Compiling file "%str%" in dir "%dir%"

rem do compile

glslangValidator -G1.0 --target-env spirv1.0 -e main -o "%str%.spv" "%str%"
if %errorlevel% neq 0 exit /b %errorlevel%
echo -- Success compiling

rem do optimize

echo "%1.spv"
spirv-remap -v --do-everything --input "%str%.spv" --output "%CD%"
if %errorlevel% neq 0 exit /b %errorlevel%
echo -- Success remapping

rem do validate

spirv-val --target-env opengl4.5 --target-env spv1.0 "%str%.spv"
if %errorlevel% neq 0 exit /b %errorlevel%
echo -- Success validating

rem next

shift
if not "%~1"=="" goto loop