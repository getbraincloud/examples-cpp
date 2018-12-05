mkdir build
pushd build
call cmake -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION="10.0" -DCMAKE_GENERATOR_PLATFORM=x64 ..
popd
