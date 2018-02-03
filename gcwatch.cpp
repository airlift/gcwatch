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

#include <atomic>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <jvmti.h>

static int port;

static std::atomic_bool inGc;
static std::atomic_long start;

static long getMillis()
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (ts.tv_sec * 1000) + (ts.tv_nsec / (1000 * 1000));
}

static void JNICALL gcStart(jvmtiEnv *jvmti)
{
   start = getMillis();
   inGc = true;
}

static void JNICALL gcFinish(jvmtiEnv *jvmti)
{
   inGc = false;
}

static int serverSocket()
{
   int fd = socket(AF_INET6, SOCK_STREAM, 0);
   if (fd == -1) {
      perror("ERROR: failed to create GcWatch socket");
      exit(1);
   }

   int reuse = true;
   if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
      perror("ERROR: failed to set GcWatch socket option");
      exit(1);
   }

   struct sockaddr_in6 addr = {};
   addr.sin6_family = AF_INET6;
   addr.sin6_port = htons(port);
   addr.sin6_addr = in6addr_any;

   if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
      perror("ERROR: failed to bind GcWatch socket");
      exit(1);
   }

   if (listen(fd, SOMAXCONN) == -1) {
      perror("ERROR: failed to listen on GcWatch socket");
      exit(1);
   }

   return fd;
}

static void JNICALL worker(jvmtiEnv *jvmti, JNIEnv *jni, void *arg)
{
   int server = serverSocket();

   fprintf(stderr, "GcWatch listener started on port %d\n", port);

   while (true) {
      int client = accept(server, NULL, 0);
      if (client == -1) {
         continue;
      }

      char output[128];
      if (inGc) {
         sprintf(output, "GC %ld\n", getMillis() - start);
      }
      else {
         strcpy(output, "OK\n");
      }

      write(client, output, strlen(output));
      close(client);
   }
}

static jthread createThread(JNIEnv *jni, const char *threadName)
{
   jclass clazz = jni->FindClass("java/lang/Thread");
   if (clazz == NULL) {
      fprintf(stderr, "ERROR: GcWatch: failed to find Thread class\n");
      exit(1);
   }

   jmethodID init = jni->GetMethodID(clazz, "<init>", "(Ljava/lang/String;)V");
   if (init == NULL) {
      fprintf(stderr, "ERROR: GcWatch: failed to find Thread constructor\n");
      exit(1);
   }

   jstring name = jni->NewStringUTF(threadName);
   if (name == NULL) {
      fprintf(stderr, "ERROR: GcWatch: failed to create String object\n");
      exit(1);
   }

   jthread object = jni->NewObject(clazz, init, name);
   if (object == NULL) {
      fprintf(stderr, "ERROR: GcWatch: failed to create Thread object\n");
      exit(1);
   }

   return object;
}

static void JNICALL vmInit(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread)
{
   jthread agent = createThread(jni, "GCWatch Listener");
   jvmtiError err = jvmti->RunAgentThread(agent, &worker, NULL, JVMTI_THREAD_MAX_PRIORITY);
   if (err != JVMTI_ERROR_NONE) {
      fprintf(stderr, "ERROR: RunAgentThread failed: %d\n", err);
      exit(1);
   }
}

JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved)
{
   jvmtiEnv *jvmti;
   jvmtiError err;

   // parse options
   if ((options == NULL) || (sscanf(options, "port=%d", &port) != 1)) {
      fprintf(stderr, "ERROR: failed to parse port option\n");
      return JNI_ERR;
   }

   // get environment
   jint rc = vm->GetEnv((void **) &jvmti, JVMTI_VERSION);
   if (rc != JNI_OK) {
      fprintf(stderr, "ERROR: GetEnv failed: %d\n", rc);
      return JNI_ERR;
   }

   // add event capability
   jvmtiCapabilities capabilities = {};
   capabilities.can_generate_garbage_collection_events = true;

   err = jvmti->AddCapabilities(&capabilities);
   if (err != JVMTI_ERROR_NONE) {
      fprintf(stderr, "ERROR: AddCapabilities for GC events failed: %d\n", err);
      return JNI_ERR;
   }

   // register event callbacks
   jvmtiEventCallbacks callbacks = {};
   callbacks.VMInit = &vmInit;
   callbacks.GarbageCollectionStart = &gcStart;
   callbacks.GarbageCollectionFinish = &gcFinish;

   err = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
   if (err != JVMTI_ERROR_NONE) {
      fprintf(stderr, "ERROR: SetEventCallbacks failed: %d\n", err);
      return JNI_ERR;
   }

   // request notifications
   jvmtiEvent events[] = {
      JVMTI_EVENT_VM_INIT,
      JVMTI_EVENT_GARBAGE_COLLECTION_START,
      JVMTI_EVENT_GARBAGE_COLLECTION_FINISH,
   };

   for (auto event : events) {
      err = jvmti->SetEventNotificationMode(JVMTI_ENABLE, event, NULL);
      if (err != JVMTI_ERROR_NONE) {
         fprintf(stderr, "ERROR: SetEventNotificationMode failed: %d\n", err);
         return JNI_ERR;
      }
   }

   return JNI_OK;
}
