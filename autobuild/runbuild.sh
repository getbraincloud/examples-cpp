#!/bin/bash
./autobuild/incbuild.sh ${1} ${2:-$1}
./${2:-$1}/build/${1}
