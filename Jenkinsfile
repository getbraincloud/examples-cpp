pipeline {
    agent none
    triggers {
        cron('H 1 * * 1-5') // nightly around 1 am
        // pollSCM('H/5 * * * *') // check git every five minutes
    }
    parameters {
        string(name: 'BC_LIB', defaultValue: '', description: 'braincloud-cpp branch (blank for .gitmodules)')
        string(name: 'BRANCH_NAME', defaultValue: 'develop', description: 'examples-cpp branch')
        choice(name: 'SERVER_ENV', choices: ['internal', 'prod', 'talespin'], description: 'Where to run tests?')
    }
    stages {

        stage('HelloBC Exe Mac') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
   			    BRAINCLOUD_TOOLS="/Users/buildmaster/braincloud-client-master"
  			}
            steps {
                deleteDir()
                checkout([$class: 'GitSCM', branches: [[name: '*/${BRANCH_NAME}']], extensions: [[$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: false, recursiveSubmodules: true, reference: '', trackingSubmodules: false]], userRemoteConfigs: [[url: 'https://github.com/getbraincloud/examples-cpp.git']]])
                sh '${BRAINCLOUD_TOOLS}/bin/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
                sh "${BRAINCLOUD_TOOLS}/bin/copy-ids.sh -o hellobc -p clientapp -x h -s ${params.SERVER_ENV}"
				sh 'bash autobuild/incbuild.sh hellobc'
				sh 'hellobc/build/hellobc'
            }
        }

        stage('HelloBC Exe Linux') {
            agent {
                label '"Linux Build Agent (.41)"'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
   			    BRAINCLOUD_TOOLS="/home/buildmaster/braincloud-client-master"
  			}
            steps {
                deleteDir()
                checkout([$class: 'GitSCM', branches: [[name: '*/${BRANCH_NAME}']], extensions: [[$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: false, recursiveSubmodules: true, reference: '', trackingSubmodules: false]], userRemoteConfigs: [[url: 'https://github.com/getbraincloud/examples-cpp.git']]])
                sh '${BRAINCLOUD_TOOLS}/bin/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
                sh "${BRAINCLOUD_TOOLS}/bin/copy-ids.sh -o hellobc -p clientapp -x h -s ${params.SERVER_ENV}"
				sh 'autobuild/incbuild.sh hellobc'
				sh 'hellobc/build/hellobc'
            }
        }

        stage('HelloBC Exe Windows') {
            agent {
                 label 'windows'
            }
            steps {
                deleteDir()
                checkout([$class: 'GitSCM', branches: [[name: '*/${BRANCH_NAME}']], extensions: [[$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: false, recursiveSubmodules: true, reference: '', trackingSubmodules: false]], userRemoteConfigs: [[url: 'https://github.com/getbraincloud/examples-cpp.git']]])
                bat 'C:\\Users\\buildmaster\\braincloud-client-master\\bin\\checkout-submodule.bat thirdparties/braincloud-cpp %BC_LIB%'
                bat "copy /Y C:\\Users\\buildmaster\\braincloud-client-master\\data\\clientapp_ids_internal.h hellobc\\ids.h"
            	bat 'autobuild\\fullbuild.bat hellobc hellobc'
            	bat 'hellobc\\build\\debug\\hellobc.exe'
            }
        }

        stage('RelayTestApp Build Mac') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
			    BRAINCLOUD_TOOLS="/Users/buildmaster/braincloud-client-master"
			}
            steps {
                checkout([$class: 'GitSCM', branches: [[name: '*/${BRANCH_NAME}']], extensions: [[$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: false, recursiveSubmodules: true, reference: '', trackingSubmodules: false]], userRemoteConfigs: [[url: 'https://github.com/getbraincloud/examples-cpp.git']]])
                sh '${BRAINCLOUD_TOOLS}/bin/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
                sh "${BRAINCLOUD_TOOLS}/bin/copy-ids.sh -o relaytestapp/src -p relaytestapp -x h -s ${params.SERVER_ENV}"
				sh 'bash autobuild/incbuild.sh RelayTestApp relaytestapp'
            }
            post {
                success {
                    fileOperations([folderCreateOperation('artifacts/rta-macos')])
                    //fileOperations([fileCreateOperation(fileContent: '''Run \'Relay Test App\' on Mac OS.\nFirst, quarantine to allow permission:\nsudo xattr -r -d com.apple.quarantine relaytestapp\n''', fileName: 'artifacts/rta-macos/README.md')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: '${BRAINCLOUD_TOOLS}/bin/quarantine.command', renameFiles: false, sourceCaptureExpression: '', targetLocation: 'artifacts/rta-macos', targetNameExpression: '')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'relaytestapp/build/RelayTestApp', renameFiles: false, sourceCaptureExpression: '', targetLocation: 'artifacts/rta-macos', targetNameExpression: '')])                    
                    fileOperations([fileZipOperation(folderPath: 'artifacts/rta-macos', outputFolderPath: 'artifacts')])
                    //fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'artifacts/rta-macos.zip', renameFiles: false, sourceCaptureExpression: '', targetLocation: '~/Library/CloudStorage/GoogleDrive-joanneh@bitheads.com/Shared drives/brainCloud Team/Client/Builds', targetNameExpression: '')])
                    archiveArtifacts allowEmptyArchive: true, artifacts: 'artifacts/rta-macos.zip', followSymlinks: false, onlyIfSuccessful: true
                }
            }
        }

        stage('RelayTestApp Build Linux') {
            agent {
                label '"Linux Build Agent (.41)"'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
   			    BRAINCLOUD_TOOLS="/home/buildmaster/braincloud-client-master"
  			}
            steps {
                checkout([$class: 'GitSCM', branches: [[name: '*/${BRANCH_NAME}']], extensions: [[$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: false, recursiveSubmodules: true, reference: '', trackingSubmodules: false]], userRemoteConfigs: [[url: 'https://github.com/getbraincloud/examples-cpp.git']]])
                sh '${BRAINCLOUD_TOOLS}/bin/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
                sh "${BRAINCLOUD_TOOLS}/bin/copy-ids.sh -o relaytestapp/src -p relaytestapp -x h -s ${params.SERVER_ENV}"
				sh 'bash autobuild/incbuild.sh RelayTestApp relaytestapp'
            }
            post {
                success {
                    fileOperations([folderCreateOperation('artifacts/rta-macos')])
                    //fileOperations([fileCreateOperation(fileContent: '''Run \'Relay Test App\' on Mac OS.\nFirst, quarantine to allow permission:\nsudo xattr -r -d com.apple.quarantine relaytestapp\n''', fileName: 'artifacts/rta-macos/README.md')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: '${BRAINCLOUD_TOOLS}/bin/quarantine.command', renameFiles: false, sourceCaptureExpression: '', targetLocation: 'artifacts/rta-macos', targetNameExpression: '')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'relaytestapp/build/RelayTestApp', renameFiles: false, sourceCaptureExpression: '', targetLocation: 'artifacts/rta-macos', targetNameExpression: '')])
                    fileOperations([fileZipOperation(folderPath: 'artifacts/rta-macos', outputFolderPath: 'artifacts')])
                    //fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'artifacts/rta-macos.zip', renameFiles: false, sourceCaptureExpression: '', targetLocation: '~/Library/CloudStorage/GoogleDrive-joanneh@bitheads.com/Shared drives/brainCloud Team/Client/Builds', targetNameExpression: '')])
                    archiveArtifacts allowEmptyArchive: true, artifacts: 'artifacts/rta-macos.zip', followSymlinks: false, onlyIfSuccessful: true
                }
            }
        }

        stage('RelayTestApp Build Windows') {
            agent {
                 label 'windows'
            }
            steps {
                deleteDir()
                checkout([$class: 'GitSCM', branches: [[name: "*/${BRANCH_NAME}"]], extensions: [[$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: false, recursiveSubmodules: true, reference: '', trackingSubmodules: false]], userRemoteConfigs: [[url: 'https://github.com/getbraincloud/examples-cpp.git']]])
                bat '%BRAINCLOUD_TOOLS%\\bin\\checkout-submodule.bat thirdparties/braincloud-cpp ${BC_LIB}'
                // todo: use server param in ids filename
                bat "copy /Y C:\\Users\\buildmaster\\braincloud-client-master\\data\\relaytestapp_ids_internal.h hellobc\\ids.h"
            	bat 'autobuild\\fullbuild.bat RelayTestApp relaytestapp'
            }
            // todo: archive windows app
        }

        stage('Android Build') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
                // ANDROID_HOME needs to be set for gradle and tools
			    ANDROID_HOME="/Users/buildmaster/Library/Android/sdk"
                // JAVA_HOME needs to set if JVM hasn't been downloaded to Android Studio
                // JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home/"
			    BRAINCLOUD_TOOLS="/Users/buildmaster/braincloud-client-master"
  			}
            steps {
                checkout([$class: 'GitSCM', branches: [[name: '*/${BRANCH_NAME}']], extensions: [[$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: false, recursiveSubmodules: true, reference: '', trackingSubmodules: false]], userRemoteConfigs: [[url: 'https://github.com/getbraincloud/examples-cpp.git']]])
                sh '${BRAINCLOUD_TOOLS}/bin/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
                sh "${BRAINCLOUD_TOOLS}/bin/copy-ids.sh -o android/app/src/main/cpp -p clientapp -x h -s ${params.SERVER_ENV}"
                dir('android') {
                    sh './gradlew assembleDebug'
                }
            }
            post {
                success {
                    fileOperations([folderCreateOperation('artifacts/android-cpp')])
                    fileOperations([fileCreateOperation(fileContent: 'Install this on a device or emulator.', fileName: 'artifacts/android-cpp/README.md')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'android/app/build/outputs/apk/debug/*', renameFiles: false, sourceCaptureExpression: '', targetLocation: 'artifacts/android-cpp', targetNameExpression: '')])
                    fileOperations([fileZipOperation(folderPath: 'artifacts/android-cpp', outputFolderPath: 'artifacts')])
                    //fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'artifacts/android-cpp.zip', renameFiles: false, sourceCaptureExpression: '', targetLocation: '~/Library/CloudStorage/GoogleDrive-joanneh@bitheads.com/Shared drives/brainCloud Team/Client/Builds', targetNameExpression: '')])
                    archiveArtifacts allowEmptyArchive: true, artifacts: 'artifacts/android-cpp.zip', followSymlinks: false, onlyIfSuccessful: true
                }
            }
        }
        
        stage('Build Unit Tests Mac') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
			    BRAINCLOUD_TOOLS="/Users/buildmaster/braincloud-client-master"
  			}
            steps {
                checkout([$class: 'GitSCM', branches: [[name: '*/${BRANCH_NAME}']], extensions: [[$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: false, recursiveSubmodules: true, reference: '', trackingSubmodules: false]], userRemoteConfigs: [[url: 'https://github.com/getbraincloud/examples-cpp.git']]])
                sh '${BRAINCLOUD_TOOLS}/bin/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
                sh "cp $BRAINCLOUD_TOOLS/data/test_ids_${params.SERVER_ENV}.txt thirdparties/braincloud-cpp/autobuild/ids.txt"
                sh 'cp $BRAINCLOUD_TOOLS/data/test_ids_blank.txt thirdparties/braincloud-cpp/autobuild/ids-empty.txt'
                dir('thirdparties/braincloud-cpp') {
				    sh 'autobuild/buildtests.sh'
                }
            }
            post {
                success {
                    fileOperations([folderCreateOperation('artifacts/bctests-macos')])
                    fileOperations([fileCreateOperation(fileContent: '''Run \'BC Tests\' on Mac OS.\nFirst, quarantine to allow permission:\nsudo xattr -r -d com.apple.quarantine bctests\nMake sure the product is executable:\nchmod a+x\nFill in ids.txt with your appIds and secret keys.\n\nUsage: ./bctests --test_output=all --gtest_output=xml:results.xml --gtest_filter=*TestBCAuth*\n''', fileName: 'artifacts/bctests-macos/README.md')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: '~/braincloud-client-master/bin/quarantine.command', renameFiles: false, sourceCaptureExpression: '', targetLocation: 'artifacts/rta-macos', targetNameExpression: '')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'thirdparties/braincloud-cpp/build/tests/bctests', renameFiles: false, sourceCaptureExpression: '', targetLocation: 'artifacts/bctests-macos', targetNameExpression: '')])
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'thirdparties/braincloud-cpp/autobuild/ids-empty.txt', renameFiles: false, sourceCaptureExpression: '', targetLocation: 'artifacts/bctests-macos', targetNameExpression: '')])
                    fileOperations([fileZipOperation(folderPath: 'artifacts/bctests-macos', outputFolderPath: 'artifacts')])
                    //fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'artifacts/bctests-macos.zip', renameFiles: false, sourceCaptureExpression: '', targetLocation: '~/Library/CloudStorage/GoogleDrive-joanneh@bitheads.com/Shared drives/brainCloud Team/Client/Builds', targetNameExpression: '')])
                    archiveArtifacts allowEmptyArchive: true, artifacts: 'artifacts/bctests-macos.zip', followSymlinks: false, onlyIfSuccessful: true
                }
            }
        }

        stage('CocoaPod Verification') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
			    BRAINCLOUD_TOOLS="/Users/buildmaster/braincloud-client-master"
  			}
            steps {
                checkout([$class: 'GitSCM', branches: [[name: '*/${BRANCH_NAME}']], extensions: [[$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: false, recursiveSubmodules: true, reference: '', trackingSubmodules: false]], userRemoteConfigs: [[url: 'https://github.com/getbraincloud/examples-cpp.git']]])
                sh '${BRAINCLOUD_TOOLS}/bin/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
                dir('thirdparties/braincloud-cpp') {
                    sh '${BRAINCLOUD_TOOLS}/data/gemprepare.sh'
        		    sh 'autobuild/podlint.sh'
                }
            }
        }

        stage('Package Library Mac') {
            agent {
                label 'clientUnit'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
			    BRAINCLOUD_TOOLS="/Users/buildmaster/braincloud-client-master"
  			}
            steps {
                checkout([$class: 'GitSCM', branches: [[name: '*/${BRANCH_NAME}']], extensions: [[$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: false, recursiveSubmodules: true, reference: '', trackingSubmodules: false]], userRemoteConfigs: [[url: 'https://github.com/getbraincloud/examples-cpp.git']]])
                sh '${BRAINCLOUD_TOOLS}/bin/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
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

        stage('Package Library Linux') {
            agent {
                label '"Linux Build Agent (.41)"'
            }
            environment {
			    PATH = "/Applications/CMake.app/Contents/bin:/usr/local/bin:${env.PATH}"
   			    BRAINCLOUD_TOOLS="/home/buildmaster/braincloud-client-master"
  			}
            steps {
                checkout([$class: 'GitSCM', branches: [[name: '*/${BRANCH_NAME}']], extensions: [[$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: false, recursiveSubmodules: true, reference: '', trackingSubmodules: false]], userRemoteConfigs: [[url: 'https://github.com/getbraincloud/examples-cpp.git']]])
                sh '${BRAINCLOUD_TOOLS}/bin/checkout-submodule.sh thirdparties/braincloud-cpp ${BC_LIB}'
                dir('thirdparties/braincloud-cpp') {
        		    sh 'autobuild/build_linux.sh'
                }
            }
            post {
                success {
                    archiveArtifacts allowEmptyArchive: true, artifacts: 'thirdparties/braincloud-cpp/artifacts/*.*', followSymlinks: false, onlyIfSuccessful: true
                }
        	}
        }

        // todo: Package Library Windows (python script?)
        // todo: package android?
        // todo: build UWP?

        // end stages
    }
    // end pipeline
}