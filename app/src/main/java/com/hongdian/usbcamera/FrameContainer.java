package com.hongdian.usbcamera;

import java.nio.ByteBuffer;

/**
 * Created by ryuusetou on 2016/10/20.
 */

public class FrameContainer {

    public int width;

    public int height;

    public long timeStamp;

    public int size;

    public ByteBuffer target;

    FrameContainer() {
        target = ByteBuffer.allocate(1280 * 720 * 2);
    }
}
