#!/bin/bash
# usage:
# ./autobuild/fullbuild.sh RelayTestApp relaytestapp/
# executable:
# relaytestapp/build/RelayTestApp

# export NINJA_COMMAND=/usr/bin/ninja
# export NINJA_COMMAND=/Volumes/CLion/CLion.app/Contents/bin/ninja/mac/ninja

pushd ${2}
rm -rf build
mkdir build
cd build
if [ "$NINJA_COMMAND" == "" ]; 
then
	cmake -DCMAKE_BUILD_TYPE=Debug ..
else
	cmake -GNinja -DCMAKE_MAKE_PROGRAM=${NINJA_COMMAND} -DCMAKE_BUILD_TYPE=Debug ..
fi
cmake --build . --target ${1} --config Debug
popd
