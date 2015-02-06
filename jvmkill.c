#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <jvmti.h>

static void JNICALL
resourceExhausted(
      jvmtiEnv *jvmti_env,
      JNIEnv *jni_env,
      jint flags,
      const void *reserved,
      const char *description)
{
   fprintf(stderr, "ResourceExhausted: killing current process!\n");
   kill(getpid(), SIGKILL);
}

JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved)
{
   jvmtiEnv *jvmti;
   jvmtiError err;

   jint rc = (*vm)->GetEnv(vm, (void **) &jvmti, JVMTI_VERSION);
   if (rc != JNI_OK) {
      fprintf(stderr, "ERROR: GetEnv failed: %d\n", rc);
      return JNI_ERR;
   }

   jvmtiEventCallbacks callbacks;
   memset(&callbacks, 0, sizeof(callbacks));

   callbacks.ResourceExhausted = &resourceExhausted;

   err = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
   if (err != JVMTI_ERROR_NONE) {
      fprintf(stderr, "ERROR: SetEventCallbacks failed: %d\n", err);
      return JNI_ERR;
   }

   err = (*jvmti)->SetEventNotificationMode(
         jvmti, JVMTI_ENABLE, JVMTI_EVENT_RESOURCE_EXHAUSTED, NULL);
   if (err != JVMTI_ERROR_NONE) {
      fprintf(stderr, "ERROR: SetEventNotificationMode failed: %d\n", err);
      return JNI_ERR;
   }

   return JNI_OK;
}
