package com.hongdian.usbcamera;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

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

    public static void convertYUYV2RGB8888(int[] rgba,
                                           byte[] data,
                                           int width, int height) {
        int size = width * height * 2;
        int u, v, y0, y1;
        int count = 0;

        for (int i = 0; i < size; ) {
            y0 = data[i] & 0xff;
            v = data[i + 1] & 0xff;
            i += 2;

            y1 = data[i] & 0xff;
            u = data[i + 1] & 0xff;
            i += 2;

            rgba[count] = convertYUVtoRGB(y0, u, v);
            rgba[count + 1] = convertYUVtoRGB(y1, u, v);
            count += 2;
        }
    }

    private static int convertYUVtoRGB(int y, int u, int v) {
        int r, g, b;

        r = (int)((y&0xff) + 1.4075 * ((v&0xff)-128));
        g = (int)((y&0xff) - 0.3455 * ((u&0xff)-128) - 0.7169*((v&0xff)-128));
        b = (int)((y&0xff) + 1.779 * ((u&0xff)-128));
        r =(r<0? 0: r>255? 255 : r);
        g =(g<0? 0: g>255? 255 : g);
        b =(b<0? 0: b>255? 255 : b);

        return 0xff000000 | (b << 16) | (g << 8) | r;
    }

    private class CameraEncoder implements CameraUser{

        public CameraEncoder(Camera camera) {

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


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mCamera = new Camera("/dev/video0", 1280, 720);
        mCamera.startStream();

        surfaceView = (SurfaceView)findViewById(R.id.play_view);
        SurfaceHolder surfaceHolder = surfaceView.getHolder();

        // surfaceHolder.addCallback(new CameraPreview());

        //////////////////////////////////////////////////

        MediaFormat format = MediaFormat.createVideoFormat(
                MediaFormat.MIMETYPE_VIDEO_AVC,
                1280, 720);
        format.setInteger(MediaFormat.KEY_FRAME_RATE,
                10);
        format.setInteger(MediaFormat.KEY_BIT_RATE,
                2 * 1000 * 1000);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);

        final MediaCodec encoder;
        try {
            encoder = MediaCodec.createEncoderByType(
                            format.getString(MediaFormat.KEY_MIME));

            encoder.configure(format, null, null,
                            MediaCodec.CONFIGURE_FLAG_ENCODE);

            mCamera.startPreview(encoder.createInputSurface());

            encoder.start();

            new Thread(new Runnable() {
                @Override
                public void run() {
                    MediaCodec.BufferInfo outInfo = new MediaCodec.BufferInfo();

                    ByteBuffer [] outBuffers = encoder.getOutputBuffers();
                    for (;;) {
                        int outputBufferId = encoder.dequeueOutputBuffer(outInfo, -1);
                        if (outputBufferId >= 0) {
                            // outputBuffers[outputBufferId] is ready to be processed or rendered.

                            Log.d("CODEC", "out size:" + outInfo.size + " timeStamp:" +
                                                outInfo.presentationTimeUs);

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
