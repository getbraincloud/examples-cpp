#!/bin/bash


case "$1" in
    devices)
        $ANDROID_SDK/platform-tools/adb devices
        ;;
    start)
        $ANDROID_SDK/platform-tools/adb shell am start -n com.bitheads.braincloud.android.debug/com.bitheads.braincloud.android.MainActivity -a android.intent.action.MAIN -c android.intent.category.LAUNCHER
        ;;
    stop)
        $ANDROID_SDK/platform-tools/adb shell am force-stop com.bitheads.braincloud.android.debug
        ;;
     
    install)
        $ANDROID_SDK/platform-tools/adb install -t $2/app/build/outputs/apk/debug/app-debug.apk
        ;;
    uninstall)
        $ANDROID_SDK/platform-tools/adb uninstall com.bitheads.braincloud.android.debug
        ;;
    clear)
        $ANDROID_SDK/platform-tools/adb shell pm clear com.bitheads.braincloud.android.debug
        ;;
     
    *)
        echo $"Usage: $0 {start|stop|install|uninstall|clear|devices}"
        exit 1
 
esac
exit 0
