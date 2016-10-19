package com.hongdian.usbcamera;

import java.nio.ByteBuffer;
import java.util.LinkedList;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Created by ryuus on 2016/9/8 0008.
 */
public class Camera {
    private Lock mUserLock;

    private LinkedList<CameraUser> mUser;

    private int nativeFD;

    private String mCameraDevPath;

    private int mWidth;

    private int mHeight;

    public Camera(String camera, int width, int height){
        mUserLock = new ReentrantLock();
        mUser = new LinkedList<>();
        nativeFD = -1;

        mCameraDevPath = camera;

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
        nativeFD = openCamera();
    }

    public void stopStream(){
        if (nativeFD > 0) {
            closeCamera();
            nativeFD = -1;
        }
    }

    private void onNewFrame(ByteBuffer frame, long timeStampUs) {
        mUserLock.lock();
        for (CameraUser user : mUser) {
            user.onFrameAvailable(this, frame, timeStampUs);
        }
        mUserLock.unlock();
    }

    private void onCameraClose() {
        mUserLock.lock();
        for (CameraUser user : mUser) {
            user.onCameraClose();
        }
        mUserLock.unlock();
    }

    private native int openCamera();

    private native int peekFrame();

    private native void closeCamera();
}
