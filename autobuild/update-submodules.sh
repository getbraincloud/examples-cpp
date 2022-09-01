#!/bin/bash
if [[ $(git diff --compact-summary) ]];
then
    echo
    echo "Warning! Project has outstanding changes. Please commit or revert first."
    echo
    exit 1
fi

for i in thirdparties/braincloud-cpp roomserver/server/brainclouds2s-cpp
do
    echo
    STR=$(git config -f .gitmodules --get submodule.$i.branch)

    if [[ $(git submodule update --remote $i) ]];
    then
        git add $i
        git commit -m "automatic submodules update" .
        git push
        echo "--- $i updated on branch $STR"
    else
        echo "--- $i is already up to date"
    fi

done
echo
