package com.hongdian.usbcamera;

import android.util.Log;
import android.view.Surface;

import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Created by ryuus on 2016/9/8 0008.
 */
public class Camera {
    protected static final String TAG = "Camera";

    public interface CameraClient {
        Surface getSurface();

        void onFirstCapture(long timeStampUS);

        void onCaptureStop();

        void onCameraError();

        boolean isEncoder();
    }

    static {
        System.loadLibrary("usbcamera");
    }

    private Lock mUserLock;

    private String mDevPath;

    private int mWidth;

    private int mHeight;

    private int mNativeObject;

    private List<CameraClient> mClients;

    private Lock mClientLock;

    public Camera(String camera, int width, int height){
        mUserLock = new ReentrantLock();

        mNativeObject = 0;

        mDevPath = camera;

        mWidth = width;

        mHeight = height;

        mClientLock = new ReentrantLock();

        mClients = new LinkedList<>();

        attachNative(mWidth, mHeight);
    }

    public int start(){
        int rc = -1;
        if ( (rc = openCamera()) != 0) {
            Log.d(TAG, "start: openCamera rc:" + rc);
            return rc;
        }

        new Thread(new Runnable() {
            @Override
            public void run() {
                startCapture();
            }
        }).start();

        return 0;
    }

    public void stop(){
        stopCapture();
    }

    public void release() {
        mClientLock.lock();
        for (CameraClient c : mClients) {
            mClients.remove(c);
        }
        mClientLock.unlock();

        releaseCamera();
    }

    public void addClient(CameraClient client) {
        mClientLock.lock();
        mClients.add(client);
        if (client.isEncoder()) {
            addEncoder(client.getSurface());
        } else {
            setPreview(client.getSurface());
        }
        mClientLock.unlock();
    }

    public void removeClient(CameraClient client) {
        mClientLock.lock();
        mClients.remove(client);
        if (client.isEncoder()) {
            removeEncoder(client.getSurface());
        } else {
            setPreview(null);
        }
        mClientLock.unlock();
    }

    private void notifyFirstCapture(long timestampUS) {
        mClientLock.lock();
        for (CameraClient c : mClients) {
            c.onFirstCapture(timestampUS);
        }
        mClientLock.unlock();
    }

    private void notifyCaptureStop() {
        mClientLock.lock();
        for (CameraClient c : mClients) {
            c.onCaptureStop();
        }
        mClientLock.unlock();
    }

    private void notifyCameraError() {
        mClientLock.lock();
        for (CameraClient c : mClients) {
            c.onCameraError();
        }
        mClientLock.unlock();
    }

    private native void attachNative(int width, int height);

    private native int openCamera();

    private native void startCapture();

    private native void stopCapture();

    private native void releaseCamera();

    private native void setPreview(Surface surface);

    private native void addEncoder(Surface surface);

    private native void removeEncoder(Surface surface);
}
