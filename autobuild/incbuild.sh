#!/bin/bash
pushd ${2:-$1}
mkdir -p build
pushd build
if [[ $(command -v ninja) ]];
then
	cmake -DCMAKE_BUILD_TYPE=Debug ..
else

	# make sure ninja is in your path OR use 
	# -DCMAKE_MAKE_PROGRAM=${NINJA_COMMAND}
	cmake -GNinja  -DCMAKE_BUILD_TYPE=Debug ..
fi
cmake --build . --target ${1} --config Debug
popd
popd