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

#include "com_hongdian_usbcamera_Camera.h"

#define LOG_TAG "USBCameraJNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

using namespace std;

class Camera;

static const char *getCameraToken(JNIEnv *env, jobject &object) {
	const char* cameraTokenUTFChars = NULL;
	jclass jCameraClazz;
	jstring cameraToken;
	jfieldID fid;
	Camera *camera;
  
	jCameraClazz = env->FindClass("com/hongdian/usbcamera/Camera");
	fid = env->GetFieldID(jCameraClazz, "mCameraDevPath", "Ljava/lang/String;");
	cameraToken = (jstring)env->GetObjectField(object,fid);
	cameraTokenUTFChars = env->GetStringUTFChars(cameraToken, NULL);

	LOGD("cameraTokenUTFChars:%s\n", cameraTokenUTFChars);	
	return cameraTokenUTFChars;
}

class CameraHolder {
public:
	CameraHolder();
	~CameraHolder();

	void putCamera(string &key, Camera *);
    Camera *getCamera(string &key);
	
	void releaseCamera(string &key);

private:
	pthread_mutex_t mCameraLock;
    std::map<string, Camera *> mOpenedCameras;
};

class Camera {
public:
	Camera(JNIEnv *env, jobject JCamera);
	~Camera();
	
    int open();

    void peekFrame();

	void close();
	string& getCameraStringToken();

private:
	string mCameraDevPath;
	
	int mFD;
	bool mPeekRun;

	JNIEnv *mNestedEnv;
    jobject mJCamera;
};

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

void CameraHolder::releaseCamera(string &key)
{
	pthread_mutex_lock(&mCameraLock);
	mOpenedCameras.erase(key);
	pthread_mutex_unlock(&mCameraLock);
}

Camera::Camera(JNIEnv *env, jobject JCamera) {
	const char *ptr = NULL;

	ptr = getCameraToken(env, JCamera);
	mCameraDevPath = ptr;

	mNestedEnv = env;
	mJCamera = JCamera;	
	mPeekRun = false;
	mFD = -1;
}

Camera::~Camera(){
	
}

int Camera::open() {
//	__android_log_print(ANDROID_LOG_DEBUG , "Camera Open: %s", mCameraDevPath);
}

void Camera::peekFrame(){
	struct timeval val;

	jsize frameBufferSize = 100;

	jobject frame;

	jbyteArray frameByteArray = mNestedEnv->NewByteArray(frameBufferSize);
	signed char *ptr = (signed char *)mNestedEnv->GetByteArrayElements(frameByteArray, NULL);
	ptr[0] = 1;
	ptr[1] = 2;
	ptr[2] = 3;
	mNestedEnv->ReleaseByteArrayElements(frameByteArray, ptr, 0);	
	jclass bytebufferClazz = mNestedEnv->FindClass("java/nio/ByteBuffer");
	jmethodID bytebufferWrapMID = mNestedEnv->GetStaticMethodID(bytebufferClazz, "wrap", "([B)Ljava/nio/ByteBuffer;");
	frame = mNestedEnv->CallStaticObjectMethod(bytebufferClazz, bytebufferWrapMID, frameByteArray);

	jclass cameraClazz = mNestedEnv->FindClass("com/hongdian/usbcamera/Camera");
	jmethodID onNewFrameMID = mNestedEnv->GetMethodID(cameraClazz, "onNewFrame", "(Ljava/nio/ByteBuffer;J)V");	

//	__android_log_print(ANDROID_LOG_DEBUG , "Camera peekFrame: %s", mCameraDevPath);
	while (1) {
		sleep(1);

		LOGD("new frame readable!");

		gettimeofday(&val, NULL);
		mNestedEnv->CallVoidMethod(mJCamera, onNewFrameMID, frame, val.tv_sec * 1000 + val.tv_usec / 1000);		
	}
}

void Camera::close() {
//	__android_log_print(ANDROID_LOG_DEBUG , "Camera close: %s", mCameraDevPath);
}

string& Camera::getCameraStringToken() {
	return mCameraDevPath;
}

CameraHolder gCameraHolder;

static Camera* findCamera(JNIEnv *env, jobject &object){
	const char *ptr = getCameraToken(env, object);
	string cameraKey = ptr;
	return gCameraHolder.getCamera(cameraKey);
}


/*
 * Class:     com_hongdian_usbcamera_Camera
 * Method:    openCamera
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_hongdian_usbcamera_Camera_openCamera
(JNIEnv *env, jobject object) {

	Camera *camera = findCamera(env, object);
	if (NULL == camera) {
		camera = new Camera(env, object);

		gCameraHolder.putCamera(camera->getCameraStringToken(), camera);

		LOGD("now open camera!\n");
		return camera->open();
	} 
	
	return -1;
}

/*
 * Class:     com_hongdian_usbcamera_Camera
 * Method:    peekFrame
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_hongdian_usbcamera_Camera_peekFrame
(JNIEnv *env, jobject object) {

	Camera *camera = findCamera(env, object);

	if (camera != NULL) {
		camera->peekFrame();
		return 0;
	}
	
	return -1;
}

/*
 * Class:     com_hongdian_usbcamera_Camera
 * Method:    closeCamera
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_hongdian_usbcamera_Camera_closeCamera
(JNIEnv *env, jobject object) {
	Camera *camera;
	const char *ptr = getCameraToken(env, object);
	string cameraKey = ptr;
	camera = gCameraHolder.getCamera(cameraKey);
	if (camera != NULL) {
		camera->close();
		gCameraHolder.releaseCamera(cameraKey);
		delete camera;
	}
}

