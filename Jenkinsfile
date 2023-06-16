pipeline {
    agent none
    triggers {
        cron('H 1 * * 1-5') // nightly around 1 am
        pollSCM('H/5 * * * *') // check git every five minutes
    }
    parameters {
        string(name: 'BC_LIB', defaultValue: '', description: 'braincloud-cpp branch (blank for .gitmodules)')
        string(name: 'BRANCH_NAME', defaultValue: 'develop', description: 'examples-cpp branch')
        booleanParam(name: 'CLEAN_BUILD', defaultValue: true, description: 'clean pull and build')
    }
    stages {

        stage('Code Pull') {
            agent {
                label 'clientUnit'
            }
            steps {
                echo "---- braincloud Code Pull ${BRANCH_NAME} ${BC_LIB}"
                //if (${CLEAN_BUILD}) {
                    deleteDir()
                //}
                checkout([$class: 'GitSCM', branches: [[name: '*/${BRANCH_NAME}']], extensions: [[$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: false, recursiveSubmodules: true, reference: '', trackingSubmodules: false]], userRemoteConfigs: [[url: 'https://github.com/getbraincloud/examples-cpp.git']]])				
                sh 'autobuild/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
                sh '~/bin/setupexamplescpp.sh'
                sh 'cp ~/bin/test_ids_internal.txt thirdparties/braincloud-cpp/autobuild/ids.txt'
                sh 'cp ~/bin/test_ids_blank.txt thirdparties/braincloud-cpp/autobuild/ids-empty.txt'
            }
        }

        stage('HelloBC exe') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
  			}
            steps {
                sh '~/bin/setupexamplescpp.sh'
				sh 'bash autobuild/incbuild.sh hellobc'
				sh 'hellobc/build/hellobc'
            }
        }
           
        stage('RelayTestApp Build macOS') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
  			}
            steps {
				sh 'bash autobuild/incbuild.sh RelayTestApp relaytestapp'
            }
            post {
                success {
                    fileOperations([folderCreateOperation('rta-macos')])
                    fileOperations([fileCreateOperation(fileContent: '''Run \'Relay Test App\' on Mac OS.\nFirst, quarantine to allow permission:\nsudo xattr -r -d com.apple.quarantine relaytestapp\n''', fileName: 'rta-macos/README.md')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'relaytestapp/build/RelayTestApp', renameFiles: false, sourceCaptureExpression: '', targetLocation: 'rta-macos', targetNameExpression: '')])
                    fileOperations([fileZipOperation(folderPath: 'rta-macos', outputFolderPath: '')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'rta-macos.zip', renameFiles: false, sourceCaptureExpression: '', targetLocation: '~/Library/CloudStorage/GoogleDrive-joanneh@bitheads.com/Shared drives/brainCloud Team/Client/Builds', targetNameExpression: '')])
                    archiveArtifacts allowEmptyArchive: true, artifacts: 'rta-macos.zip', followSymlinks: false, onlyIfSuccessful: true
                }
            }
        }
                   
        stage('Android Build') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
                // this needs to be set for gradle and tools
			    ANDROID_HOME="/Users/buildmaster/Library/Android/sdk"
                // JAVA_HOME needs to set if JVM hasn't been downloaded to Android Studio
                // JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home/"
  			}
            steps {
                dir('android') {
                    sh './gradlew assembleDebug'
                }
            }
            post {
                success {
                    fileOperations([folderCreateOperation('android-cpp')])
                    fileOperations([fileCreateOperation(fileContent: 'Install this on a device or emulator.', fileName: 'android-cpp/README.md')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'android/app/build/outputs/apk/debug', renameFiles: false, sourceCaptureExpression: '', targetLocation: 'android-cpp', targetNameExpression: '')])
                    fileOperations([fileZipOperation(folderPath: 'android-cpp', outputFolderPath: '')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'android-cpp.zip', renameFiles: false, sourceCaptureExpression: '', targetLocation: '~/Library/CloudStorage/GoogleDrive-joanneh@bitheads.com/Shared drives/brainCloud Team/Client/Builds', targetNameExpression: '')])
                    archiveArtifacts allowEmptyArchive: true, artifacts: 'android-cpp.zip', followSymlinks: false, onlyIfSuccessful: true
                }
            }            
        }
        stage('Build Unit Tests') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
  			}
            steps {
                dir('thirdparties/braincloud-cpp') {
				    sh 'autobuild/buildtests.sh'
                }
            }
            post {
                success {
                    fileOperations([folderCreateOperation('bctests-macos')])
                    fileOperations([fileCreateOperation(fileContent: '''Run \'BC Tests\' on Mac OS.\nFirst, quarantine to allow permission:\nsudo xattr -r -d com.apple.quarantine relaytestapp\nThen, make sure to fill in ids.txt with your appId and secret key.''', fileName: 'bctests-macos/README.md')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'thirdparties/braincloud-cpp/build/tests/bctests', renameFiles: false, sourceCaptureExpression: '', targetLocation: 'bctests-macos', targetNameExpression: '')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'thirdparties/braincloud-cpp/autobuild/ids-empty.txt', renameFiles: false, sourceCaptureExpression: '', targetLocation: 'bctests-macos', targetNameExpression: '')])
                    fileOperations([fileZipOperation(folderPath: 'bctests-macos', outputFolderPath: '')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'bctests-macos.zip', renameFiles: false, sourceCaptureExpression: '', targetLocation: '~/Library/CloudStorage/GoogleDrive-joanneh@bitheads.com/Shared drives/brainCloud Team/Client/Builds', targetNameExpression: '')])
                    archiveArtifacts allowEmptyArchive: true, artifacts: 'bctests-macos.zip', followSymlinks: false, onlyIfSuccessful: true
                }
            }
        }
        
                
        stage('Package Apple Library') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
  			}
            steps {
                dir('thirdparties/braincloud-cpp') {
        		    sh 'autobuild/build_apple_unified.sh'
                }
            }
            post {
                success {
                    archiveArtifacts allowEmptyArchive: true, artifacts: 'thirdparties/braincloud-cpp/artifacts/*.*', followSymlinks: false, onlyIfSuccessful: true
                }
        	}
        }
                        
        stage('CocoaPod Verification') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
  			}
            steps {
                dir('thirdparties/braincloud-cpp') {
                    sh '~/bin/gemprepare.sh'
                    sh 'export LANG=en_US.UTF-8'
                    sh 'pod cache clean --all'
                    sh 'pod lib lint --use-libraries --allow-warnings --verbose'
                }
            }
        }
        // end stages
    }
    // end pipeline
}