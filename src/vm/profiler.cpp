#include "slate.hpp"

void profiler_start(struct object_heap* oh) {
  oh->currentlyProfiling = 1;
  oh->profilerTimeStart = getTickCount();
  oh->profilerTime = oh->profilerTimeStart;
  oh->profilerLastTime = oh->profilerTimeStart;
  oh->profiledMethods.clear();
  oh->profilerCallCounts.clear();
  oh->profilerSelfTime.clear();

  oh->profilerChildCallCount.clear();
  oh->profilerChildCallTime.clear();

  oh->profilerParentChildCalls.clear();
  oh->profilerParentChildTimes.clear();
  oh->profilerParentChildCount.clear();

}


void profiler_stop(struct object_heap* oh) {
  oh->currentlyProfiling = 0;
}

void profilerIncrement(std::map<struct Object*,word_t>& dict, struct Object* entry, word_t count) {
  dict[entry] += count;
}

/*push is true if we're calling a method and false if we're returing to something higher on the stack*/
void profiler_enter_method(struct object_heap* oh, struct Object* fromMethod, struct Object* toMethod, bool_t push) {
  if (!oh->currentlyProfiling) return;


  //printf("%p -> %p (%d) (%d)\n", fromMethod, toMethod, (int)push, (int) oh->profilerCallStack.size());

  oh->profilerTime = getTickCount();
  word_t timeDiff = oh->profilerTime - oh->profilerLastTime;

  oh->profiledMethods.insert(toMethod);

  /* increment the time for the last method that just finished*/
  profilerIncrement(oh->profilerSelfTime, fromMethod, timeDiff);

  if (push) {
    profilerIncrement(oh->profilerCallCounts, toMethod, 1);

    /* increment child call count if we have a parent */
    struct Object* parent = fromMethod;
    struct Object* child = toMethod;

    if (oh->profilerChildCallCount.find(parent) == oh->profilerChildCallCount.end()) {
      std::map<struct Object*,word_t> entries;
      entries[child] = 1;
      oh->profilerChildCallCount[parent] = entries;
    } else {
      profilerIncrement(oh->profilerChildCallCount[parent], child, 1);
    }

    profilerIncrement(oh->profilerParentChildCount, child, 1);
    if (oh->profilerParentChildCount[child] == 1) {
      oh->profilerParentChildCalls[child] = parent;
      oh->profilerParentChildTimes[child] = oh->profilerTime;
    }
  } else {
    /* returned from fromMethod to toMethod */
    struct Object* child = fromMethod;

    // if there aren't any more instances of this child on the stack...
    if (oh->profilerParentChildCount[child] == 1) {
      struct Object* parent = oh->profilerParentChildCalls[child];
      word_t childStartTime = oh->profilerParentChildTimes[child];

      if (oh->profilerChildCallTime.find(parent) == oh->profilerChildCallTime.end()) {
        std::map<struct Object*,word_t> entries;
        entries[child] = oh->profilerTime - childStartTime;
        oh->profilerChildCallTime[parent] = entries;
      } else {
        profilerIncrement(oh->profilerChildCallTime[parent], child, oh->profilerTime - childStartTime);
      }

    }

    if (oh->profilerParentChildCount[child] > 0) {
      profilerIncrement(oh->profilerParentChildCount, child, -1);
    }

  }

  oh->profilerLastTime = oh->profilerTime;
}

void profiler_notice_forwarded_object(struct object_heap* oh, struct Object* from, struct Object* to) {
  if (!oh->currentlyProfiling) return;
  //printf("pf %p -> %p\n", from, to);
  if (oh->profiledMethods.find(from) == oh->profiledMethods.end()) return;
  oh->profiledMethods.erase(from); oh->profiledMethods.insert(to);

  oh->profilerSelfTime[to] = oh->profilerSelfTime[from];
  oh->profilerSelfTime.erase(from);

  oh->profilerCallCounts[to] = oh->profilerCallCounts[from];
  oh->profilerCallCounts.erase(from);

 
  for (std::map<struct Object*, std::map<struct Object*,word_t> >::iterator i = oh->profilerChildCallCount.begin();
       i != oh->profilerChildCallCount.end(); i++) {
    std::map<struct Object*,word_t>& childSet = (*i).second;
    childSet[to] = childSet[from];
    childSet.erase(from);
  }
  for (std::map<struct Object*, std::map<struct Object*,word_t> >::iterator i = oh->profilerChildCallTime.begin();
       i != oh->profilerChildCallTime.end(); i++) {
    std::map<struct Object*,word_t>& childSet = (*i).second;
    childSet[to] = childSet[from];
    childSet.erase(from);
  }

}

/*this will be called when the GC deletes the object*/
void profiler_delete_method(struct object_heap* oh, struct Object* method) {
  if (!oh->currentlyProfiling) return;
  //printf("pf del %p\n", method);
  oh->profiledMethods.erase(method);
  oh->profilerSelfTime.erase(method);
  oh->profilerCallCounts.erase(method);

  for (std::map<struct Object*, std::map<struct Object*,word_t> >::iterator i = oh->profilerChildCallCount.begin();
       i != oh->profilerChildCallCount.end(); i++) {
    (*i).second.erase(method);
  }
  for (std::map<struct Object*, std::map<struct Object*,word_t> >::iterator i = oh->profilerChildCallTime.begin();
       i != oh->profilerChildCallTime.end(); i++) {
    (*i).second.erase(method);
  }

}


