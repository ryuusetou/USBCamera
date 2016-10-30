package com.hongdian.usbcamera;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import static android.media.MediaCodec.CONFIGURE_FLAG_ENCODE;
import static android.media.MediaCodec.createEncoderByType;

public class MainActivity extends AppCompatActivity {
    private SurfaceView surfaceView;

    private Camera mCamera;

    private class CameraPreview implements SurfaceHolder.Callback, Camera.CameraClient{
        private Surface mSurface;

        @Override
        public void surfaceCreated(SurfaceHolder surfaceHolder) {
            mSurface = surfaceHolder.getSurface();
            mCamera.addClient(this);
        }

        @Override
        public void surfaceChanged(SurfaceHolder surfaceHolder, int i, int i1, int i2) {

        }

        @Override
        public void surfaceDestroyed(SurfaceHolder surfaceHolder) {
            mCamera.removeClient(this);
        }

        @Override
        public Surface getSurface() {
            return mSurface;
        }

        @Override
        public void onFirstCapture(long timeStampUS) {

        }

        @Override
        public void onCaptureStop() {

        }

        @Override
        public void onCameraError() {

        }

        @Override
        public boolean isEncoder() {
            return false;
        }
    }

    private class CameraEncoder implements Camera.CameraClient {

        private MediaCodec mEncoder;

        public CameraEncoder() {
            MediaFormat format = MediaFormat.createVideoFormat(
                    MediaFormat.MIMETYPE_VIDEO_AVC,
                    640, 480);
            format.setInteger(MediaFormat.KEY_FRAME_RATE,
                    10);
            format.setInteger(MediaFormat.KEY_BIT_RATE,
                    2 * 1000 * 1000);
            format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);
            format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                    MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);

            try {
                mEncoder = MediaCodec.createEncoderByType(
                        format.getString(MediaFormat.KEY_MIME));

                mEncoder.configure(format, null, null,
                        CONFIGURE_FLAG_ENCODE);


            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        @Override
        public Surface getSurface() {
            Surface surface = mEncoder.createInputSurface();
            if (surface != null) {
                Log.d("Camera", "encoder surface is ok");
            }
            return surface;
        }

        @Override
        public void onFirstCapture(long timeStampUS) {
            mEncoder.start();

            File targetFile = new File(Environment.getExternalStorageDirectory(), "1.h264");
            FileOutputStream fos = null;
            try {
                fos = new FileOutputStream(targetFile);
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            }

            final FileOutputStream finalFos = fos;
            new Thread(new Runnable() {
                @Override
                public void run() {
                    MediaCodec.BufferInfo outInfo = new MediaCodec.BufferInfo();

                    ByteBuffer [] outBuffers;

                    byte[] encodeData;

                    for (;;) {
                        int outputBufferId = mEncoder.dequeueOutputBuffer(outInfo, -1);
                        if (outputBufferId >= 0) {
                            // outputBuffers[outputBufferId] is ready to be processed or rendered.

                            Log.d("CODEC", "out size:" + outInfo.size +
                                    " timeStamp:" + outInfo.presentationTimeUs);

                            outBuffers = mEncoder.getOutputBuffers();

                            encodeData = new byte[outInfo.size];

                            outBuffers[outputBufferId].get(encodeData, 0, outInfo.size);
                            try {
                                finalFos.write(encodeData, 0, outInfo.size);
                            } catch (IOException e) {
                                e.printStackTrace();
                            }

                            mEncoder.releaseOutputBuffer(outputBufferId, false);
                        } else if (outputBufferId == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                            // outputBuffers = encoder.getOutputBuffers();
                        } else if (outputBufferId == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                            // Subsequent data will conform to new format.
                            MediaFormat format = mEncoder.getOutputFormat();
                        }
                    }
                }
            }).start();
        }

        @Override
        public void onCaptureStop() {

        }

        @Override
        public void onCameraError() {

        }

        @Override
        public boolean isEncoder() {
            return true;
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mCamera = new Camera("/dev/video0", 1280, 720);

        surfaceView = (SurfaceView)findViewById(R.id.play_view);
        SurfaceHolder surfaceHolder = surfaceView.getHolder();

        surfaceHolder.addCallback(new CameraPreview());

        mCamera.addClient(new CameraEncoder());
        mCamera.start();
    }
}
