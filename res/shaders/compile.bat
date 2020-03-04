@echo off
:loop
glslangValidator -G1.3 --target-env spirv1.3 -Os -e main -o "%1.spv" "%1"
spirv-remap -v --do-everything --input "%1.spv" --output .
spirv-val --target-env opengl4.5 --target-env spv1.3 "%1.spv"
shift
if not "%~1"=="" goto loop