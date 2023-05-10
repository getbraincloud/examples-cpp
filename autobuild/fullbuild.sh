NINJA_COMMAND=/Volumes/CLion/CLion.app/Contents/bin/ninja/mac/ninja

pushd ${2}
rm -rf build
mkdir build
cd build
cmake -GNinja -DCMAKE_MAKE_PROGRAM=${NINJA_COMMAND} -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target ${1} --config Debug
popd
