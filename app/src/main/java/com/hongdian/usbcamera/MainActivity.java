package com.hongdian.usbcamera;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;

import java.nio.ByteBuffer;

public class MainActivity extends AppCompatActivity {

    private class MyCameraUser implements CameraUser{
        private FrameContainer container;

        public MyCameraUser() {
            container = new FrameContainer();
        }

        @Override
        public FrameContainer onGetFrameContainer() {

            Log.d("FrameUser", "len:" + container.size + " stamp:" + container.timeStamp);

            return container;
        }

        @Override
        public void onCameraClose() {

        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        new Thread(new Runnable() {
            @Override
            public void run() {

                Camera camera = new Camera("/dev/video0", 1280, 720);

                camera.addUser(new MyCameraUser());

                camera.startStream();
            }
        }).start();

    }
}
