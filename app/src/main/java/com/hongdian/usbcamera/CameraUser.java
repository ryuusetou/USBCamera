package com.hongdian.usbcamera;

import java.nio.ByteBuffer;

/**
 * Created by ryuus on 2016/9/8 0008.
 */
public interface CameraUser {
    void onFrameAvailable(Camera camera, ByteBuffer frame, long timeStampUs);

    void onCameraClose();
}
