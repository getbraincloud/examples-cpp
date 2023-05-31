@echo off
copy /Y c:\Users\buildmaster\bin\test-ids-internal.txt ..\%1\ids.txt
rmdir /S /Q build
mkdir build
pushd build
cmake -DSSL_ALLOW_SELFSIGNED=ON -DCMAKE_GENERATOR_PLATFORM=x64 -DBUILD_TESTS=ON ../%1
cmake --build . --target bctests --config Debug
tests\Debug\bctests.exe --test_output=all --gtest_output=xml:tests\results.xml --gtest_filter=*TestBC%2%*
popd
