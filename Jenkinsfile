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
            }
        }
    }
}