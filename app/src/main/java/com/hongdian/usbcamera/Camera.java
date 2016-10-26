package com.hongdian.usbcamera;

import android.view.Surface;

import java.util.LinkedList;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Created by ryuus on 2016/9/8 0008.
 */
public class Camera {

    static {
        System.loadLibrary("usbcamera");

        initNative();
    }

    private Lock mUserLock;

    private LinkedList<CameraUser> mUser;

    private int mNativeFD;

    private String mDevPath;

    private int mWidth;

    private int mHeight;

    private int mNativeObject;

    public Camera(String camera, int width, int height){
        mUserLock = new ReentrantLock();

        mUser = new LinkedList<>();

        mNativeFD = -1;

        mNativeObject = 0;

        mDevPath = camera;

        mWidth = width;

        mHeight = height;
    }

    public void addUser(CameraUser user) {
        mUserLock.lock();
        mUser.add(user);
        mUserLock.unlock();
    }

    public void deleteUser(CameraUser user){
        mUserLock.lock();
        mUser.remove(user);
        if (mUser.size() == 0)
            closeCamera();
        mUserLock.unlock();
    }

    public void startStream(){
        mNativeFD = openCamera(mWidth, mHeight);

        if (mNativeFD > 0) {
            new Thread(new Runnable() {
                @Override
                public void run() {
                    peekFrame();
                }
            }).start();
        }
    }

    public void startPreview(Surface surface) {
        if (mNativeFD > 0) {
            nativeStartPreview(surface);
        }
    }

    public void stopPreview() {
        if (mNativeFD > 0) {
            nativeStopPreview();
        }
    }

    public void stopStream(){
        if (mNativeFD > 0) {
            closeCamera();
            mNativeFD = -1;
        }
    }

    private void onNewFrame() {
        FrameContainer container;

        mUserLock.lock();
        for (CameraUser user : mUser) {
            container = user.onGetFrameContainer();
            if (container != null) {
                fillFrame(container);
                user.onNotifyFilled();
            }
        }
        mUserLock.unlock();
    }

    private void onCameraClose() {
        mNativeFD = -1;

        mUserLock.lock();
        for (CameraUser user : mUser) {
            user.onCameraClose();
        }
        mUserLock.unlock();
    }

    static private native void initNative();

    private native int openCamera(int width, int height);

    private native void nativeStartPreview(Surface view);

    private native void nativeStopPreview();

    private native int peekFrame();

    private native void fillFrame(FrameContainer container);

    private native void closeCamera();
}
