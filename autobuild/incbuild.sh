#!/bin/bash
pushd ${2}
cd build
cmake --build . --target ${1} --config Debug
popd
