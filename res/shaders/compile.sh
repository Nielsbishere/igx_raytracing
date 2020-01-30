#!/bin/sh

for i in "$@"
do
	glslangValidator -V -q -Os -e main -o "$i.spv" "$i"
	spirv-remap -v --do-everything --input "$i.spv" --output .
	spirv-val --target-env opengl4.5 "$i.spv"
	spirv-dis "$i.spv"
done