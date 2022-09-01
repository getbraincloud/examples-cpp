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

    STR=$(git config -f .gitmodules --get submodule.$i.branch)

    autobuild/verify-hash.sh $i $STR

    case $verify in
    0) echo "$i is up to date.";;
    1) git submodule update --remote $i
    esac
        #if [[ $(git submodule update --remote %i) ]]; then
        #    echo "UPDATED: thirdparties/braincloud-cpp had new commits."
        #fi
done


