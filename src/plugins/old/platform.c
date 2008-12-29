#include <stdlib.h>
#include "platform.h"

struct utsname info;

int slate_RefreshSystemInfo() {
  // Run uname() to initialize the info struct if needed.
  // Answer whether uname() worked; should mean the data is there and valid.
  return !(&info && uname(&info));
}

char* slate_GetSystemName() {
  return (slate_RefreshSystemInfo() ? info.nodename : NULL);
}

char* slate_GetSystemRelease() {
  return (slate_RefreshSystemInfo() ? info.release : NULL);
}

char* slate_GetSystemVersion() {
  return (slate_RefreshSystemInfo() ? info.version : NULL);
}

char* slate_GetPlatform() {
#ifdef PLATFORM
  return PLATFORM;
#else
  return (slate_RefreshSystemInfo() ? info.sysname : NULL);
#endif
}

char* slate_GetMachine() {
  return (slate_RefreshSystemInfo() ? info.machine : NULL);
}

char* slate_EnvironmentAt(const char* name) {
  return getenv(name);
}

int slate_EnvironmentAtPut(const char* name, const char* value) {
  return !setenv(name, value, 1);
}

int slate_EnvironmentRemoveKey(const char* name) {
  unsetenv(name); return 1;
}

int slate_SystemProcessCommand(const char* command) {
  return !system(command);
}

int slate_GetEndianness () {
  int x = 1;
  char little_endian = *(char*)&x;
  return (int) little_endian;
}
