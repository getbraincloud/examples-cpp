#!/bin/bash

if [[ -d $1 ]];
then
	    pushd $1 
	    if [[ $(git diff --compact-summary .) ]];
    	then
			echo "Folder has mods"
		else
			git checkout $2
			git pull
		fi
		popd				
else
	echo "Folder not exists"
fi 
