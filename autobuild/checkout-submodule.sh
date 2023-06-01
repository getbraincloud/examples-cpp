#!/bin/bash

if [[ -d $1 ]];
then
		STR=$(git config -f .gitmodules --get submodule.$1.branch)
	    pushd $1 
	    if [[ $(git diff --compact-summary .) ]];
    	then
			echo "Folder has mods"
		else
			git checkout ${2:-$STR}
			git pull
		fi
		popd				
else
	echo "Folder not exists"
fi 
