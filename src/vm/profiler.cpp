#include "slate.hpp"

void profiler_start(struct object_heap* oh) {
  oh->currentlyProfiling = 1;
  oh->profilerTimeStart = getRealTimeClock();
  oh->profilerTime = oh->profilerTimeStart;
  oh->profilerLastTime = oh->profilerTimeStart;
  oh->profiledMethods.clear();
  oh->profilerCallCounts.clear();
  oh->profilerSelfTime.clear();

  oh->profilerChildCallCount.clear();
  oh->profilerChildCallTime.clear();

  oh->profilerCallStack.clear();
  oh->profilerCallStackTimes.clear();


}


void profiler_stop(struct object_heap* oh) {
  oh->currentlyProfiling = 0;
}


/*push is true if we're calling a method and false if we're returing to something higher on the stack*/
void profiler_enter_method(struct object_heap* oh, struct Object* fromMethod, struct Object* toMethod, bool_t push) {
  if (!oh->currentlyProfiling) return;


  //printf("%p -> %p (%d) (%d)\n", fromMethod, toMethod, (int)push, (int) oh->profilerCallStack.size());

  oh->profilerTime = getRealTimeClock();
  word_t timeDiff = oh->profilerTime - oh->profilerLastTime;

  oh->profiledMethods.insert(toMethod);

  /* increment the time for the last method that just finished*/
  oh->profilerSelfTime[fromMethod] += timeDiff;

  if (push) {
    oh->profilerCallCounts[toMethod] += 1;

    /* increment child call count if we have a parent */
    struct Object* parent = fromMethod;
    struct Object* child = toMethod;

    if (oh->profilerChildCallCount.find(parent) == oh->profilerChildCallCount.end()) {
      oh->profilerChildCallCount[parent][child] = 1;
    } else {
      oh->profilerChildCallCount[parent][child] += 1;
    }
    
    oh->profilerCallStack.push_back(child);
    oh->profilerCallStackTimes.push_back(oh->profilerTime);

  } else {
    /* returned from fromMethod to toMethod */
    struct Object *child = fromMethod, *parent = toMethod;
    word_t callStartTime; 
    struct Object* callMethod;
    do {
      callStartTime = oh->profilerCallStackTimes.back();
      callMethod = oh->profilerCallStack.back();
      oh->profilerCallStackTimes.pop_back();
      oh->profilerCallStack.pop_back();
    } while(callMethod != fromMethod && !oh->profilerCallStack.empty());

    assert(callMethod == fromMethod || !oh->profilerCallStack.empty());

    // tell the parent that this child was called for X time
    word_t callTimeDiff = oh->profilerTime - callStartTime;
    if (oh->profilerChildCallTime.find(parent) == oh->profilerChildCallTime.end()) {
      oh->profilerChildCallTime[parent][child] = callTimeDiff;
    } else {
      oh->profilerChildCallTime[parent][child] += callTimeDiff;
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
    if (childSet.find(from) != childSet.end()) {
      childSet[to] = childSet[from];
      childSet.erase(from);
    }
  }
  for (std::map<struct Object*, std::map<struct Object*,word_t> >::iterator i = oh->profilerChildCallTime.begin();
       i != oh->profilerChildCallTime.end(); i++) {
    std::map<struct Object*,word_t>& childSet = (*i).second;
    if (childSet.find(from) != childSet.end()) {
      childSet[to] = childSet[from];
      childSet.erase(from);
    }
  }

  for (size_t i = 0; i < oh->profilerCallStack.size(); i++) {
    if (oh->profilerCallStack[i] == from) oh->profilerCallStack[i] = to;
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

  for (size_t i = 0; i < oh->profilerCallStack.size(); i++) {
    if (oh->profilerCallStack[i] == method) oh->profilerCallStack[i] = NULL;
  }

}


