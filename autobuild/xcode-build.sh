#!/bin/bash
pushd ${2:-$1}
mkdir -p build
cd build

cmake -G Xcode ..
xcodebuild -scheme hellobc build

popd