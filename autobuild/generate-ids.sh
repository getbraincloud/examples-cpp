while getopts p:s:a:k: flag
do
    case "${flag}" in
	p) project_source=${OPTARG};;
        s) serverurl=${OPTARG};;
        a) appid=${OPTARG};;
        k) secretkey=${OPTARG};;
    esac
done

cd $project_source

if [ "$project_source" == "" ]; then
  echo "Must set arg -p project_source."
  exit 1
fi

echo "#pragma once" >ids.h

echo "#define BRAINCLOUD_SERVER_URL \"${serverurl:-$BRAINCLOUD_SERVER_URL}\"" >>ids.h
echo "#define BRAINCLOUD_APP_ID \"${appid:-$BRAINCLOUD_APP_ID}\"" >>ids.h
echo "#define BRAINCLOUD_APP_SECRET \"${secretkey:-$BRAINCLOUD_APP_SECRET}\"" >>ids.h

echo "File ids.h created in $SRC_DIR"
cat ids.h
