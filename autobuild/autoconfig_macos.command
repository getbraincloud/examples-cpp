#!/bin/bash
# run this in your project root folder

if [ -z "$BRAINCLOUD_TOOLS" ];
then
  export BRAINCLOUD_TOOLS=~/braincloud-client-master
fi

if ! [ -d "${BRAINCLOUD_TOOLS}/bin" ];
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

cd "`dirname "$0"`"

export WORKSPACE=$PWD

${BRAINCLOUD_TOOLS}/bin/copy-ids.sh -o android/app/src/main/cpp -p clientapp -x h -s $SERVER_ENVIRONMENT
${BRAINCLOUD_TOOLS}/bin/copy-ids.sh -o bcchat/src -p bcchat -x h -s $SERVER_ENVIRONMENT
${BRAINCLOUD_TOOLS}/bin/copy-ids.sh -o hellobc -p clientapp -x h -s $SERVER_ENVIRONMENT
${BRAINCLOUD_TOOLS}/bin/copy-ids.sh -o relaytestapp/src -p relaytestapp -x h -s $SERVER_ENVIRONMENT
${BRAINCLOUD_TOOLS}/bin/copy-ids.sh -o roomserver/client -p roomserver -x h -s prod
${BRAINCLOUD_TOOLS}/bin/copy-ids.sh -o thirdparties/braincloud-cpp/autobuild -p test -x txt -s $SERVER_ENVIRONMENT

if [[ $2 != -nodev ]];
then
    git update-index --assume-unchanged android/app/src/main/cpp/ids.h
    git update-index --assume-unchanged bcchat/src/ids.h
    git update-index --assume-unchanged hellobc/ids.h
    git update-index --assume-unchanged relaytestapp/src/ids.h
    echo
    echo "No worries, your secret config files have been excluded from git worktree."
    echo
fi