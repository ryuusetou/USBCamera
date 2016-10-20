package com.hongdian.usbcamera;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;

import java.nio.ByteBuffer;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

//        new Thread(new Runnable() {
//            @Override
//            public void run() {
//                Camera camera = new Camera("/dev/video0", 640, 480);
//
//                camera.addUser(new CameraUser() {
//                    @Override
//                    public void onFrameAvailable() {
//                        Log.d("JAVAWORLD",
//                                frame.array()[0] + " " +
//                                        frame.array()[1] + " "
//                                        + frame.array()[2] + " "
//                                        + timeStampUs);
//                    }
//
//                    @Override
//                    public void onCameraClose() {
//                        Log.d("JAVAWORLD", "closed!");
//                    }
//                });
//
//                camera.startStream();
//            }
//        }).start();
    }
}
