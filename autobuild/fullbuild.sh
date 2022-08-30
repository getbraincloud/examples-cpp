pushd ${2}
rm -rf build
mkdir build
cd build
cmake -DSSL_ALLOW_SELFSIGNED=1 -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target ${1} --config Debug
popd
