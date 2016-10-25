package com.hongdian.usbcamera;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

public class MainActivity extends AppCompatActivity {


//    private class MyCameraUser implements CameraUser{
//        private FrameContainer container;
//
//        private FileOutputStream mFos;
//
//        private File mPictureFile;
//
//        private int mPictureCount;
//
//        public MyCameraUser() {
//            container = new FrameContainer();
//
//            mPictureFile = new File(Environment.getExternalStorageDirectory(), "1.yuyv");
//
//            mPictureCount = 0;
//        }
//
//        @Override
//        public FrameContainer onGetFrameContainer() {
//            // Log.d("CU", "len:" + container.size + " stamp:" + container.timeStamp);
//
//            return container;
//        }
//
//        @Override
//        public void onNotifyFilled() {
//            // Log.d("CU", "Frame prepared");
//
//            if (mPictureCount++ == 10) {
//                Log.d("CU", "onNotifyFilled: save to file " + mPictureFile.getAbsolutePath());
//
//                try {
//                    mFos = new FileOutputStream(mPictureFile);
//                } catch (FileNotFoundException e) {
//                    Log.d("CU", "file not found");
//                    e.printStackTrace();
//                    return;
//                }
//
//                try {
//                    mFos.write(container.target.array());
//                    mFos.close();
//                } catch (IOException e) {
//                    Log.d("CU", "file write close IOException");
//                    e.printStackTrace();
//                    return;
//                }
//            }
//        }
//
//        @Override
//        public void onCameraClose() {
//            Log.d("CU", "camera closed");
//        }
//    }

    private SurfaceView surfaceView;

    private Camera camera;

    private class CameraPreviewUser implements CameraUser, SurfaceHolder.Callback {
        private SurfaceHolder mSurfaceHolder;

        private Paint mBitPaint;

        private FrameContainer mContainer;

        private Camera mCamera;

        private Lock mFrameLock;

        int [] intArray;


        public CameraPreviewUser(Camera camera) {
            mContainer = new FrameContainer();

            mCamera = camera;

            mFrameLock = new ReentrantLock();

            mBitPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            mBitPaint.setFilterBitmap(true);
            mBitPaint.setDither(true);
        }

        @Override
        public void surfaceCreated(SurfaceHolder surfaceHolder) {
            mCamera.addUser(this);
            mSurfaceHolder = surfaceHolder;

            Log.d("Main", "surfaceCreated");
        }

        @Override
        public void surfaceChanged(SurfaceHolder surfaceHolder, int i, int i1, int i2) {
            Log.d("Main", "surfaceChanged");

            mCamera.startPreview(surfaceHolder.getSurface());
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder surfaceHolder) {
            Log.d("Main", "surfaceDestroyed");
        }

        @Override
        public FrameContainer onGetFrameContainer() {
            FrameContainer container = null;

            if (mFrameLock.tryLock()) {
                container = mContainer;
                mFrameLock.unlock();
            }

            return container;
        }

        @Override
        public void onNotifyFilled() {
            Canvas canvas;

            intArray = new int[1280 * 720];

            canvas = mSurfaceHolder.lockCanvas();

            mFrameLock.lock();
            convertYUYV2RGB8888(intArray, mContainer.target.array(), 1280, 720);
            mFrameLock.unlock();

            Bitmap bmp = Bitmap.createBitmap(intArray, 1280, 720, Bitmap.Config.ARGB_8888);
            canvas.drawBitmap(bmp, 0, 0, mBitPaint);
            mSurfaceHolder.unlockCanvasAndPost(canvas);
        }

        @Override
        public void onCameraClose() {

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
            u = data[i + 1] & 0xff;
            i += 2;

            y1 = data[i] & 0xff;
            v = data[i + 1] & 0xff;
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

        return 0xff000000 | (r << 16) | (g << 8) | b;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        camera = new Camera("/dev/video0", 1280, 720);
        camera.startStream();

        surfaceView = (SurfaceView)findViewById(R.id.play_view);
        SurfaceHolder surfaceHolder = surfaceView.getHolder();

        surfaceHolder.addCallback(new CameraPreviewUser(camera));
    }
}
