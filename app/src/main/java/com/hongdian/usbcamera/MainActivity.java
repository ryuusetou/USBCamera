package com.hongdian.usbcamera;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
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

public class MainActivity extends AppCompatActivity {
    private SurfaceView surfaceView;

    private Camera mCamera;

    private class CameraPreview implements SurfaceHolder.Callback {
        @Override
        public void surfaceCreated(SurfaceHolder surfaceHolder) {
            mCamera.startPreview(surfaceHolder.getSurface());
        }

        @Override
        public void surfaceChanged(SurfaceHolder surfaceHolder, int i, int i1, int i2) {

        }

        @Override
        public void surfaceDestroyed(SurfaceHolder surfaceHolder) {
            mCamera.stopPreview();
        }
    }

    private class CameraEncoder implements CameraUser{

        public CameraEncoder() {

        }

        @Override
        public FrameContainer onGetFrameContainer() {
            return null;
        }

        @Override
        public void onNotifyFilled() {

        }

        @Override
        public void onCameraClose() {

        }
    }

    private class CameraRecorder implements CameraUser{

        private  MediaCodecInfo selectCodec() {
            int numCodecs = MediaCodecList.getCodecCount();
            for (int i = 0; i < numCodecs; i++) {
                MediaCodecInfo codecInfo = MediaCodecList.getCodecInfoAt(i);

                if (!codecInfo.isEncoder()) {
                    continue;
                }

                String[] types = codecInfo.getSupportedTypes();
                for (int j = 0; j < types.length; j++) {
                    Log.d("CameraRecorder", "support type:" + types[j]);
                }
            }
            return null;
        }

        private MediaCodec encoder;

        public CameraRecorder() {
            container = new FrameContainer();
            mLock = new ReentrantLock();

            ByteBuffer mHotBuffer;

            MediaFormat format = MediaFormat.createVideoFormat(
                    MediaFormat.MIMETYPE_VIDEO_AVC,
                    1280, 720);
            format.setInteger(MediaFormat.KEY_FRAME_RATE,
                    10);
            format.setInteger(MediaFormat.KEY_BIT_RATE,
                    2 * 1000 * 1000);
            format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);

            format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                    MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar);

            File targetFile = new File(Environment.getExternalStorageDirectory(), "1.h264");
            FileOutputStream fos = null;
            try {
                fos = new FileOutputStream(targetFile);
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            }
//            selectCodec();

            encoder = null;
            try {
                encoder = MediaCodec.createEncoderByType(
                        format.getString(MediaFormat.KEY_MIME));

                encoder.configure(format, null, null,
                        CONFIGURE_FLAG_ENCODE);

                // mCamera.startPreview(encoder.createInputSurface());

                encoder.start();

                final FileOutputStream finalFos = fos;
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        MediaCodec.BufferInfo outInfo = new MediaCodec.BufferInfo();

                        ByteBuffer [] outBuffers;

                        ByteBuffer [] inBuffers;

                        byte[] encodeData;

                        for (;;) {

                            int outputBufferId = encoder.dequeueOutputBuffer(outInfo, 20 * 1000);

                            // Log.d("CODEC", "out id:" + outputBufferId);

                            if (outputBufferId >= 0) {
                                // outputBuffers[outputBufferId] is ready to be processed or rendered.

                                Log.d("CODEC", "out size:" + outInfo.size +
                                        " timeStamp:" + outInfo.presentationTimeUs);

                                outBuffers = encoder.getOutputBuffers();

                                encodeData = new byte[outInfo.size];

                                outBuffers[outputBufferId].get(encodeData, 0, outInfo.size);
                                try {
                                    finalFos.write(encodeData, 0, outInfo.size);
                                } catch (IOException e) {
                                    e.printStackTrace();
                                }

                                encoder.releaseOutputBuffer(outputBufferId, false);
                            } else if (outputBufferId == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                                // outputBuffers = encoder.getOutputBuffers();
                            } else if (outputBufferId == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                                // Subsequent data will conform to new format.
                                MediaFormat format = encoder.getOutputFormat();
                            }
                        }
                    }
                }).start();

            } catch (IOException e) {
                e.printStackTrace();

            }
        }

        private FrameContainer container;

        private Lock mLock;

        @Override
        public FrameContainer onGetFrameContainer() {
            if (mLock.tryLock()) {
                return container;
            }

            return null;
        }

        @Override
        public void onNotifyFilled() {
            ByteBuffer [] inBuffers = null;

            ByteBuffer curInputBuffer;
            int inputBufferId = encoder.dequeueInputBuffer(10 * 1000);
            if (inputBufferId >= 0) {
                // fill inputBuffers[inputBufferId] with valid data

                Log.d("CODEC", "input len:" + container.len +
                        " " + container.timeStamp * 1000);

                inBuffers = encoder.getInputBuffers();
                inBuffers[inputBufferId].clear();

                //mLock.lock();
                inBuffers[inputBufferId].put(container.target.array(), 0, container.len);
                //mLock.unlock();

                encoder.queueInputBuffer(inputBufferId, 0, container.len, 0, 0);
            }

            mLock.unlock();
        }

        @Override
        public void onCameraClose() {

        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mCamera = new Camera("/dev/video0", 1280, 720);

        // mCamera.addUser(new CameraRecorder());

        mCamera.startStream();

        surfaceView = (SurfaceView)findViewById(R.id.play_view);
        SurfaceHolder surfaceHolder = surfaceView.getHolder();

//         surfaceHolder.addCallback(new CameraPreview());

        //////////////////////////////////////////////////

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

        File targetFile = new File(Environment.getExternalStorageDirectory(), "1.h264");
        FileOutputStream fos = null;
        try {
            fos = new FileOutputStream(targetFile);
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        }

        final MediaCodec encoder;
        try {
            encoder = MediaCodec.createEncoderByType(
                            format.getString(MediaFormat.KEY_MIME));

            encoder.configure(format, null, null,
                            CONFIGURE_FLAG_ENCODE);

            mCamera.startPreview(encoder.createInputSurface());

            encoder.start();

            final FileOutputStream finalFos = fos;
            new Thread(new Runnable() {
                @Override
                public void run() {
                    MediaCodec.BufferInfo outInfo = new MediaCodec.BufferInfo();

                    ByteBuffer [] outBuffers;

                    byte[] encodeData;

                    for (;;) {
                        int outputBufferId = encoder.dequeueOutputBuffer(outInfo, -1);
                        if (outputBufferId >= 0) {
                            // outputBuffers[outputBufferId] is ready to be processed or rendered.

                            Log.d("CODEC", "out size:" + outInfo.size +
                                    " timeStamp:" + outInfo.presentationTimeUs);

                            outBuffers = encoder.getOutputBuffers();

                            encodeData = new byte[outInfo.size];

                            outBuffers[outputBufferId].get(encodeData, 0, outInfo.size);
                            try {
                                finalFos.write(encodeData, 0, outInfo.size);
                            } catch (IOException e) {
                                e.printStackTrace();
                            }

                            encoder.releaseOutputBuffer(outputBufferId, false);
                        } else if (outputBufferId == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                            // outputBuffers = encoder.getOutputBuffers();
                        } else if (outputBufferId == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                            // Subsequent data will conform to new format.
                            MediaFormat format = encoder.getOutputFormat();
                        }
                    }
                }
            }).start();

        } catch (IOException e) {
            e.printStackTrace();
        }

    }
}
