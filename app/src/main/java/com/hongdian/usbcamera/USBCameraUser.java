package com.hongdian.usbcamera;

import java.nio.ByteBuffer;

/**
 * Created by ryuus on 2016/9/8 0008.
 */
public interface USBCameraUser {

    void onFrameAvailable(USBCamera camera, ByteBuffer frame);

    void onCameraClosing();

}
