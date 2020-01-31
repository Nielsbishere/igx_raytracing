@echo off
:loop
glslangValidator -V -q -Os -e main -o "%1.spv" "%1"
spirv-remap -v --do-everything --input "%1.spv" --output .
spirv-val --target-env opengl4.5 "%1.spv"
shift
if not "%~1"=="" goto loop