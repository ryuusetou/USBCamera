package com.hongdian.usbcamera;

import java.nio.ByteBuffer;
import java.util.LinkedList;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Created by ryuus on 2016/9/8 0008.
 */
public class USBCamera {
    private Lock mUserLock;

    private LinkedList<USBCameraUser> mUser;

    public USBCamera(int id, int width, int height){
        mUserLock = new ReentrantLock();
        mUser = new LinkedList<>();
    }

    public void addUser(USBCameraUser user) {
        mUserLock.lock();
        mUser.add(user);
        mUserLock.unlock();
    }

    public void deleteUser(USBCameraUser user){
        mUserLock.lock();
        mUser.remove(user);
        if (mUser.size() == 0)
            closeCamera();
        mUserLock.unlock();
    }

    private void onNewFrame(ByteBuffer frame) {
        mUserLock.lock();
        for (USBCameraUser user : mUser) {
            user.onFrameAvailable(this, frame);
        }
        mUserLock.unlock();
    }

    private void onCameraClose(ByteBuffer frame) {
        mUserLock.lock();
        for (USBCameraUser user : mUser) {
            user.onCameraClosing();
        }
        mUserLock.unlock();
    }

    private native int openCamera();

    private native int peekFrame();

    private native void closeCamera();
}
