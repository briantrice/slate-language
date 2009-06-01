/* necessary for POSIX conform readdir_r API */
#if defined(__sun)
#define _POSIX_PTHREAD_SEMANTICS
#endif

#include "slate.h"

void dir_module_init(struct object_heap* oh) {
  oh->dir_index_size = SLATE_DIRECTORIES_MAXIMUM;
  oh->dir_index = calloc(oh->dir_index_size, sizeof(DIR*));
}

bool_t dir_handle_isvalid(struct object_heap* oh, word_t dir) {
  return (0 <= dir && dir < oh->dir_index_size && oh->dir_index[dir] != NULL);
}

static int dir_allocate(struct object_heap* oh) {
  int i;
  for (i = 0; i < SLATE_DIRECTORIES_MAXIMUM; i++) {
    if (oh->dir_index[i] == NULL)
      return i;
  }
  return -EMFILE;
}

int dir_open(struct object_heap* oh, struct ByteArray *dirName) {
  int dirHandle = dir_allocate(oh);
  if (dirHandle < 0) {
    return dirHandle;
  } else {
    size_t nameLen = payload_size((struct Object *) dirName);
    char *name = malloc(nameLen + 1);
    if (name == NULL) {
      return -errno;
    } else {
      memcpy(name, dirName->elements, nameLen);
      name[nameLen] = '\0';
      oh->dir_index[dirHandle] = opendir(name);
      if (oh->dir_index[dirHandle] == NULL) {
        int savedErrno = errno;
        free(name);
        return -savedErrno;
      } else {
        free(name);
        return dirHandle;
      }
    }
  }
}

int dir_close(struct object_heap* oh, int dirHandle) {
  if (oh->dir_index[dirHandle] != NULL) {
    closedir(oh->dir_index[dirHandle]);
    oh->dir_index[dirHandle] = NULL;
    return 0;
  } else {
    return -EINVAL;
  }
}

int dir_read(struct object_heap* oh, int dirHandle, struct ByteArray *entNameBuffer) {
  struct dirent ent;
  struct dirent *result = &ent;
  int resultCode, entLen;

  if (dirHandle < 0)
    return -EINVAL;

#if defined(__CYGWIN__)
  result = readdir(oh->dir_index[dirHandle]);
  resultCode = errno;
#else
  resultCode = readdir_r(oh->dir_index[dirHandle], result, &result);
#endif
  if (resultCode != 0) {
    /* An error situation. */
    assert(resultCode >= 0);	/* TODO: read SUSv2. The Darwin manpage is unclear about this */
    return -resultCode;
  }

  if (result == NULL) {
    /* End of the directory. */
    return 0;
  }

  entLen = strlen(result->d_name);
  if (entLen == 0) {
    /* Bizarre! Make up a reasonable response. */
    return -ENOENT;
  }

  assert(payload_size((struct Object *) entNameBuffer) >= entLen);
  memcpy(entNameBuffer->elements, result->d_name, entLen);
  return entLen;
}

int dir_getcwd(struct ByteArray *pathBuffer) {
#ifdef WIN32
  return GetCurrentDirectory((DWORD)payload_size((struct Object *) pathBuffer), pathBuffer->elements);
#else
  return ((getcwd((char *)pathBuffer->elements, byte_array_size(pathBuffer)) == NULL)
	  ? -errno : strlen((const char *)pathBuffer->elements));
#endif
}

int dir_setcwd(struct ByteArray *newpath) {
  word_t pathLen = byte_array_size(newpath);
  char *path = malloc(pathLen + 1);
#ifdef WIN32
  if (path == NULL)
	return GetLastError();
  pathLen = extractCString(newpath, path, sizeof(path));

  if (pathLen < 0)
    return SLATE_ERROR_RETURN;

  return SetCurrentDirectory (path);
#else
  if (path == NULL)
    return -errno;
  memcpy(path, newpath->elements, pathLen);
  path[pathLen] = '\0';
  if (chdir(path) == -1) {
    int savedErrno = errno;
    free(path);
    return -savedErrno;
  } else {
    free(path);
    return 0;
  }
#endif
}
