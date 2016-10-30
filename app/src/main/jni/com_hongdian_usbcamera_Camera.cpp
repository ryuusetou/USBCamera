//
// Created by ryuusetou on 2016/10/19.
//

#include <list>
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
#include <utils/misc.h>
#include <android_runtime/AndroidRuntime.h>



#include <ui/GraphicBufferMapper.h>
#include <android_runtime/android_view_Surface.h>


#include "com_hongdian_usbcamera_Camera.h"

//#define LOG_TAG "USBCameraJNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "USBCameraJNI", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "USBCameraJNI", __VA_ARGS__)

using namespace std;
using namespace android;

static int ALIGN(int x, int y) {
    return (x + y - 1) & ~(y - 1);
}

static void 
copyFrameCenterGravity
(void *dest, const void *src, int dest_w, int dest_h, int src_w, int src_h, char bpp_shift) {	
	if (src_w < dest_w || src_h < dest_h) {
		LOGD("This resolution don't be supported.w%d h%d", dest_w, dest_h);
		return;
	}

	int horizontal_margin = (src_w - dest_w + 1) >> 1;
	int vertical_margin = (src_h - dest_h + 1) >> 1;

	// LOGD("hm:%d vm:%d", horizontal_margin, vertical_margin);

	unsigned long src_off = 
		( (vertical_margin * src_w) + horizontal_margin) << bpp_shift;

	unsigned long copy_once = dest_w << bpp_shift;

	unsigned long copyed = 0;
	
	
	for (int i = 0; i < dest_h; i++) {
		// LOGD("copped:%ld src_offset:%ld", copyed, src_off);
		
		memcpy(dest + copyed, src + src_off, copy_once);
		copyed += copy_once;
		
		src_off += (copy_once + (horizontal_margin << (bpp_shift + 1)));
	}	
}

static struct {
	jclass clazz;
	
	jfieldID mDevPath;
	jfieldID mNativeObject;

	jmethodID notifyFirstCapture;
	jmethodID notifyCaptureStop;
	jmethodID notifyCameraError;
} gCameraClassInfo;

class Camera {
	
#define BUFFER_NR						(4)
struct buffer {
	void *start;
	size_t length;
};
	
public:
	
	Camera(const char *devPath, int cropWidth, int cropHeight);
	
	~Camera();
	
    int openCamera();
	
    void startCapture(JNIEnv *env, jobject jcamera);

    void stopCapture();
	
	// void closeCamera(JNIEnv *env, jobject jCamera);
	
	void setPreviewSurface(JNIEnv *env, jobject jsurface);

	void addEncoderSurface(JNIEnv *env, jobject jsurface);

	void removeEncoderSurface(JNIEnv *env, jobject jsurface);

	static const char *getDevicePath(JNIEnv *env, jobject camera);

	static Camera *getCamera(JNIEnv *env, jobject camera);

	static void *setCamera(JNIEnv *env, jobject jcamera, Camera *camera);

	static void Yvyu2RGBA(int *rgbData, char *yvyu, int w, int h);

	static void  Yvyu2Yuv420(char * yvyu, char *yuv420, int width, int height);
	
private:
	
	void drawPreviewSurface();
	
	void drawEncoderSurface();
	
	char *mYuv420sp;
	
	char *mRGBA8888;

	sp<Surface> mPreviewSurface;
	
	std::list<sp<Surface> > mEncoderSurfaces;
	
	char *mCameraDevPath;
	
	bool mPeekRun;
	
	int mFD;

	int mWidth;
	
	int mHeight;
	
	struct v4l2_buffer mCurBufInfo;
	
	struct buffer mBufferArray[BUFFER_NR];

	pthread_mutex_t mPreviewLock;
	pthread_mutex_t mEncoderLock;
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
 
void Camera::Yvyu2RGBA(int *rgbData, char *yvyu, int w, int h) {
	int Y0 = 0;
	int Y1 = 0;
	int Cr = 0;
	int Cb = 0;

	int R = 0;
	int G = 0;
	int B = 0;	
	
	int pixPtr = 0;

	int diff_r = 0;

	int diff_g = 0;

	int diff_b = 0;

	char *ptr = yvyu;

	for (int j = 0; j < h; j++) {				
		for (int i = 0; i < w / 2; i++) {			
			Y0 = *ptr++;
			Cb = *ptr++;
 			
			Y1 = *ptr++;
			Cr = *ptr++;		

			if(Cb < 0) Cb += 127; else Cb -= 128;
			if(Cr < 0) Cr += 127; else Cr -= 128;

			diff_r = Cr + (Cr >> 2) + (Cr >> 3) + (Cr >> 5);			
			diff_g = 0 - (Cb >> 2) + (Cb >> 4) + (Cb >> 5) - (Cr >> 1) + (Cr >> 3) + (Cr >> 4) + (Cr >> 5);			
			diff_b = Cb + (Cb >> 1) + (Cb >> 2) + (Cb >> 6);		

			R = Y0 + diff_r;//1.406*~1.403
			if(R < 0) R = 0; else if(R > 255) R = 255;
			G = Y0 + diff_g;//
			if(G < 0) G = 0; else if(G > 255) G = 255;
			B = Y0 + diff_b;//1.765~1.770
			if(B < 0) B = 0; else if(B > 255) B = 255;
			
			rgbData[pixPtr++] = 0xff000000 + (B << 16) + (G << 8) + (R << 0);

			R = Y1 + diff_r;//1.406*~1.403
			if(R < 0) R = 0; else if(R > 255) R = 255;
			G = Y1 + diff_g;//
			if(G < 0) G = 0; else if(G > 255) G = 255;
			B = Y1 + diff_b;//1.765~1.770
			if(B < 0) B = 0; else if(B > 255) B = 255;

			rgbData[pixPtr++] = 0xff000000 + (B << 16) + (G << 8) + (R << 0);
		}
	}
}

Camera::Camera
	(const char *devPath, int cropWidth, int cropHeight)
	: mPeekRun(false)
	, mFD(-1)
	, mWidth(cropWidth)
	, mHeight(cropHeight)
	, mYuv420sp(NULL)
	, mRGBA8888(NULL) {	

	if (devPath != NULL )
		mCameraDevPath = strdup(devPath);
	else
		mCameraDevPath = NULL;

	pthread_mutex_init(&mPreviewLock, NULL);
	pthread_mutex_init(&mEncoderLock, NULL);
}

Camera::~Camera() {
	if (mYuv420sp != NULL) {
		delete [] mYuv420sp;
		mYuv420sp = NULL;
	}

	if (mRGBA8888!= NULL) {
		delete [] mRGBA8888;
		mRGBA8888 = NULL;
	}

	if (mFD > 0) {
		close(mFD);
		mFD = -1;
	}

	pthread_mutex_destroy(&mEncoderLock);
	pthread_mutex_destroy(&mPreviewLock);
	
	if (mCameraDevPath != NULL) {
		free(mCameraDevPath);
		mCameraDevPath = NULL;
	}
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
		pthread_mutex_lock(&mPreviewLock);
		mPreviewSurface = NULL;	
		pthread_mutex_unlock(&mPreviewLock);
		return;
	}
	
	sp<Surface> surface =
		android_view_Surface_getSurface(env, jsurface);
	if(!android::Surface::isValid(surface)){
		ALOGE("surface is invalid, setPreviewSurface failed!");
		return;
	}
	
	pthread_mutex_lock(&mPreviewLock);
	mPreviewSurface = surface;	
	pthread_mutex_unlock(&mPreviewLock);
}

void Camera::addEncoderSurface(JNIEnv *env, jobject jsurface) {
	if (jsurface == NULL) {
		LOGD("encoder surface is null");
		return;
	}

	LOGD("addEncoderSurface !");
	
	sp<Surface> surface =
		android_view_Surface_getSurface(env, jsurface);
	if(!android::Surface::isValid(surface)){
		ALOGE("surface is invalid, setPreviewSurface failed!");
		return;
	}
	
	pthread_mutex_lock(&mEncoderLock);
	mEncoderSurfaces.push_back(surface);
	pthread_mutex_unlock(&mEncoderLock);

	LOGD("addEncoderSurface size:%d", mEncoderSurfaces.size());
}

void Camera::removeEncoderSurface(JNIEnv *env, jobject jsurface) {
	if (jsurface == NULL) {
		return;
	}
	
	sp<Surface> surface =
		android_view_Surface_getSurface(env, jsurface);
	if(!android::Surface::isValid(surface)){
		ALOGE("surface is invalid, setPreviewSurface failed!");
		return;
	}
	
	pthread_mutex_lock(&mEncoderLock);
	mEncoderSurfaces.remove(surface);
	pthread_mutex_unlock(&mEncoderLock);
}

void Camera::drawPreviewSurface() {
	int err;
	ANativeWindowBuffer *buf;
	sp<ANativeWindow> nativeWindow = mPreviewSurface; 
	int halFormat = HAL_PIXEL_FORMAT_YV12;
	int bufWidth = (mWidth + 1) & ~1;
	int bufHeight = (mHeight + 1) & ~1;

	pthread_mutex_lock(&mPreviewLock);
	if (mPreviewSurface.get() == NULL) {
		if (mYuv420sp != NULL) {
			delete [] mYuv420sp;
			mYuv420sp = NULL;
		}
		pthread_mutex_unlock(&mPreviewLock);

		LOGD("no preview windows!");
		return;
	}
	
	if (mYuv420sp == NULL) {
		mYuv420sp = new char[mWidth * mHeight * 3 / 2];
	}
	Yvyu2Yuv420((char *)mBufferArray[mCurBufInfo.index].start, mYuv420sp, 
													mWidth, mHeight);	

	native_window_set_usage(nativeWindow.get(),
			GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN
			| GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP);

	native_window_set_buffers_dimensions(nativeWindow.get(), 
			bufWidth, bufHeight);

	native_window_set_scaling_mode(nativeWindow.get(),
			NATIVE_WINDOW_SCALING_MODE_SCALE_CROP);

	native_window_set_buffers_format(nativeWindow.get(), 
			halFormat); 
	
	if ((err = native_window_dequeue_buffer_and_wait(
			nativeWindow.get(),
			&buf)) != 0) {
		ALOGW("Surface::dequeueBuffer returned error %d", err);
		pthread_mutex_unlock(&mPreviewLock);
		return;
	}

	GraphicBufferMapper &mapper = GraphicBufferMapper::get();
	Rect bounds(mWidth, mHeight);
	void *dst;

	mapper.lock(buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst);
	if (true) {
		size_t dst_y_size = buf->stride * buf->height;
		size_t dst_c_stride = ALIGN(buf->stride / 2, 16);	
		size_t dst_c_size = dst_c_stride * buf->height / 2;

		// LOGD("%d %d %d %d", buf->format, buf->width, buf->height, buf->stride);	
		memcpy(dst, mYuv420sp, dst_y_size + dst_c_size * 2);
	}
	mapper.unlock(buf->handle);

	if ((err = nativeWindow->queueBuffer(nativeWindow.get(), buf, -1)) != 0) {
		ALOGW("Surface::queueBuffer returned error %d", err);
	}
	pthread_mutex_unlock(&mPreviewLock);
}
 
void Camera::drawEncoderSurface() {
	pthread_mutex_lock(&mEncoderLock);
	if (mEncoderSurfaces.size() == 0) {	
		pthread_mutex_unlock(&mEncoderLock);
		if (mRGBA8888 != NULL) {
			delete [] mRGBA8888;
			mRGBA8888 = NULL;
		}
		// LOGD("mEncoderSurfaces is empty");
		return;
	}
	
	if (mRGBA8888 == NULL) {
		mRGBA8888 = new char[mWidth * mHeight * 4];
	}

	Yvyu2RGBA((int *)mRGBA8888, (char *)mBufferArray[mCurBufInfo.index].start, 
			  mWidth, mHeight);	

	ANativeWindow_Buffer buffer;		
	sp<ANativeWindow> nativeWindow;
	std::list<sp<Surface> >::iterator i;
	for (i = mEncoderSurfaces.begin(); i != mEncoderSurfaces.end(); i++) {
		nativeWindow = *i;
		if (ANativeWindow_lock(nativeWindow.get(), &buffer, NULL) == 0) {
			if (mWidth == buffer.width && mHeight == buffer.height) {
				memcpy(buffer.bits, mRGBA8888, mWidth * mHeight * 4);
			} else {		
				copyFrameCenterGravity(buffer.bits, mRGBA8888, 
							buffer.width, buffer.height, mWidth, mHeight, 2);
			}
			
			ANativeWindow_unlockAndPost(nativeWindow.get());
		}		
	}

	pthread_mutex_unlock(&mEncoderLock);
}

void Camera::startCapture(JNIEnv *env, jobject jcamera) {
	int r;
	bool firstCapture = true;
	fd_set fds;
	struct timeval tv;
	
	if (mFD < 0) {
		LOGD("camera not open, open first\n");
		return;
	}

	mPeekRun = true;
	while(mPeekRun) {
		
		FD_ZERO(&fds);
		FD_SET(mFD, &fds);

		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(mFD + 1, &fds, NULL, NULL, &tv);
		if (mPeekRun == false) {
			break;
		}
		
		if (-1 == r) {
			if (EINTR == errno)
				continue;
			else {
				LOGD("Frame-peek is error.Reopening is reqired");
				goto CAMERA_ERR;
			}
		} else if (0 == r) {
			LOGD("Frame-peek is time-out.Reopening is reqired\n");
			goto CAMERA_ERR;
		}

		memset(&mCurBufInfo, 0, sizeof mCurBufInfo);
		mCurBufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		mCurBufInfo.memory = V4L2_MEMORY_MMAP;

		// this operator below will change buf.index and (0 <= buf.index <= 3)
		if (0 > ioctl(mFD, VIDIOC_DQBUF, &mCurBufInfo)) {
			switch (errno) {
			case EAGAIN:
				goto CAMERA_ERR;
			case EIO:
				/* Could ignore EIO, see spec. */
				/* fall through */
			default:
				LOGD("VIDIOC_DQBUF is err.Reopening is reqired\n");
			}
		}

		if (firstCapture) {
			struct timeval time;
			long long timestampUS = 0;

			LOGD("firstCapture got!");
			
			gettimeofday(&time, NULL);
			timestampUS = ( (long long)time.tv_sec) * 1000 * 1000 + time.tv_usec;
			env->CallVoidMethod(jcamera, gCameraClassInfo.notifyFirstCapture, timestampUS);
			firstCapture = false;
		}
		
		drawPreviewSurface();
		
		drawEncoderSurface();
		
		// Retrieve
		if (0 > ioctl(mFD, VIDIOC_QBUF, &mCurBufInfo)) {
			LOGD("Retrieve buf err\n");
			goto CAMERA_ERR;
		}
	}

	mPeekRun = false;
	env->CallVoidMethod(jcamera, gCameraClassInfo.notifyCaptureStop);
	return;

CAMERA_ERR:
	close(mFD);
	mFD = -1;
	
	env->CallVoidMethod(jcamera, gCameraClassInfo.notifyCameraError);
}

void Camera::stopCapture() {
	mPeekRun = false;	
}

static void nativeAttachNative
(JNIEnv *env, jobject object, jint width, jint height) {
	if (Camera::getCamera(env, object) == NULL) {
		const char *devPath = 
				Camera::getDevicePath(env, object);

		if (devPath != NULL) {
			Camera *camera = 
				new Camera(devPath, width, height);
			
			Camera::setCamera(env, object, camera);
		}					
	}
}

static jint nativeOpenCamera
(JNIEnv *env, jobject object) {
	int fd = -1;

	Camera *camera = Camera::getCamera(env, object);
	if (camera != NULL) {
		if ( (fd = camera->openCamera()) > 0) {
			return 0;
		}
	}

	return -1;	
}

static void  nativeStartCapture
(JNIEnv *env, jobject object) {

	Camera *camera = Camera::getCamera(env, object);
	if (camera != NULL) {
		camera->startCapture(env, object);		
	}
}

static void  nativeStopCapture
(JNIEnv *env, jobject object) {
	Camera *camera = Camera::getCamera(env, object);
	if (camera != NULL) {
		camera->stopCapture();		
	}
}

static void nativeReleaseCamera
(JNIEnv *env, jobject object) {
	Camera *camera = Camera::getCamera(env, object);
	if (camera != NULL) {
		camera->stopCapture();
		delete camera;

		Camera::setCamera(env, object, NULL);
	}
}

static void nativeSetPreview
(JNIEnv *env, jobject object, jobject jsurface) {
	Camera *camera = Camera::getCamera(env, object);
	if (camera != NULL) {
		camera->setPreviewSurface(env, jsurface);		
	}
}

static void nativeAddEncoder
(JNIEnv *env, jobject object, jobject jsurface) {	
	Camera *camera = Camera::getCamera(env, object);
	if (camera != NULL) {
		camera->addEncoderSurface(env, jsurface);		
	} 
}

static void nativeRemoveEncoder
(JNIEnv *env, jobject object, jobject jsurface) {
	Camera *camera = Camera::getCamera(env, object);
	if (camera != NULL) {
		camera->removeEncoderSurface(env, jsurface);		
	}
}

#define PACKAGE_PATH			"com/hongdian/usbcamera/"
#define CLASS_NAME				"Camera"

void initNativeClassInfo(JNIEnv *env) {
	gCameraClassInfo.clazz = env->FindClass(PACKAGE_PATH CLASS_NAME);
	
	gCameraClassInfo.mDevPath = env->GetFieldID(gCameraClassInfo.clazz, 
										"mDevPath", "Ljava/lang/String;");
	
	gCameraClassInfo.mNativeObject = env->GetFieldID(gCameraClassInfo.clazz,
														"mNativeObject", "I");

	gCameraClassInfo.notifyFirstCapture = env->GetMethodID(gCameraClassInfo.clazz, 
										"notifyFirstCapture",  "(J)V");
	
	gCameraClassInfo.notifyCaptureStop = env->GetMethodID(gCameraClassInfo.clazz, 
										"notifyCaptureStop",  "()V");

	gCameraClassInfo.notifyCameraError = env->GetMethodID(gCameraClassInfo.clazz, 
										"notifyCameraError",  "()V");	
}

static JNINativeMethod gMethods[] = {
	{"attachNative", "(II)V", (void *)nativeAttachNative},
    {"openCamera", "()I", (void *)nativeOpenCamera},
	{"startCapture", "()V", (void *)nativeStartCapture},
	{"stopCapture", "()V", (void *)nativeStopCapture},
	{"releaseCamera", "()V", (void *)nativeReleaseCamera},
	{"setPreview", "(Landroid/view/Surface;)V", (void *)nativeSetPreview},
	{"addEncoder", "(Landroid/view/Surface;)V", (void *)nativeAddEncoder},
	{"removeEncoder", "(Landroid/view/Surface;)V", (void *)nativeRemoveEncoder},
};

jint JNI_OnLoad(JavaVM * vm,void * reserved) {
	JNIEnv *env;
	int	result = -1;
	
	if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("ERROR: GetEnv failed\n");
		goto bail;
    }

	initNativeClassInfo(env);	
	if (AndroidRuntime::registerNativeMethods(env, 
						PACKAGE_PATH CLASS_NAME,
						gMethods, NELEM(gMethods))) {
		ALOGE("ERROR: registerNativeMethods failed\n");
		goto bail;
	}

	result = JNI_VERSION_1_4;
bail:
	return result;
}

