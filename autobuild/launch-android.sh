#!/bin/bash


case "$1" in
    devices)
        adb devices
        ;;
    start)
        adb -s $2 shell am start -n com.bitheads.braincloud.android.debug/com.bitheads.braincloud.android.MainActivity -a android.intent.action.MAIN -c android.intent.category.LAUNCHER
        ;;
    stop)
        adb -s $2 shell am force-stop com.bitheads.braincloud.android.debug
        ;;
     
    install)
        adb -s $2  install -t $3/app/build/outputs/apk/debug/app-debug.apk
        ;;
    uninstall)
    adb -s $2 uninstall com.bitheads.braincloud.android.debug
        ;;
    clear)
    adb -s $2 shell pm clear com.bitheads.braincloud.android.debug
        ;;
     
    *)
        echo $"Usage: $0 {start|stop|install|uninstall|clear|devices}"
        exit 1
 
esac
