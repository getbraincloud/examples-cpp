package com.bitheads.braincloud.android;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    private MainLoop thread;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        TextView tv = (TextView) findViewById(R.id.sample_text);
        tv.setText("Initializing...");

        thread = new MainLoop(tv);
        thread.start();
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String mainLoopJNI();

    class MainLoop extends Thread {
        private TextView tv;

        public MainLoop(TextView in_tv) {
            tv = in_tv;
        }

        @Override
        public void run() {
            int frameId = 0;
            while(true) {
                try {
                    Thread.sleep(200);
                }
                catch (Exception ex) {
                    return;
                }

                tv.post(new Runnable() {
                    public void run() {
                        tv.setText(mainLoopJNI());
                    }
                });

                ++frameId;
            }
        }
    }
}
