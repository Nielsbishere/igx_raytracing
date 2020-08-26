#!/bin/sh

debug=n

if [ $1 == "-d" ] 
then
	debug=y 
	shift 1
fi

for i in "$@" 
do

	echo -- Compiling file "$i"

	glslangValidator -G1.0 --target-env spirv1.0 -e main -o "$i.spv" "$i"

	if [ $? -ne 0 ]; 
	then 
		exit $? 
	fi

	echo -- Success compiling

	if [ $debug == "n" ] 
	then
		spirv-remap -v --opt all --map all --dce all --input "$(basename "${i}").spv" --output $(dirname "${i}")
	else
		spirv-remap -v --do-everything --input "$(basename "${i}").spv" --output $(dirname "${i}")
	fi
		
	if [ $? -ne 0 ]; 
	then 
		exit $? 
	fi

	echo -- Success remapping

	spirv-val --target-env opengl4.5 --target-env spv1.0 "$i.spv"

	if [ $? -ne 0 ]; 
	then 
		exit $? 
	fi

	echo -- Success validating

done