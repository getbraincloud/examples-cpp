				#!/bin/bash
				# Generate makefiles
				mkdir -p build
				pushd build
				rm -rf *	
				cmake -GNinja -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug ../${1:-thirdparties/braincloud-cpp}

				# Build
				cmake --build . --target bctests --config Debug -j 8

				cp ../${1:-thirdparties/braincloud-cpp}/ids.txt .

				# Run tests
				./tests/bctests --test_output=all --gtest_output=xml:tests/results.xml --gtest_filter=*TestBC${2}*
				
				popd