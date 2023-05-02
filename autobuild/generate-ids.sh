while getopts p:s:a:k: flag
do
    case "${flag}" in
	p) project_source=${OPTARG};;
        s) serverurl=${OPTARG};;
        a) appid=${OPTARG};;
        k) secretkey=${OPTARG};;
    esac
done


if [ "$project_source" == "" ]; then
  echo "Must set arg -p project_source."
  exit 1
fi

echo "#pragma once" >$project_source/ids.h

echo "#define BRAINCLOUD_SERVER_URL \"${serverurl:-$BRAINCLOUD_SERVER_URL}\"" >>$project_source/ids.h
echo "#define BRAINCLOUD_APP_ID \"${appid:-$BRAINCLOUD_APP_ID}\"" >>$project_source/ids.h
echo "#define BRAINCLOUD_APP_SECRET \"${secretkey:-$BRAINCLOUD_APP_SECRET}\"" >>$project_source/ids.h

echo "-- File ids.h created in $project_source"
cat $project_source/ids.h

git update-index --assume-unchanged $project_source/ids.h
echo "-- File ids.h excluded from git worktree"

