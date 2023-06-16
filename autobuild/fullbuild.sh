#!/bin/bash
# usage:
# ./autobuild/fullbuild.sh RelayTestApp relaytestapp/
# executable:
# relaytestapp/build/RelayTestApp

pushd ${2:-$1}
rm -rf build
popd
./autobuild/incbuild.sh ${1} ${2:-$1}
