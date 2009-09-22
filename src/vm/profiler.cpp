#include "slate.hpp"

void profiler_start(struct object_heap* oh) {
  oh->currentlyProfiling = 1;
  oh->profilerTimeStart = getTickCount();
  oh->methodCallDepth = calculateMethodCallDepth(oh);

}


void profiler_stop(struct object_heap* oh) {
  oh->currentlyProfiling = 0;
}


void profiler_enter_method(struct object_heap* oh, struct Object* method) {
  int64_t time;
  if (!oh->currentlyProfiling) return;
  if (!object_is_old(oh, method)) return; /*young objects move in memory*/
  oh->profiledMethods.insert(method);

  if (oh->profilerFile != NULL) {
    time = getTickCount() - oh->profilerTimeStart;
    word_t method = (word_t)method;
    fwrite(&time, sizeof(time), 1, oh->profilerFile);
    fwrite(&oh->methodCallDepth, sizeof(word_t), 1, oh->profilerFile);
    fwrite(&method, sizeof(word_t), 1, oh->profilerFile);
  }
  
}

/*this will be called when the GC deletes or forwards the object*/
void profiler_delete_method(struct object_heap* oh, struct Object* method) {
  if (!oh->currentlyProfiling) return;
  oh->profiledMethods.erase(method);

}


