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

static sp<Surface> surface;

static int ALIGN(int x, int y) {
    // y must be a power of 2.
    return (x + y - 1) & ~(y - 1);
}

static void render(
        const void *data, size_t size, const sp<ANativeWindow> &nativeWindow,int width,int height) {
	ALOGE("[%s]%d",__FILE__,__LINE__);
    sp<ANativeWindow> mNativeWindow = nativeWindow;
    int err;
	int mCropWidth = width;
	int mCropHeight = height;
	
	int halFormat = HAL_PIXEL_FORMAT_YV12;//颜色空间
    int bufWidth = (mCropWidth + 1) & ~1;//按2对齐
    int bufHeight = (mCropHeight + 1) & ~1;

#if 0	
	CHECK_EQ(0,
            native_window_set_usage(
            mNativeWindow.get(),
            GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN
            | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP));

    CHECK_EQ(0,
            native_window_set_scaling_mode(
            mNativeWindow.get(),
            NATIVE_WINDOW_SCALING_MODE_SCALE_CROP));

    // Width must be multiple of 32???
	//很重要,配置宽高和和指定颜色空间yuv420
	//如果这里不配置好，下面deque_buffer只能去申请一个默认宽高的图形缓冲区
    CHECK_EQ(0, native_window_set_buffers_geometry(
                mNativeWindow.get(),
                bufWidth,
                bufHeight,
                halFormat));
#endif

	native_window_set_usage(
            mNativeWindow.get(),
            GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN
            | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP);


	native_window_set_scaling_mode(
            mNativeWindow.get(),
            NATIVE_WINDOW_SCALING_MODE_SCALE_CROP);


	native_window_set_buffers_geometry(
                mNativeWindow.get(),
                bufWidth,
                bufHeight,
                halFormat);
	
	
	ANativeWindowBuffer *buf;//描述buffer
	//申请一块空闲的图形缓冲区
    if ((err = native_window_dequeue_buffer_and_wait(mNativeWindow.get(),
            &buf)) != 0) {
        ALOGW("Surface::dequeueBuffer returned error %d", err);
        return;
    }

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    Rect bounds(mCropWidth, mCropHeight);

    void *dst;

#if 0
    CHECK_EQ(0, mapper.lock(//用来锁定一个图形缓冲区并将缓冲区映射到用户进程
                buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst));//dst就指向图形缓冲区首地址
#endif

	mapper.lock(//用来锁定一个图形缓冲区并将缓冲区映射到用户进程
                buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst);

    if (true) {
        size_t dst_y_size = buf->stride * buf->height;
        size_t dst_c_stride = ALIGN(buf->stride / 2, 16);//1行v/u的大小
        size_t dst_c_size = dst_c_stride * buf->height / 2;//u/v的大小
        
        memcpy(dst, data, dst_y_size + dst_c_size*2);//将yuv数据copy到图形缓冲区
    }

    // CHECK_EQ(0, mapper.unlock(buf->handle));

	mapper.unlock(buf->handle);

    if ((err = mNativeWindow->queueBuffer(mNativeWindow.get(), buf,
            -1)) != 0) {
        ALOGW("Surface::queueBuffer returned error %d", err);
    }
    buf = NULL;
}

static void nativeTest(){
	ALOGE("[%s]%d",__FILE__,__LINE__);
}

static jboolean
nativeSetVideoSurface(JNIEnv *env, jobject thiz, jobject jsurface){
	ALOGE("[%s]%d",__FILE__,__LINE__);
	surface = android_view_Surface_getSurface(env, jsurface);
	if(android::Surface::isValid(surface)){
		ALOGE("surface is valid ");
	}else {
		ALOGE("surface is invalid ");
		return false;
	}
	ALOGE("[%s][%d]\n",__FILE__,__LINE__);
	return true;
}

static void
nativeShowYUV(JNIEnv *env, jobject thiz, char *yuv,jint width,jint height){
	ALOGE("width = %d,height = %d",width,height);
	// jint len = env->GetArrayLength(yuvData);
	// ALOGE("len = %d",len);
	// jbyte *byteBuf = env->GetByteArrayElements(yuvData, 0);
	
	render(yuv, 0,surface,width,height);
}



class Camera {
#define BUFFER_NR			(4)
	struct buffer {
		void *start;
		size_t length;
	};
	
public:
	Camera(const char *devPath, jobject jCamera);
	Camera(const char *devPath, jobject jCamera, int cropWidth, int cropHeight);
	~Camera();
	
    int openCamera();
	
    void peekFrame(jobject jCamera);

	void fillWithFrame(jobject container);
	
	void closeCamera(jobject jCamera);

	int setWindowSize(int width, int height);
	
	void storeEnv(JNIEnv *env);

	void setPreviewSurface(JNIEnv *env, jobject jsurface);

	static const char *getCameraDevicePath(JNIEnv *env, jobject camera);

	static void frameYUYV2RGBA8888(int *out, char *buffer, int width, int height);

	static inline int YUV2RGBA888(int y, int u, int v);
	
private:
	void drawPreviewFrame(char *buffer);
	
	int *mARGB;	

	char *yuv420;

	
	Surface *mPreviewSurface;
	ANativeWindow_Buffer mPreviewDrawBuffer;
	pthread_mutex_t mSurfaceLock;
	
	const char *mCameraDevPath;
	bool mPeekRun;
	int mFD;

	int mWidth;
	int mHeight;
	
	struct v4l2_buffer mCurBufInfo;
	struct buffer mBufferArray[BUFFER_NR];

	JNIEnv *nestedEnv;
	
	jclass cameraClass;
	
	// method id of bytebuffer.array
	jmethodID arrayMethodID;
	
	jfieldID stampID;
	jfieldID sizeID;
	jfieldID targetID;
};

class CameraHolder {
public:
	static CameraHolder &globalInstance();

	void putCamera(string &key, Camera *);
    Camera *getCamera(string &key);	
	void removeCamera(string &key);

private:
	CameraHolder();
	~CameraHolder();

	static CameraHolder *single;

	static int ret;
	static int init_lock();
	static pthread_mutex_t mSingleLock;
	
	pthread_mutex_t mCameraLock;
    std::map<string, Camera *> mOpenedCameras;
};

pthread_mutex_t CameraHolder::mSingleLock;
int CameraHolder::ret = CameraHolder::init_lock();
CameraHolder *CameraHolder::single;

int CameraHolder::init_lock() {
	return pthread_mutex_init(&mSingleLock, NULL);
}

CameraHolder &CameraHolder::globalInstance() {

	if (single == NULL) {
		pthread_mutex_lock(&mSingleLock);
		if (single == NULL)			// double check here
			single = new CameraHolder();	
		pthread_mutex_unlock(&mSingleLock);
	}

	return *single;
}

CameraHolder::CameraHolder() {
	pthread_mutex_init(&mCameraLock, NULL);
}

CameraHolder::~CameraHolder() {
	pthread_mutex_destroy(&mCameraLock);
}

void CameraHolder::putCamera(string &key, Camera *camera){
	pthread_mutex_lock(&mCameraLock);
	mOpenedCameras.insert(std::make_pair(key, camera));
	pthread_mutex_unlock(&mCameraLock);
}

Camera *CameraHolder::getCamera(string &key) {
	std::map<string, Camera*>::iterator i;

	pthread_mutex_lock(&mCameraLock);
	i = mOpenedCameras.find(key);
	if (i != mOpenedCameras.end()) {
		pthread_mutex_unlock(&mCameraLock);
		return i->second;
	}
	pthread_mutex_unlock(&mCameraLock);

	return NULL;
}

void CameraHolder::removeCamera(string &key) {
	pthread_mutex_lock(&mCameraLock);
	mOpenedCameras.erase(key);
	pthread_mutex_unlock(&mCameraLock);
}

static Surface *getNativeSurface(JNIEnv* env, jobject jsurface) {
    jclass clazz = env->FindClass("android/view/Surface");
    jfieldID field_surface;

    field_surface = env->GetFieldID(clazz, "mNativeObject", "I");
    if (field_surface == NULL)
    {
        return NULL;
    }


	Surface *surface = reinterpret_cast<Surface *>(env->GetIntField(jsurface, field_surface));

	LOGD("getNativeSurface OK, surface=%p\n", surface);
	
    return surface;
}

void Camera::storeEnv(JNIEnv *env) {
	jclass containerClass;	
	jclass bytebufferClass;
	nestedEnv = env;
	
	cameraClass = nestedEnv->FindClass("com/hongdian/usbcamera/Camera");
	containerClass = nestedEnv->FindClass("com/hongdian/usbcamera/FrameContainer");
	bytebufferClass = nestedEnv->FindClass("java/nio/ByteBuffer");

	arrayMethodID = nestedEnv->GetMethodID(bytebufferClass, "array", "()[B");

	stampID = nestedEnv->GetFieldID(containerClass, "timeStamp", "J");
	sizeID = nestedEnv->GetFieldID(containerClass, "size", "I");
	targetID = nestedEnv->GetFieldID(containerClass, "target", "Ljava/nio/ByteBuffer;");
}

const char *Camera::getCameraDevicePath(JNIEnv *env, jobject camera) {
	jfieldID fid;
	jstring devPath;

	jclass cameraClass = env->FindClass("com/hongdian/usbcamera/Camera");	
	fid = env->GetFieldID(cameraClass, "mCameraDevPath", "Ljava/lang/String;");
	devPath = (jstring)env->GetObjectField(camera, fid);
	return env->GetStringUTFChars(devPath, NULL);
}

Camera::Camera(const char *devPath, jobject jCamera) 
	: mCameraDevPath(devPath)
	, mPeekRun(false)
	, mFD(-1)
	, mWidth(1280)
	, mHeight(720)
	, mPreviewSurface(NULL)
	, mARGB(NULL)
	, yuv420(NULL){


	pthread_mutex_init(&mSurfaceLock, NULL);

}

Camera::Camera
(const char *devPath, jobject jCamera, int cropWidth, int cropHeight) 
	: mCameraDevPath(devPath)
	, mPeekRun(false)
	, mFD(-1)
	, mWidth(cropWidth)
	, mHeight(cropHeight)
	, mPreviewSurface(NULL)
	, mARGB(NULL)
	, yuv420(NULL){	

	pthread_mutex_init(&mSurfaceLock, NULL);
}

Camera::~Camera(){
	
}

int Camera::setWindowSize(int width, int height){
	mWidth = width;
	mHeight = height;
}


/*
 * no scale
 */
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
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;//V4L2_PIX_FMT_YUYV V4L2_PIX_FMT_MJPEG
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

    LOGD("VIDIOC_G_PARM %d %d\n", parm.parm.capture.timeperframe.denominator, parm.parm.capture.timeperframe.numerator );


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

void Camera::setPreviewSurface(JNIEnv *env,jobject jsurface) {
	Surface *surface = getNativeSurface(env, jsurface);

	pthread_mutex_lock(&mSurfaceLock);
	mPreviewSurface = surface;
	pthread_mutex_unlock(&mSurfaceLock);
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

static void  yuyv_2_yuv420(char * yuyv, char *yuv420, int width, int height)  
{  
    int i,j;  
    char *Y,*U,*V;  
    char y1,y2,u,v;  
      
    Y = yuv420;  
    U = yuv420 + width * height;
    V = U + width * height / 4;  
      
    for(i = 0; i < height; i++) {  
		for(j = 0; j < width / 2; j++) {
	        y1 = *(yuyv + (i * width / 2 + j) * 4);  
	        v  = *(yuyv + (i * width / 2 + j) * 4 + 1);  
	        y2 = *(yuyv + (i * width / 2 + j) * 4 + 2);  
	        u  = *(yuyv + (i * width / 2 + j) * 4 + 3);  
	      
		    *Y++=y1;  
		    *Y++=y2;  
		      
		    if(i % 2 == 0) {  
				*U++=u;
				*V++=v;  
		    }  
		}  
    }  
  
}  


void Camera::drawPreviewFrame(char *buffer) {

	if (yuv420 == NULL) {
		yuv420 = new char[mWidth * mHeight * 3 / 2];
	}

	yuyv_2_yuv420(buffer, yuv420, mWidth, mHeight);	
	
	nativeShowYUV(nestedEnv,  NULL, yuv420, mWidth, mHeight);


#if 0
	if (mARGB == NULL) {
		mARGB = new int[mWidth * mHeight];
	}

	ARect rect;

	rect.left = 0;
	rect.top = 0;
	rect.right = rect.left + 100;
	rect.bottom = rect.bottom + 100;
	

	LOGD("drawPreviewFrame buffer=%p mARGB=%p\n", buffer, mARGB);

	frameYUYV2RGBA8888(mARGB, buffer, mWidth, mHeight);

	mPreviewSurface->getIGraphicBufferProducer();

    
	LOGD("mPreviewSurface =%p\n", mPreviewSurface);	
	
	mPreviewSurface->lock(&mPreviewDrawBuffer, &rect);	
	LOGD("drawview", "%d %d %d %d %p\n", \
		mPreviewDrawBuffer.width, mPreviewDrawBuffer.height,\
		mPreviewDrawBuffer.stride, mPreviewDrawBuffer.format,\
		mPreviewDrawBuffer.bits);
	mPreviewSurface->unlockAndPost();	
#endif

}

void Camera::peekFrame(jobject jCamera) {

	if (mFD < 0) {
		LOGD("camera not open, open first\n");
		return;
	}

	int r;
	fd_set fds;
	struct timeval tv;

	jmethodID onNewFrameMID = \
		nestedEnv->GetMethodID(cameraClass, "onNewFrame", "()V");	

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

		LOGD("buffer index:%d\n", mCurBufInfo.index);

#if 0
		pthread_mutex_lock(&mSurfaceLock);
		if (mPreviewSurface != NULL) {
			drawPreviewFrame((char *)(mBufferArray[mCurBufInfo.index].start));
		}	
		pthread_mutex_unlock(&mSurfaceLock);
#endif

		if (surface != NULL)
			drawPreviewFrame((char *)(mBufferArray[mCurBufInfo.index].start));	
		
		// nestedEnv->CallVoidMethod(jCamera, onNewFrameMID);
		// LOGD("A Frame is readable\n");
		
		// Retrieve
		if (0 > ioctl(mFD, VIDIOC_QBUF, &mCurBufInfo)) {
			LOGD("Retrieve buf err\n");
			goto exit_tag;
		}
	}

exit_tag:	
	mPeekRun = false;

	closeCamera(jCamera);
	
}

void Camera::fillWithFrame(jobject container) {
	jobject targetByteBuffer;
	jbyteArray targetBuffer;

	// fill field<timestamp> of container
	struct timeval timestamp;	
	gettimeofday(&timestamp, NULL);
	long long stampMilli = ( ((long long)timestamp.tv_sec) * 1000 \
								+ timestamp.tv_usec / 1000);
	nestedEnv->SetLongField(container, stampID, stampMilli);

	// fill field<length> of container
	int length = mCurBufInfo.length;
	nestedEnv->SetIntField(container, sizeID, length);

	// fill field<target payload> of container
	targetByteBuffer = nestedEnv->GetObjectField(container, targetID);
	targetBuffer = (jbyteArray)nestedEnv->CallObjectMethod(targetByteBuffer, arrayMethodID);
	
	signed char *ptr = (signed char *)nestedEnv->GetByteArrayElements(targetBuffer, NULL);
	memcpy(ptr, mBufferArray[mCurBufInfo.index].start, length);
	nestedEnv->ReleaseByteArrayElements(targetBuffer, ptr, 0);
}


void Camera::closeCamera(jobject jCamera) {
	if (mFD < 0)
		return;

	mPeekRun = false;

	close(mFD);
	mFD = -1;

	jmethodID onCloseMethodID;
	onCloseMethodID = nestedEnv->GetMethodID(cameraClass, "onCameraClose", "()V");
	nestedEnv->CallVoidMethod(jCamera, onCloseMethodID);
}

/*
 * Class:     com_hongdian_usbcamera_Camera
 * Method:    openCamera
 * Signature: ()I
 */
JNIEXPORT jint JNICALL 
Java_com_hongdian_usbcamera_Camera_openCamera
(JNIEnv *env, jobject object) {

	const char *cameraToken = NULL;
	
	cameraToken = Camera::getCameraDevicePath(env, object);
	string cameraKey(cameraToken);
	Camera *camera = CameraHolder::globalInstance()\
								.getCamera(cameraKey);
	if (NULL == camera) {
		camera = new Camera(cameraToken, object);

		CameraHolder::globalInstance()\
						.putCamera(cameraKey, camera);
		return camera->openCamera();
	} 

	// can not reopen
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

	const char *cameraToken = Camera::getCameraDevicePath(env, object);
	string cameraKey(cameraToken);
	Camera *camera = CameraHolder::globalInstance()\
									.getCamera(cameraKey);

	if (camera != NULL) {
		camera->storeEnv(env);
		camera->peekFrame(object);
		return 0;
	}
	
	return -1;
}

JNIEXPORT void JNICALL 
Java_com_hongdian_usbcamera_Camera_fillFrame
(JNIEnv *env, jobject object, jobject jContainer) {

	const char *cameraToken = Camera::getCameraDevicePath(env, object);
	string cameraKey(cameraToken);
	Camera *camera = CameraHolder::globalInstance()\
									.getCamera(cameraKey);
	if (camera != NULL) {
		camera->fillWithFrame(jContainer);
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
	const char *cameraToken = Camera::getCameraDevicePath(env, object);
	string cameraKey(cameraToken);
	Camera *camera = CameraHolder::globalInstance()\
									.getCamera(cameraKey);
	if (camera != NULL) {
		camera->closeCamera(object);
		CameraHolder::globalInstance().removeCamera(cameraKey);
		delete camera;
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

#if 0
	const char *cameraToken = Camera::getCameraDevicePath(env, object);
	string cameraKey(cameraToken);
	Camera *camera = CameraHolder::globalInstance()\
									.getCamera(cameraKey);
	if (camera != NULL) {
		camera->setPreviewSurface(env, jsurface);
	}
#endif

	nativeSetVideoSurface(env, object, jsurface);
}

/*
 * Class:     com_hongdian_usbcamera_Camera
 * Method:    nativeStopPreview
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_com_hongdian_usbcamera_Camera_nativeStopPreview
(JNIEnv *env, jobject object) {

#if 0
	LOGD("nativeStartPreview\n");

	const char *cameraToken = Camera::getCameraDevicePath(env, object);
	string cameraKey(cameraToken);
	Camera *camera = CameraHolder::globalInstance()\
									.getCamera(cameraKey);
	if (camera != NULL) {
		camera->setPreviewSurface(env, NULL);
	}
#endif

	surface = NULL;
}



