#!/bin/sh

for i in "$@"
do
	glslangValidator -G1.0 --target-env spirv1.0 -e main -o "$i.spv" "$i"
	spirv-remap -v --do-everything --input "$i.spv" --output .
	spirv-val --target-env opengl4.5 --target-env spv1.0 "$i.spv"
	spirv-dis "$i.spv"
done