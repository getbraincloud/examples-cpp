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
				sh 'autobuild/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
				sh 'autobuild/runtests.sh thirdparties/braincloud-cpp ${TEST_NAME}'
            }
        }
    }
}