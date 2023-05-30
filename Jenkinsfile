pipeline {
    agent none
    stages {
        stage('Build on Linux') {
            agent { 
                label '"Linux Build Agent (.41)"'
            }
            steps { 
            	echo 'Linux...'
            }
        }
        stage('Build on Windows') {
            agent {
                label 'Windows Build Agent (.34)'
            }
            steps {
            	echo "Windows..."
            }
        }
        stage('Build on Mac') {
            agent {
                label 'clientUnit'
            }
            steps {
            	echo "Mac..."
				sh '~/bin/setupexamplescpp.sh'
				
				# Generate makefiles
				mkdir -p build
				cd build
				rm -rf *	
				cmake -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug ../thirdparties/braincloud-cpp/

				# Build
				cmake --build . --target bctests --config Debug

				cp ../thirdparties/braincloud-cpp/ids.txt .

				# Run tests
				./bctests --test_output=all --gtest_output=xml:results.xml --gtest_filter=*TestBC${TEST_NAME}*
            }
        }
    }
}