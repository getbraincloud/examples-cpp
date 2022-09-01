#!/bin/bash
if [[ $(git diff --compact-summary) ]];
then
    echo
    echo "Warning! Project has outstanding changes. Please commit first."
    echo
    exit 1
fi

for i in thirdparties/braincloud-cpp roomserver/server/brainclouds2s-cpp
do

    git submodule update --remote $i

done


