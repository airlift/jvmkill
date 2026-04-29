/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <jvmti.h>

/*
 * True if -XX:+ExitOnOutOfMemoryError or -XX:+CrashOnOutOfMemoryError was
 * set at VM init.
 *
 * Probed once at VM init via com.sun.management.HotSpotDiagnosticMXBean#
 * getVMOption(String). Both flags can be flipped at runtime via jcmd VM.set_flag -
 * we ignore that and trust the value at init time.
 */
static volatile int g_jvmWillExitOnOOM = 0;

static int
readBoolFlag(JNIEnv *env, jobject mxbean,
             jmethodID getVMOption, jmethodID getValue,
             const char *flagName)
{
   jstring nameStr = (*env)->NewStringUTF(env, flagName);
   if (nameStr == NULL) { (*env)->ExceptionClear(env); return 0; }

   jobject vmOption = (*env)->CallObjectMethod(env, mxbean, getVMOption, nameStr);

   (*env)->DeleteLocalRef(env, nameStr);
   if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionClear(env); return 0;
   }
   if (vmOption == NULL) return 0;

   jstring valStr = (jstring) (*env)->CallObjectMethod(env, vmOption, getValue);
   (*env)->DeleteLocalRef(env, vmOption);
   if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionClear(env); return 0;
   }
   if (valStr == NULL) return 0;

   const char *cstr = (*env)->GetStringUTFChars(env, valStr, NULL);
   int isTrue = (cstr != NULL && strcmp(cstr, "true") == 0);
   if (cstr != NULL) (*env)->ReleaseStringUTFChars(env, valStr, cstr);
   (*env)->DeleteLocalRef(env, valStr);
   return isTrue;
}

static void JNICALL
vmInit(jvmtiEnv *jvmti_env, JNIEnv *env, jthread thread)
{
   /*
    * Walk:
    *   ManagementFactory.getPlatformMXBean(HotSpotDiagnosticMXBean.class)
    *     .getVMOption("ExitOnOutOfMemoryError").getValue()
    *     .equals("true")
    *
    * Any failure (class not found, exception thrown, null result) leaves
    * g_jdkWillExitOnOOM == 0, which is the safe default: the agent will
    * SIGKILL on every OOM, matching the pre-introspection behavior.
    */
   jclass diagCls = (*env)->FindClass(env,
      "com/sun/management/HotSpotDiagnosticMXBean");
   if (diagCls == NULL) {
      (*env)->ExceptionClear(env);
      fprintf(stderr,
         "jvmkill: HotSpotDiagnosticMXBean not found "
         "(jdk.management module missing?) -- "
         "falling back to kill-on-every-OOM\n");
      return;
   }

   jclass mfCls = (*env)->FindClass(env,
      "java/lang/management/ManagementFactory");
   if (mfCls == NULL) {
      (*env)->ExceptionClear(env);
      fprintf(stderr,
         "jvmkill: ManagementFactory not found -- "
         "falling back to kill-on-every-OOM\n");
      return;
   }

   jmethodID getPlatformMXBean = (*env)->GetStaticMethodID(env, mfCls,
      "getPlatformMXBean",
      "(Ljava/lang/Class;)Ljava/lang/management/PlatformManagedObject;");
   if (getPlatformMXBean == NULL) {
      (*env)->ExceptionClear(env); return;
   }

   jobject mxbean = (*env)->CallStaticObjectMethod(env, mfCls,
      getPlatformMXBean, diagCls);
   if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionClear(env); return;
   }
   if (mxbean == NULL) {
      fprintf(stderr,
         "jvmkill: getPlatformMXBean(HotSpotDiagnosticMXBean) returned null "
         "-- falling back to kill-on-every-OOM\n");
      return;
   }

   jmethodID getVMOption = (*env)->GetMethodID(env, diagCls,
      "getVMOption",
      "(Ljava/lang/String;)Lcom/sun/management/VMOption;");
   if (getVMOption == NULL) {
      (*env)->ExceptionClear(env); return;
   }

   jclass vmOptCls = (*env)->FindClass(env, "com/sun/management/VMOption");
   if (vmOptCls == NULL) {
      (*env)->ExceptionClear(env); return;
   }
   jmethodID getValue = (*env)->GetMethodID(env, vmOptCls,
      "getValue", "()Ljava/lang/String;");
   if (getValue == NULL) {
      (*env)->ExceptionClear(env); return;
   }

   int exitSet  = readBoolFlag(env, mxbean, getVMOption, getValue,
                               "ExitOnOutOfMemoryError");
   int crashSet = readBoolFlag(env, mxbean, getVMOption, getValue,
                               "CrashOnOutOfMemoryError");

   g_jvmWillExitOnOOM = (exitSet || crashSet);

   fprintf(stderr,
      "jvmkill: ExitOnOutOfMemoryError=%s CrashOnOutOfMemoryError=%s -- "
      "mode=%s\n",
      exitSet  ? "true" : "false",
      crashSet ? "true" : "false",
      g_jvmWillExitOnOOM ? "JVM handles OOM"
                         : "kill-on-every-OOM");
}

static void JNICALL
resourceExhausted(
      jvmtiEnv *jvmti_env,
      JNIEnv *jni_env,
      jint flags,
      const void *reserved,
      const char *description)
{
   if (g_jvmWillExitOnOOM
       && flags == (JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR
                  | JVMTI_RESOURCE_EXHAUSTED_JAVA_HEAP)) {
      fprintf(
         stderr,
         "jvmkill: JVMTI ResourceExhausted OOM_ERROR|JAVA_HEAP "
         "\"%s\" [flags=0x%x]\n",
         description,
         flags);
   }
   else if (g_jvmWillExitOnOOM
            && flags == JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR) {
      fprintf(
         stderr,
         "jvmkill: JVMTI ResourceExhausted OOM_ERROR "
         "\"%s\" [flags=0x%x]\n",
         description,
         flags);
   }
   else {
      fprintf(
         stderr,
         "jvmkill: JVMTI ResourceExhausted  OOM_ERROR | THREADS"
         "\"%s\" [flags=0x%x] -- killing process (SIGKILL)\n",
         description,
         flags);
      kill(getpid(), SIGKILL);
      _exit(1);
   }
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
   callbacks.VMInit            = &vmInit;

   err = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
   if (err != JVMTI_ERROR_NONE) {
      fprintf(stderr, "ERROR: SetEventCallbacks failed: %d\n", err);
      return JNI_ERR;
   }

   err = (*jvmti)->SetEventNotificationMode(
         jvmti, JVMTI_ENABLE, JVMTI_EVENT_RESOURCE_EXHAUSTED, NULL);
   if (err != JVMTI_ERROR_NONE) {
      fprintf(stderr, "ERROR: SetEventNotificationMode(ResourceExhausted) failed: %d\n", err);
      return JNI_ERR;
   }

   err = (*jvmti)->SetEventNotificationMode(
         jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
   if (err != JVMTI_ERROR_NONE) {
      fprintf(stderr, "ERROR: SetEventNotificationMode(VMInit) failed: %d\n", err);
      return JNI_ERR;
   }

   return JNI_OK;
}
