#!/bin/bash

if [ -z $BRAINCLOUD_TOOLS ];
then
  export BRAINCLOUD_TOOLS=~/braincloud-client-master
fi

if ! [ -d ${BRAINCLOUD_TOOLS} ];
then
    echo "Error: Can't find brainCloud tools in path ${BRAINCLOUD_TOOLS}"
    exit 1
fi

if [ -z $1 ];
then
  if [ -z $SERVER_ENVIRONMENT ];
  then
    SERVER_ENVIRONMENT=internal
  fi
else
  SERVER_ENVIRONMENT=$1
fi

cd "`dirname "$0"`"/..
${BRAINCLOUD_TOOLS}/bin/setupexamplescpp.sh $SERVER_ENVIRONMENT

