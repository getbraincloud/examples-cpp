#!/bin/bash
./autobuild/fullbuild.sh ${1} ${2:-$1}
./${2:-$1}/build/${1}
