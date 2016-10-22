//
// Created by ryuusetou on 2016/10/19.
//

#include <map>
#include <iostream>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <android/log.h>

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


#include "com_hongdian_usbcamera_Camera.h"

#define LOG_TAG "USBCameraJNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

using namespace std;

class Camera {
#define BUFFER_NR			(4)
	struct buffer {
		void *start;
		size_t length;
	};
	
public:
	Camera(const char *devPath, jobject jCamera);
	~Camera();
	
    int openCamera();
	
    void peekFrame();

	void fillFrame(jobject frameContainer);
	
	void closeCamera();

	int setWindowSize(int width, int height);

	static void init(JNIEnv *env);
	static const char *getCameraDevicePath(jobject &);

private:
	jobject mJCamera;
	const char *mCameraDevPath;
	bool mPeekRun;
	int mFD;

	int mWidth;
	int mHeight;
	
	struct v4l2_buffer mCurBufInfo;
	struct buffer mBufferArray[BUFFER_NR];

	static JNIEnv *nestedEnv;
	
	static jclass cameraClass;
	static jclass containerClass;
	static jclass bytebufferClass;

	// method id of bytebuffer.array
	static jmethodID arrayMethodID;
	
	static jfieldID stampID;
	static jfieldID sizeID;
	static jfieldID targetID;
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

JNIEnv *Camera::nestedEnv;
jclass Camera::cameraClass;

jclass Camera::containerClass;
jclass Camera::bytebufferClass;

jmethodID Camera::arrayMethodID;
jfieldID Camera::stampID;
jfieldID Camera::sizeID;
jfieldID Camera::targetID;

void Camera::init(JNIEnv *env) {
	nestedEnv = env;
	
	cameraClass = nestedEnv->FindClass("com/hongdian/usbcamera/Camera");
	containerClass = nestedEnv->FindClass("com/hongdian/usbcamera/FrameContainer");
	bytebufferClass = nestedEnv->FindClass("java/nio/ByteBuffer");

	arrayMethodID = nestedEnv->GetMethodID(bytebufferClass, "array", "()[B");

	stampID = nestedEnv->GetFieldID(containerClass, "timeStamp", "J");
	sizeID = nestedEnv->GetFieldID(containerClass, "size", "I");
	targetID = nestedEnv->GetFieldID(containerClass, "size", "Ljava/nio/ByteBuffer;");
}

const char *Camera::getCameraDevicePath(jobject &camera) {
	if (nestedEnv == NULL || cameraClass == NULL) {
		LOGD("Do Camera::init first\n");
		return NULL;
	}
	
	jfieldID fid;
	jstring devPath;
	
	fid = nestedEnv->GetFieldID(cameraClass, "mCameraDevPath", "Ljava/lang/String;");
	devPath = (jstring)nestedEnv->GetObjectField(camera, fid);
	return nestedEnv->GetStringUTFChars(devPath, NULL);
}

Camera::Camera(const char *devPath, jobject jCamera) 
	: mCameraDevPath(devPath)
	, mJCamera(jCamera)
	, mPeekRun(false)
	, mFD(-1)
	, mWidth(1280)
	, mHeight(720) {

	

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
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
	if (ioctl(mFD, VIDIOC_S_FMT, &fmt) < 0) {
		LOGD("VIDIOC_S_FMT failed\n");
		goto exit_tag;
	}

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

	for (int i = 0; i < VIDIOC_QUERYBUF; ++i) {
		struct v4l2_buffer buf;

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (0 > ioctl(mFD, VIDIOC_QBUF, &buf)) {
			LOGD("VIDIOC_QBUF failed\n");
			goto exit_tag;
		}	
	}

	if (0 > ioctl(mFD, VIDIOC_STREAMON, &type)) {
		LOGD("VIDIOC_STREAMON failed\n");
		goto exit_tag;
	}	

	return mFD;
	
exit_tag:
	close(mFD);
	mFD = -1;
	return mFD;
}

void Camera::peekFrame() {

	if (mFD < 0) {
		LOGD("camera not open, open first\n");
		return;
	}

	int r;
	fd_set fds;
	struct timeval tv;

	jmethodID onNewFrameMID = \
		nestedEnv->GetMethodID(cameraClass, "onNewFrame", "()V");	

	while(mPeekRun) {
		FD_ZERO(&fds);
		FD_SET(mFD, &fds);

		tv.tv_sec = 0;
		tv.tv_usec = 500 * 1000;

		r = select(mFD + 1, &fds, NULL, NULL, &tv);
		if (-1 == r) {
			if (EINTR == errno)
				continue;
			else {
			LOGD("frame-peek is error, \
					hardware may not work normally.Reopening is reqired");
			}
		}

		if (0 == r) {
			LOGD("frame-peek is time-out, \
					hardware may not work normally.Reopening is reqired\n");
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
				LOGD("VIDIOC_DQBUF is err, \
						hardware may not work normally.Reopening is reqired\n");
			}
		}

		// get a frame now
		nestedEnv->CallVoidMethod(mJCamera, onNewFrameMID);
		
		// Retrieve
		if (0 > ioctl(mFD, VIDIOC_QBUF, &mCurBufInfo)) {
			LOGD("Retrieve buf err\n");
			goto exit_tag;
		}
	}

exit_tag:	
	mPeekRun = false;
	
	
}

void Camera::fillFrame(jobject container) {
	jobject target;
	jbyteArray targetBuffer;

	// fill field<timestamp> of container
	long long timestamp = ((long long)(mCurBufInfo.timestamp.tv_sec) * 1000 \
								+ mCurBufInfo.timestamp.tv_usec / 1000);
	nestedEnv->SetLongField(container, stampID, timestamp);

	// fill field<length> of container
	int length = mCurBufInfo.length;
	nestedEnv->SetIntField(container, sizeID, length);

	// fill field<target payload> of container
	target = nestedEnv->GetObjectField(container, targetID);
	targetBuffer = (jbyteArray)nestedEnv->CallObjectMethod(target, arrayMethodID);
	
	signed char *ptr = (signed char *)nestedEnv->GetByteArrayElements(targetBuffer, NULL);
	memcpy(ptr, mBufferArray[mCurBufInfo.index].start, length);
	nestedEnv->ReleaseByteArrayElements(targetBuffer, ptr, 0);
}

/*
 *	��֤peekFrameѭ��������֪ͨCameraUser�رգ��ر�����ͷ���Ƴ�CameraHolder
 */
void Camera::closeCamera() {
	if (mFD < 0)
		return;

	mPeekRun = false;	
	
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
	
	Camera::init(env);
	cameraToken = Camera::getCameraDevicePath(object);
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

	const char *cameraToken = Camera::getCameraDevicePath(object);
	string cameraKey(cameraToken);
	Camera *camera = CameraHolder::globalInstance()\
									.getCamera(cameraKey);
	if (camera != NULL) {
		camera->peekFrame();
		return 0;
	}
	
	return -1;
}

JNIEXPORT void JNICALL 
Java_com_hongdian_usbcamera_Camera_fillFrame
(JNIEnv *env, jobject object, jobject jContainer) {

	const char *cameraToken = Camera::getCameraDevicePath(object);
	string cameraKey(cameraToken);
	Camera *camera = CameraHolder::globalInstance()\
									.getCamera(cameraKey);
	if (camera != NULL) {
		camera->fillFrame(jContainer);
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
	const char *cameraToken = Camera::getCameraDevicePath(object);
	string cameraKey(cameraToken);
	Camera *camera = CameraHolder::globalInstance()\
									.getCamera(cameraKey);
	if (camera != NULL) {
		camera->closeCamera();
		CameraHolder::globalInstance().removeCamera(cameraKey);
		delete camera;
	}
}

