#!/bin/sh

mode=RELEASE

if [ $1 == "-d" ] 
then
	mode=DEBUG 
	shift 1
fi

function compileVendor {

	echo -- Compiling file "$1" to "$2"

	glslangValidator -G1.0 --target-env spirv1.0 -DVENDOR_$vendor -D$mode -e main -o "$2" "$1"

	if [ $? -ne 0 ]; 
	then 
		exit $? 
	fi

	echo -- Success compiling "$mode"

	if [ $mode == "RELEASE" ] 
	then
		spirv-remap -v --opt all --map all --dce all --input "$(basename "$2")" --output $PWD
	else
		spirv-remap -v --do-everything --input "$(basename "$2")" --output $PWD
	fi
		
	if [ $? -ne 0 ]; 
	then 
		exit $? 
	fi

	echo -- Success remapping

	spirv-val --target-env opengl4.5 --target-env spv1.0 "$2"

	if [ $? -ne 0 ]; 
	then 
		exit $? 
	fi

	echo -- Success validating

}

for i in "$@" 
do

	file=$(basename "${i}")
	fileNoExt="${file%.*}"
	dir=$(dirname "${i}")
	
	previousDir=$PWD
	cd $(dirname ${i})

	if [[ $fileNoExt == *'.'* ]]; then

		start=$(echo "$file" | cut -d. -f1-1)
		output=$(echo "$file" | cut -d. -f2-)

		if [[ $start == *'nv'* ]]; then
			vendor=NV
			compileVendor "$file" "$output.nv.spv"
		fi

		if [[ $start == *'amd'* ]]; then
			vendor=AMD
			compileVendor "$file" "$output.amd.spv"
		fi

		if [[ $start == *'int'* ]]; then
			vendor=INT
			compileVendor "$file" "$output.int.spv"
		fi

		if [[ $start == *'arm'* ]]; then
			vendor=ARM
			compileVendor "$file" "$output.arm.spv"
		fi

		if [[ $start == *'all'* ]]; then
			vendor=ALL
			compileVendor "$file" "$output.spv"
		fi

	else
		vendor=ALL
		compileVendor "$file" "$file.spv"
	fi

	cd $previousDir

done
