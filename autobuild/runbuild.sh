pushd ${2}
rm -rf build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target ${1} --config Debug
./${1}
popd