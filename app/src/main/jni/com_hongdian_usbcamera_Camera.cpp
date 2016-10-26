//
// Created by ryuusetou on 2016/10/19.
//

#include <map>
#include <iostream>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <asm/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <linux/videodev2.h>
#include <dirent.h>

#include <android/log.h>
#include <android/native_window.h>
#include <gui/Surface.h>

#include <ui/GraphicBufferMapper.h>
#include <android_runtime/android_view_Surface.h>


#include "com_hongdian_usbcamera_Camera.h"

//#define LOG_TAG "USBCameraJNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "USBCameraJNI", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "USBCameraJNI", __VA_ARGS__)

using namespace std;
using namespace android;


static int ALIGN(int x, int y) {
    // y must be a power of 2.
    return (x + y - 1) & ~(y - 1);
}

static void render(
        const void *data, const sp<ANativeWindow> &nativeWindow,int width,int height) {

    int err;
    sp<ANativeWindow> mNativeWindow = nativeWindow;	
	int halFormat = HAL_PIXEL_FORMAT_YV12;
    int bufWidth = (width + 1) & ~1;
    int bufHeight = (height + 1) & ~1;

	native_window_set_usage(mNativeWindow.get(),
            GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN
            | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP);

	native_window_set_buffers_dimensions(mNativeWindow.get(), 
			bufWidth, bufHeight);

	native_window_set_scaling_mode(mNativeWindow.get(),
        	NATIVE_WINDOW_SCALING_MODE_SCALE_CROP);

	native_window_set_buffers_format(mNativeWindow.get(), 
			halFormat);	
	
	ANativeWindowBuffer *buf;
	
	// ����һ����е�ͼ�λ�����
    if ((err = native_window_dequeue_buffer_and_wait(
			mNativeWindow.get(),
            &buf)) != 0) {
        ALOGW("Surface::dequeueBuffer returned error %d", err);
        return;
    }

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    Rect bounds(width, height);
    void *dst;

	// ��������һ��ͼ�λ�����,����ӳ�䵽�û�����
	mapper.lock(buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst);

    if (true) {
        size_t dst_y_size = buf->stride * buf->height;
        size_t dst_c_stride = ALIGN(buf->stride / 2, 16);	//1��v/u�Ĵ�С
        size_t dst_c_size = dst_c_stride * buf->height / 2;//u/v�Ĵ�С
        
        memcpy(dst, data, dst_y_size + dst_c_size * 2);
    }

	mapper.unlock(buf->handle);

    if ((err = mNativeWindow->queueBuffer(mNativeWindow.get(), buf, -1)) != 0) {
        ALOGW("Surface::queueBuffer returned error %d", err);
    }
    buf = NULL;
}

#if 0
static jboolean
nativeSetVideoSurface(JNIEnv *env, jobject thiz, jobject jsurface){
	// ALOGE("[%s]%d",__FILE__,__LINE__);
	surface = android_view_Surface_getSurface(env, jsurface);
	if(android::Surface::isValid(surface)){
		// ALOGE("surface is valid ");
	}else {
		ALOGE("surface is invalid ");
		return false;
	}
	// ALOGE("[%s][%d]\n",__FILE__,__LINE__);
	return true;
}

static void
nativeShowYUV(char *yuv,  int width, int height){
	render(yuv, surface, width, height);
}
#endif

static struct {
	jclass clazz;
	jfieldID stampID;
	jfieldID lenID;
	jfieldID targetID;
} gContainerClassInfo;

static struct {
	jclass clazz;
	jmethodID arrayID;
} gByteBufferClassInfo;

static struct {
	jclass clazz;
	jfieldID mDevPath;
	jfieldID mNativeObject;
	jmethodID onNewFrame;
	jmethodID onCameraClose;	
} gCameraClassInfo;


class Camera {
	
#define BUFFER_NR			(4)
	struct buffer {
		void *start;
		size_t length;
	};
	
public:
	static const char *getDevicePath(JNIEnv *env, jobject camera);

	static Camera *getCamera(JNIEnv *env, jobject camera);

	static void *setCamera(JNIEnv *env, jobject jcamera, Camera *camera);

	static inline int YUV2RGBA888(int y, int u, int v);

	static void frameYUYV2RGBA8888(int *out, char *buffer, int width, int height);

	static void  Yvyu2Yuv420(char * yvyu, char *yuv420, int width, int height);
	
	Camera(const char *devPath);
	Camera(const char *devPath, int cropWidth, int cropHeight);
	~Camera();
	
    int openCamera();
	
    void peekFrame(JNIEnv *env,jobject jCamera);

	void fillWithFrame(JNIEnv *env, jobject container);
	
	void closeCamera(JNIEnv *env, jobject jCamera);
	
	void setPreviewSurface(JNIEnv *env, jobject jsurface);
	
private:
	void drawPreview(char *buffer);
	
	char *mYuv420;

	pthread_mutex_t mSurfaceLock;
	sp<Surface> mPreviewSurface;
	
	char *mCameraDevPath;
	bool mPeekRun;
	int mFD;

	int mWidth;
	int mHeight;
	
	struct v4l2_buffer mCurBufInfo;
	struct buffer mBufferArray[BUFFER_NR];	
};

const char *Camera::getDevicePath(JNIEnv *env, jobject camera) {
	jstring devPath;	
	devPath = (jstring)env->GetObjectField(camera, gCameraClassInfo.mDevPath);
	return env->GetStringUTFChars(devPath, NULL);
}

Camera *Camera::getCamera(JNIEnv *env, jobject jcamera) {
	return reinterpret_cast<Camera *>(
				env->GetIntField(jcamera, gCameraClassInfo.mNativeObject));
}

void *Camera::setCamera(JNIEnv *env, jobject jcamera, Camera *camera) {
	env->SetIntField(jcamera, gCameraClassInfo.mNativeObject, 
				reinterpret_cast<int>(camera));
}

void  Camera::Yvyu2Yuv420(char * yvyu, char *yuv420, int width, int height)  
{  
    int i,j;  
    char *Y,*U,*V;  
    char y1,y2,u,v;  
      
    Y = yuv420;  
    U = yuv420 + width * height;
    V = U + width * height / 4;  
      
    for(i = 0; i < height; i++) {  
		for(j = 0; j < width / 2; j++) {
	        y1 = *(yvyu + (i * width / 2 + j) * 4);  
	        v  = *(yvyu + (i * width / 2 + j) * 4 + 1);  
	        y2 = *(yvyu + (i * width / 2 + j) * 4 + 2);  
	        u  = *(yvyu + (i * width / 2 + j) * 4 + 3);  
	      
		    *Y++ = y1; 
		    *Y++ = y2;
		      
		    if(i % 2 == 0) {  
				*U++=u;
				*V++=v;  
		    }  
		}  
    }  
}

int Camera::YUV2RGBA888(int y, int u, int v) {
	int r, g, b;

	r = (int)((y&0xff) + 1.4075 * ((v&0xff)-128));
	g = (int)((y&0xff) - 0.3455 * ((u&0xff)-128) - 0.7169*((v&0xff)-128));
	b = (int)((y&0xff) + 1.779 * ((u&0xff)-128));
	r =(r<0? 0: r>255? 255 : r);
	g =(g<0? 0: g>255? 255 : g);
	b =(b<0? 0: b>255? 255 : b);

	return 0x000000FF | (r << 24) | (g << 16) | (b << 8);
}

void Camera::frameYUYV2RGBA8888(int *out, char *data, int width, int height) {
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

        out[count] = YUV2RGBA888(y0, u, v);
        out[count + 1] = YUV2RGBA888(y1, u, v);
        count += 2;
    }
}

Camera::Camera(const char *devPath) 
	: mPeekRun(false)
	, mFD(-1)
	, mWidth(1280)
	, mHeight(720)
	, mYuv420(NULL){

	if (devPath != NULL )
		mCameraDevPath = strdup(devPath);
	else
		mCameraDevPath = NULL;
	
	pthread_mutex_init(&mSurfaceLock, NULL);
}

Camera::Camera
	(const char *devPath, int cropWidth, int cropHeight)
	: mPeekRun(false)
	, mFD(-1)
	, mWidth(cropWidth)
	, mHeight(cropHeight)
	, mYuv420(NULL){	

	if (devPath != NULL )
		mCameraDevPath = strdup(devPath);
	else
		mCameraDevPath = NULL;

	pthread_mutex_init(&mSurfaceLock, NULL);
}

Camera::~Camera() {

	pthread_mutex_destroy(&mSurfaceLock);

	if (mCameraDevPath != NULL)
		free(mCameraDevPath);	
}

int Camera::openCamera() {
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	mFD = open(mCameraDevPath, O_RDWR);

	if (mFD < 0) {
		LOGD("open camera %s failed\n", mCameraDevPath);
		return mFD;
	}

	struct v4l2_capability cap;
	if (0 > ioctl(mFD, VIDIOC_QUERYCAP, &cap)) {
		LOGD("VIDIOC_QUERYCAP failed\n");
		goto exit_tag;
	}

	if (0 == (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		LOGD("V4L2_CAP_VIDEO_CAPTURE loss\n");
		goto exit_tag;
	}

	struct v4l2_format fmt;
	memset(&fmt, 0, sizeof fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = mWidth;
	fmt.fmt.pix.height = mHeight;
	//V4L2_PIX_FMT_YUYV V4L2_PIX_FMT_MJPEG
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
	if (ioctl(mFD, VIDIOC_S_FMT, &fmt) < 0) {
		LOGD("VIDIOC_S_FMT failed\n");
		goto exit_tag;
	}

    struct v4l2_streamparm parm;
    memset(&parm,0,sizeof(struct v4l2_streamparm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
    parm.parm.capture.timeperframe.denominator = 30;
    parm.parm.capture.timeperframe.numerator = 1;
    if(0 > ioctl(mFD,VIDIOC_S_PARM, &parm)) {
        LOGD("VIDIOC_S_PARM failed\n");
        goto exit_tag;
    }

    parm.parm.capture.timeperframe.denominator = 0;
    parm.parm.capture.timeperframe.numerator = 0;

    if(0 > ioctl(mFD,VIDIOC_G_PARM, &parm)) {
        LOGD("VIDIOC_S_PARM failed\n");
        goto exit_tag;
    }

    LOGD("VIDIOC_G_PARM %d %d\n", 
		parm.parm.capture.timeperframe.denominator, 
		parm.parm.capture.timeperframe.numerator );

    struct v4l2_requestbuffers req;
	req.count = BUFFER_NR;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(mFD, VIDIOC_REQBUFS, &req)) {
		LOGD("VIDIOC_REQBUFS failed\n");
		goto exit_tag;
	}

	for (int i = 0; i < req.count; ++i) {
		struct v4l2_buffer buf;

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (ioctl(mFD, VIDIOC_QUERYBUF, &buf)) {
			LOGD("VIDIOC_QUERYBUF failed\n");
			goto exit_tag;
		}

		mBufferArray[i].length = buf.length;
		mBufferArray[i].start = mmap(NULL /* start anywhere */,
				buf.length, PROT_READ | PROT_WRITE /* required */,
				MAP_SHARED /* recommended */, mFD, buf.m.offset);

		if (MAP_FAILED == mBufferArray[i].start) {
			LOGD("mmap failed\n");
			goto exit_tag;
		}
	}

	for (int i = 0; i < BUFFER_NR; ++i) {
		struct v4l2_buffer buf;

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (0 > ioctl(mFD, VIDIOC_QBUF, &buf)) {
			LOGD("VIDIOC_QBUF failed at:%d\n", i);
			goto exit_tag;
		}	
	}

	if (0 > ioctl(mFD, VIDIOC_STREAMON, &type)) {
		LOGD("VIDIOC_STREAMON failed\n");
		goto exit_tag;
	}

	LOGD("camera open with fd=%d\n", mFD);

	return mFD;
	
exit_tag:
	close(mFD);
	mFD = -1;
	return mFD;
}

void Camera::setPreviewSurface(JNIEnv *env, jobject jsurface) {
	if (jsurface == NULL) {
		pthread_mutex_lock(&mSurfaceLock);
		mPreviewSurface = NULL;	
		pthread_mutex_unlock(&mSurfaceLock);
		return;
	}
	
	sp<Surface> surface =
		android_view_Surface_getSurface(env, jsurface);
	if(!android::Surface::isValid(surface)){
		ALOGE("surface is invalid, setPreviewSurface failed!");
		return;
	}
	
	pthread_mutex_lock(&mSurfaceLock);
	mPreviewSurface = surface;	
	pthread_mutex_unlock(&mSurfaceLock);
}

void Camera::drawPreview(char *buffer) {
	if (mYuv420 == NULL) {
		mYuv420 = new char[mWidth * mHeight * 3 / 2];
	}

	Yvyu2Yuv420(buffer, mYuv420, mWidth, mHeight);
	render(mYuv420, mPreviewSurface, mWidth, mHeight);
}

void Camera::peekFrame(JNIEnv *env,jobject jCamera) {
	if (mFD < 0) {
		LOGD("camera not open, open first\n");
		return;
	}

	int r;
	fd_set fds;
	struct timeval tv;

	mPeekRun = true;
	while(mPeekRun) {
		
		FD_ZERO(&fds);
		FD_SET(mFD, &fds);

		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(mFD + 1, &fds, NULL, NULL, &tv);
		if (-1 == r) {
			if (EINTR == errno)
				continue;
			else {
			LOGD("Frame-peek is error.Reopening is reqired");
			}
		}

		if (0 == r) {
			LOGD("Frame-peek is time-out.Reopening is reqired\n");
			goto exit_tag;
		}

		memset(&mCurBufInfo, 0, sizeof mCurBufInfo);
		mCurBufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		mCurBufInfo.memory = V4L2_MEMORY_MMAP;

		// this operator below will change buf.index and (0 <= buf.index <= 3)
		if (0 > ioctl(mFD, VIDIOC_DQBUF, &mCurBufInfo)) {
			switch (errno) {
			case EAGAIN:
				goto exit_tag;
			case EIO:
				/* Could ignore EIO, see spec. */
				/* fall through */
			default:
				LOGD("VIDIOC_DQBUF is err.Reopening is reqired\n");
			}
		}

		// LOGD("buffer index:%d\n", mCurBufInfo.index);
		// LOGD("A Frame is readable\n");

		pthread_mutex_lock(&mSurfaceLock);
		if (mPreviewSurface.get() != NULL)
			drawPreview((char *)(mBufferArray[mCurBufInfo.index].start));	
		pthread_mutex_unlock(&mSurfaceLock);
		
		// env->CallVoidMethod(jCamera, onNewFrameMID);
		
		// Retrieve
		if (0 > ioctl(mFD, VIDIOC_QBUF, &mCurBufInfo)) {
			LOGD("Retrieve buf err\n");
			goto exit_tag;
		}
	}

exit_tag:	
	mPeekRun = false;

	closeCamera(env, jCamera);
}

void Camera::fillWithFrame(JNIEnv *env, jobject container) {
	jobject targetByteBuffer;
	jbyteArray targetBuffer;

	// fill field<timestamp> of container
	struct timeval timestamp;	
	gettimeofday(&timestamp, NULL);
	long long stampMilli = ( ((long long)timestamp.tv_sec) * 1000 \
								+ timestamp.tv_usec / 1000);
	env->SetLongField(container, gContainerClassInfo.stampID, stampMilli);

	// fill field<length> of container
	int length = mCurBufInfo.length;
	env->SetIntField(container, gContainerClassInfo.lenID, length);

	// fill field<target payload> of container
	targetByteBuffer = env->GetObjectField(container, 
					gContainerClassInfo.targetID);
	targetBuffer = (jbyteArray)env->CallObjectMethod(targetByteBuffer, 
					gByteBufferClassInfo.arrayID);
	
	signed char *ptr = (signed char *)env->GetByteArrayElements(targetBuffer, NULL);
	memcpy(ptr, mBufferArray[mCurBufInfo.index].start, length);
	env->ReleaseByteArrayElements(targetBuffer, ptr, 0);	
}

void Camera::closeCamera(JNIEnv *env, jobject jCamera) {
	mPeekRun = false;
	
	if (mFD < 0)
		return;
	
	close(mFD);
	mFD = -1;
	
	if (mYuv420 != NULL)
		delete [] mYuv420;

	env->CallVoidMethod(jCamera, gCameraClassInfo.onCameraClose);
}

JNIEXPORT void JNICALL
Java_com_hongdian_usbcamera_Camera_initNative
(JNIEnv *env, jobject object) {

	gCameraClassInfo.clazz = env->FindClass("com/hongdian/usbcamera/Camera");
	gCameraClassInfo.mDevPath = env->GetFieldID(gCameraClassInfo.clazz, 
										"mDevPath", "Ljava/lang/String;");	
	gCameraClassInfo.mNativeObject = env->GetFieldID(gCameraClassInfo.clazz,
														"mNativeObject", "I");
	gCameraClassInfo.onNewFrame = env->GetMethodID(gCameraClassInfo.clazz,
														"onNewFrame", "()V");
	gCameraClassInfo.onCameraClose = env->GetMethodID(gCameraClassInfo.clazz,
														"onCameraClose", "()V");

	gByteBufferClassInfo.clazz = env->FindClass("java/nio/ByteBuffer");
	gByteBufferClassInfo.arrayID = env->GetMethodID(gByteBufferClassInfo.clazz,
														"array", "()[B");	

	gContainerClassInfo.clazz = env->FindClass("com/hongdian/usbcamera/FrameContainer");
	gContainerClassInfo.stampID = env->GetFieldID(gContainerClassInfo.clazz,
													"timeStamp", "J");
	gContainerClassInfo.lenID = env->GetFieldID(gContainerClassInfo.clazz,
													"len", "I");
	gContainerClassInfo.targetID = env->GetFieldID(gContainerClassInfo.clazz,
													"target", "Ljava/nio/ByteBuffer;");
}

/*
 * Class:     com_hongdian_usbcamera_Camera
 * Method:    openCamera
 * Signature: ()I
 */
JNIEXPORT jint JNICALL 
Java_com_hongdian_usbcamera_Camera_openCamera
(JNIEnv *env, jobject object, jint width, jint height) {

	if (Camera::getCamera(env, object) == NULL) {
		const char *devPath = 
				Camera::getDevicePath(env, object);

		if (devPath != NULL) {
			int fd = -1;
			Camera *camera = 
				new Camera(devPath, width, height);
			
			if ( (fd = camera->openCamera()) > 0) {
				Camera::setCamera(env, object, camera);
				return fd;
			}		
		}					
	}

	return -1;
}

/*
 * Class:     com_hongdian_usbcamera_Camera
 * Method:    peekFrame
 * Signature: ()I
 */
JNIEXPORT jint JNICALL 
Java_com_hongdian_usbcamera_Camera_peekFrame
(JNIEnv *env, jobject object) {
	Camera *camera = Camera::getCamera(env, object);
	if (camera != NULL) {
		camera->peekFrame(env, object);
		return 0;
	}	

	return -1;
}

JNIEXPORT void JNICALL 
Java_com_hongdian_usbcamera_Camera_fillFrame
(JNIEnv *env, jobject object, jobject jContainer) {

	Camera *camera = Camera::getCamera(env, object);
	if (camera != NULL) {
		camera->fillWithFrame(env, jContainer);
	}	
}

/*
 * Class:     com_hongdian_usbcamera_Camera
 * Method:    closeCamera
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_com_hongdian_usbcamera_Camera_closeCamera
(JNIEnv *env, jobject object) {
	Camera *camera = Camera::getCamera(env, object);
	if (camera != NULL) {
		camera->closeCamera(env, object);
		delete camera;
		Camera::setCamera(env, object, NULL);
	}
}

/*
 * Class:     com_hongdian_usbcamera_Camera
 * Method:    nativeStartPreview
 * Signature: (Landroid/view/Surface;)V
 */
JNIEXPORT void JNICALL 
Java_com_hongdian_usbcamera_Camera_nativeStartPreview
(JNIEnv *env, jobject object, jobject jsurface) {
	Camera *camera = Camera::getCamera(env, object);
	if (camera != NULL) {
		camera->setPreviewSurface(env, jsurface);		
	}
}

/*
 * Class:     com_hongdian_usbcamera_Camera
 * Method:    nativeStopPreview
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_com_hongdian_usbcamera_Camera_nativeStopPreview
(JNIEnv *env, jobject object) {
	Camera *camera = Camera::getCamera(env, object);
	if (camera != NULL) {
		camera->setPreviewSurface(env, NULL);		
	}
}

