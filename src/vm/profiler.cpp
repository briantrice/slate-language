#include "slate.hpp"

void profiler_start(struct object_heap* oh) {
  oh->currentlyProfiling = 1;
  oh->profilerTimeStart = getTickCount();
  oh->profilerTime = oh->profilerTimeStart;
  oh->profilerLastTime = oh->profilerTimeStart;
  oh->profiledMethods.clear();
  oh->profilerCallCounts.clear();
  oh->profilerSelfTime.clear();
  oh->profilerCumTime.clear();
  oh->profilerCallStack.clear();
  oh->profilerTimeStack.clear();

  oh->profilerChildCallCount.clear();
  oh->profilerChildCallTime.clear();

  oh->profilerLastMethod = 0;
}


void profiler_stop(struct object_heap* oh) {
  oh->currentlyProfiling = 0;
}

void profilerIncrement(std::map<struct Object*,word_t>& dict, struct Object* entry, word_t count) {
  dict[entry] += count;
}

void profiler_enter_method(struct object_heap* oh, struct Object* method, bool_t enter /*0 if leaving*/) {
  if (!oh->currentlyProfiling) return;
  if (!object_is_old(oh, method)) return; /*young objects move in memory*/

  oh->profilerTime = getTickCount();
  word_t timeDiff = oh->profilerTime - oh->profilerLastTime;

  oh->profiledMethods.insert(method);

  /* increment the call count for this method and the time for the last method that just finished*/
  profilerIncrement(oh->profilerCallCounts, method, 1);
  if (oh->profilerLastMethod) {
    profilerIncrement(oh->profilerSelfTime, oh->profilerLastMethod, timeDiff);
  }

  if (enter) {
    /* increment child call count if we have a parent */
    if (oh->profilerCallStack.size() >= 1) {
      struct Object* parent = oh->profilerCallStack[oh->profilerCallStack.size()-1];
      if (oh->profilerChildCallCount.find(parent) == oh->profilerChildCallCount.end()) {
        std::map<struct Object*,word_t> entries;
        entries[method] = 1;
        oh->profilerChildCallCount[parent] = entries;
      } else {
        profilerIncrement(oh->profilerChildCallCount[parent], method, 1);
      }
    }

    oh->profilerCallStack.push_back(method);
    oh->profilerTimeStack.push_back(oh->profilerTime);
  } else {
    if (oh->profilerCallStack.size() > 1) {
      struct Object* child = oh->profilerCallStack.back();
      word_t childStartTime = oh->profilerTimeStack.back();
      oh->profilerCallStack.pop_back();
      oh->profilerTimeStack.pop_back();

      /*don't double count it*/
      if (std::find(oh->profilerCallStack.begin(), oh->profilerCallStack.end(), child) == oh->profilerCallStack.end()) {
        profilerIncrement(oh->profilerCumTime, child, oh->profilerTime - childStartTime);
      }
      /*increment child time if there is a parent */
      if (oh->profilerCallStack.size() > 1) {
        struct Object* parent = oh->profilerCallStack[oh->profilerCallStack.size()-1];
        if (oh->profilerChildCallTime.find(parent) == oh->profilerChildCallTime.end()) {
          std::map<struct Object*,word_t> entries;
          entries[child] = oh->profilerTime - childStartTime;
          oh->profilerChildCallTime[parent] = entries;
        } else {
          profilerIncrement(oh->profilerChildCallTime[parent], child, oh->profilerTime - childStartTime);
        }
        
      }
    }
  }


  oh->profilerLastTime = oh->profilerTime;
  oh->profilerLastMethod = method;
  
}

/*this will be called when the GC deletes or forwards the object*/
void profiler_delete_method(struct object_heap* oh, struct Object* method) {
  if (!oh->currentlyProfiling) return;
  oh->profiledMethods.erase(method);

}


