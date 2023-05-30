pipeline {
    agent none
    stages {

        stage('Build on Mac') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin::${env.PATH}"
  			}
            steps {
            	echo "Mac..."
            	sh 'git submodule update --init --recursive'
				sh '~/bin/setupexamplescpp.sh'
				sh 'autobuild/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
				sh 'autobuild/runtests.sh thirdparties/braincloud-cpp ${TEST_NAME}'
            }
            post {
	      		always {
    	    		junit 'build/tests/results.xml'
      			}
  			}	 
        }
        
//         stage('Build on Linux') {
//             agent { 
//                 label '"Linux Build Agent (.41)"'
//             }
//             steps { 
//             	echo 'Linux...'
//             }
//         }
//         
//         stage('Build on Windows') {
//             agent {
//                 label 'Windows Build Agent (.34)'
//             }
//             steps {
//             	echo "Windows..."
//             }
//         }
    }
}