package com.bitheads.braincloud.android;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Context;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.widget.TextView;
import android.content.res.AssetManager;

import com.bitheads.braincloud.android.databinding.ActivityMainBinding;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'android' library on application startup.
    static {
        System.loadLibrary("braincloud_android");
    }

    public native void setDir(String caCertDir);

    private String saveCertPemFile()
    {
        Context context = getApplicationContext();
        String assetFileName = "cacerts.pem";

        if(context == null || !FileExistInAssets(assetFileName,context))
        {
            Log.i("TestActivity", "Context is null or asset file doesnt exist");
            return null;
        }
        //destination path is data/data/packagename
        String destPath = getApplicationContext().getApplicationInfo().dataDir;
        String CertFilePath = destPath + "/" +assetFileName;
        File file = new File(CertFilePath);
        if(file.exists())
        {
            //delete file
            file.delete();
        }
        //copy to internal storage
        if(CopyAssets(context,assetFileName,CertFilePath) == 1) return CertFilePath;

        return CertFilePath=null;

    }

    private int CopyAssets(Context context,String assetFileName, String toPath)
    {
        AssetManager assetManager = context.getAssets();
        InputStream in = null;
        OutputStream out = null;
        try {
            in = assetManager.open(assetFileName);
            new File(toPath).createNewFile();
            out = new FileOutputStream(toPath);
            byte[] buffer = new byte[1024];
            int read;
            while ((read = in.read(buffer)) != -1)
            {
                out.write(buffer, 0, read);
            }
            in.close();
            in = null;
            out.flush();
            out.close();
            out = null;
            return 1;
        } catch(Exception e) {
            Log.e("tag", "CopyAssets"+e.getMessage());

        }
        return 0;

    }

    private boolean FileExistInAssets(String fileName,Context context)
    {
        try {
            return Arrays.asList(context.getResources().getAssets().list("")).contains(fileName);
        } catch (IOException e) {
            // TODO Auto-generated catch block

            Log.e("tag", "FileExistInAssets"+e.getMessage());

        }
        return false;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setDir(saveCertPemFile());

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

                        //automatically scroll as text is added
                        int scrollAmount = tv.getLayout().getLineTop(tv.getLineCount()) - tv.getHeight();
                        if (scrollAmount > 0)
                            tv.scrollTo(0, scrollAmount);
                        else
                            tv.scrollTo(0, 0);
                    }
                });
            }
        }
    }
}
