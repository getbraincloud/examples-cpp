pipeline {
    agent none
    stages {

        stage('HelloBC on Mac') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
  			}
            steps {
            	echo "Mac..."
				sh '~/bin/setupexamplescpp.sh'
				sh 'autobuild/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
				sh 'bash autobuild/runbuild.sh hellobc'
            }
        }
        
        stage('HelloBC on Linux') {
            agent { 
                label '"Linux Build Agent (.41)"'
            }
            environment {
			    PATH = "/usr/bin:${env.PATH}"
  			}
  			steps { 
            	echo 'Linux...'
				sh 'bash ~/bin/setupexamplescpp.sh'
				sh 'bash autobuild/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
				sh 'bash autobuild/runbuild.sh hellobc'
            }
        }
        
         stage('HelloBC on Windows') {
            agent {
                label 'Windows Build Agent (.34)'
            }
            steps {
            	echo "Windows..."
            	bat 'C:\\Users\\buildmaster\\bin\\setupexamplescpp.bat'
            	bat 'autobuild\\runbuild.bat hellobc'
            }
        }
        
        // end stages
    }
    // end pipeline
}