#include "slate.h"

void profiler_start(struct object_heap* oh) {
  word_t i;

  for (i = 0; i < PROFILER_ENTRY_COUNT; i++) {
    oh->profiler_entries[i].method = NULL;
  }

  oh->currentlyProfiling = 1;

}


void profiler_stop(struct object_heap* oh) {
  oh->currentlyProfiling = 0;
}


void profiler_enter_method(struct object_heap* oh, struct Object* method) {
  word_t i;
  if (!oh->currentlyProfiling) return;
  if (!object_is_old(oh, method)) return; /*young objects move in memory*/

  for (i = 0; i < PROFILER_ENTRY_COUNT; i++) {
    if (oh->profiler_entries[i].method == method) {
      oh->profiler_entries[i].callCount++;
      oh->profilerTimeStart = getTickCount();
      return;
    }
  }

  /*not found*/
  for (i = 0; i < PROFILER_ENTRY_COUNT; i++) {
    if (oh->profiler_entries[i].method != NULL) continue;
    /*use this entry*/
    oh->profiler_entries[i].method = method;
    oh->profiler_entries[i].callCount = 1;
    oh->profiler_entries[i].callTime = 0;
    oh->profilerTimeStart = getTickCount();
    return;
  }
  /*no free entries if we reach this*/

}

void profiler_leave_method(struct object_heap* oh, struct Object* method) {
  word_t i;
  if (!oh->currentlyProfiling) return;
  if (!object_is_old(oh, method)) return; /*young objects move in memory*/
  for (i = 0; i < PROFILER_ENTRY_COUNT; i++) {
    if (oh->profiler_entries[i].method == method) {
      oh->profilerTimeEnd = getTickCount();
      oh->profiler_entries[i].callTime += oh->profilerTimeEnd - oh->profilerTimeStart;
      return;
    }
  }

  /*not found*/
}

/*this will be called when the GC deletes or forwards the object*/
void profiler_delete_method(struct object_heap* oh, struct Object* method) {
  word_t i;
  if (!oh->currentlyProfiling) return;
  for (i = 0; i < PROFILER_ENTRY_COUNT; i++) {
    if (oh->profiler_entries[i].method == method) oh->profiler_entries[i].method = NULL;
  }

}


