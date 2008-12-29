#include <sys/utsname.h>

int uname(struct utsname *name);

#ifdef __BEOS__
#define PLATFORM	"BeOS"
#elif defined(WINDOWS)
#define PLATFORM	"Windows"
#endif

char* slate_GetSystemName();
char* slate_GetSystemRelease();
char* slate_GetSystemVersion();
char* slate_GetPlatform();
char* slate_GetMachine();
char* slate_EnvironmentAt(const char* name);
int slate_EnvironmentAtPut(const char* name, const char* value);
int slate_EnvironmentRemoveKey(const char* name);
int slate_SystemProcessCommand(const char* command);
int slate_GetEndianness();
