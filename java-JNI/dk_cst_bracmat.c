#include "dk_cst_bracmat.h"
#include "../src/bracmat.h"

#if !defined WIN32
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

pthread_mutex_t mutexout = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexinit = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexend = PTHREAD_MUTEX_INITIALIZER;
#endif

const char * out;

JNIEXPORT jstring JNICALL Java_dk_cst_bracmat_eval(JNIEnv * env, jobject obj, jstring expr)
    {
    int err;
    jstring ret;
    const char * str;
#if defined WIN32
    void mutexLockOut();
    void mutexUnlockOut();
    mutexLockOut();
#else
    pthread_mutex_lock( &mutexout);
#endif
    str = (*env)->GetStringUTFChars(env, expr, NULL);
    stringEval(str,&out,&err);
    ret = (*env)->NewStringUTF(env, out);
#if defined WIN32
    mutexUnlockOut();
#else
    pthread_mutex_unlock(&mutexout);
#endif
    return ret;
    }

JNIEXPORT jint JNICALL Java_dk_cst_bracmat_init(JNIEnv * env, jobject obj, jstring expr)
    {
    int err;
    jint ret;
#if defined WIN32
    void mutexLockInit();
    void mutexUnlockInit();
    mutexLockInit();
#else
    pthread_mutex_lock( &mutexinit);
#endif
    ret = startProc(0);
    if(ret == 1)
        {
        const char * str = (*env)->GetStringUTFChars(env, expr, NULL);
        stringEval((const char *)str,&out,&err); /*initialize, e.g. read program file*/
        }
#if defined WIN32
    mutexUnlockInit();
#else
    pthread_mutex_unlock(&mutexinit);
#endif
    return ret;
    }

JNIEXPORT void JNICALL Java_dk_cst_bracmat_end(JNIEnv * env, jobject obj)
    {
#if defined WIN32
    void mutexLockEnd();
    void mutexUnlockEnd();
    mutexLockEnd();
#else
    pthread_mutex_lock( &mutexend);
#endif
    endProc();
#if defined WIN32
    mutexUnlockEnd();
#else
    pthread_mutex_unlock(&mutexend);
#endif
    }


