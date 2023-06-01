@echo off

pushd %1

echo #pragma once >ids.h

echo #define BRAINCLOUD_SERVER_URL "%2" >>ids.h
echo #define BRAINCLOUD_APP_ID "%3" >>ids.h
echo #define BRAINCLOUD_APP_SECRET "%4" >>ids.h

echo File ids.h created in %1
:: type ids.h

git update-index --skip-worktree ids.h
echo File ids.h excluded from git worktree

popd
