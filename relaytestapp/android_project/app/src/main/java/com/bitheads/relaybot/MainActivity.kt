package com.bitheads.relaybot

//import com.google.androidgamesdk.GameActivity
import android.os.Bundle
import org.libsdl.app.SDLActivity

class MainActivity : SDLActivity() {

    /* A fancy way of getting the class name */
    private val TAG: String = com.bitheads.relaybot.MainActivity::class.java.getSimpleName()

    override fun getLibraries(): Array<String>? {
        return arrayOf("hidapi", "SDL2", "relaybot")
    }

    override fun getArguments(): Array<String>? {
        return arrayOf(filesDir.absolutePath)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
    }
}