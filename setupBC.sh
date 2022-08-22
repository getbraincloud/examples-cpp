export BRAINCLOUD_SERVER_URL="https://api.internal.braincloudservers.com/dispatcherV2"
export BRAINCLOUD_APP_ID="20001"
export BRAINCLOUD_APP_SECRET="4e51b45c-030e-4f21-8457-dc53c9a0ed5f"

autobuild/generate-ids.sh -p android/app/src/main/cpp/
autobuild/generate-ids.sh -p bcchat/src
autobuild/generate-ids.sh -p hellobc
autobuild/generate-ids.sh -p relaytestapp/src
