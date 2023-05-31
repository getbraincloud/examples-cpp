pipeline {
    agent none
    stages {

//         stage('Tests on Mac') {
//             agent {
//                 label 'clientUnit'
//             }
//             environment {
// 			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
//   			}
//             steps {
//             	echo "Mac..."
//             	sh 'git submodule update --init --recursive'
// 				sh '~/bin/setupexamplescpp.sh'
// 				sh 'autobuild/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
// 				sh 'autobuild/runtests.sh thirdparties/braincloud-cpp ${TEST_NAME}'
//             }
//             post {
// 	      		always {
//     	    		junit 'build/tests/results.xml'
//       			}
//   			}	 
//         }
//         
//         stage('Tests on Linux') {
//             agent { 
//                 label '"Linux Build Agent (.41)"'
//             }
//             environment {
// 			    PATH = "/usr/bin:${env.PATH}"
//   			}
//   			steps { 
//             	echo 'Linux...'
//             	sh 'git submodule update --init --recursive'
// 				sh 'bash ~/bin/setupexamplescpp.sh'
// 				sh 'bash autobuild/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
// 				sh 'bash autobuild/runtests.sh thirdparties/braincloud-cpp ${TEST_NAME}'
//             }
//             post {
// 	      		always {
//     	    		junit 'build/tests/results.xml'
//       			}
//   			}	 
//         }
        
         stage('Tests on Windows') {
            agent {
                label 'Windows Build Agent (.34)'
            }
            steps {
            	echo "Windows..."
            	bat 'git submodule update --init --recursive'
            	bat 'autobuild\\runtests.bat thirdparties\\braincloud-cpp %TEST_NAME%'
            }
            post {
	      		always {
    	    		junit 'build/tests/results.xml'
      			}
  			}	 
        }
    }
}