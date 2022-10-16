cd ../%2
rmdir /S /Q build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target %1 --config Debug
cd ../../autobuild
