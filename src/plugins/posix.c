#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int slate_CreateDirectory(const char* dirname) {
  //TODO: permissions.
  return !mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

int slate_DeleteDirectory(const char* dirname) {
  return !rmdir(dirname);
}

int slate_LinkFile(const char* pathExisting, const char* pathNew) {
  return !link(pathExisting, pathNew);
}

int slate_UnlinkFile(const char* path) {
  return !unlink(path);
}

int slate_SymbolicLinkFile(const char* pathExisting, const char* pathNew) {
  return !symlink(pathExisting, pathNew);
}

int slate_ReadSymbolicLinkInto(const char* path, char* buffer, int bufferSize) {
  return readlink(path, buffer, bufferSize);
}

int slate_RenameFile(const char* pathExisting, const char* pathNew) {
  return !rename(pathExisting, pathNew);
}

int slate_RemoveFile(const char* path) {
  return !remove(path);
}

int slate_CreateFIFO(const char* path) {
  return !mkfifo(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

int slate_CreateFSNode(const char* path, mode_t mode, dev_t dev) {
  //TODO: Make slate aware of dev_t; maybe remove other mk* API's?
  return !mknod(path, mode, dev);
}

int slate_ChangeFileModes(const char* path, mode_t mode) {
  return !chmod(path, mode);
}

int state_GetFileInfo(const char* path) {
  struct stat buffer;
  int status = stat(path, &buffer);
  //TODO: Decide how to pass the stat struct info. Use a ByteArray arg?
  return !status;
}
