package com.bitheads.braincloud.android;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.widget.TextView;

import com.bitheads.braincloud.android.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'android' library on application startup.
    static {
        System.loadLibrary("android");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ActivityMainBinding binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        // Example of a call to a native method
        TextView tv = binding.sampleText;
        tv.setMovementMethod(new ScrollingMovementMethod());
        tv.setText("Welcome to BrainCloud C++ for Android");

        MainLoop thread = new MainLoop(tv);
        thread.start();
    }

    /**
     * A native method that is implemented by the 'android' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();

    class MainLoop extends Thread {
        private final TextView tv;

        public MainLoop(TextView in_tv) {
            tv = in_tv;
        }

        @Override
        public void run() {
            while(true) {
                try {
                    Thread.sleep(200);
                }
                catch (Exception ex) {
                    return;
                }

                tv.post(() -> {
                    String newText = stringFromJNI();
                    if (!("".equals(newText)) && (newText.length() != tv.getText().length())) {
                        tv.setText(newText);
                        //tv.scrollTo(0, tv.getHeight());
                    }
                });
            }
        }
    }
}
