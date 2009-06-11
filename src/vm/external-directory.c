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

#if defined(WIN32) || defined(__CYGWIN__)
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


bool_t dir_make(struct object_heap* oh, char* dir) {
#ifdef WIN32
  return FALSE; /*fixme*/
#else
  return (mkdir(dir, 0777) == 0 ? TRUE : FALSE);
#endif
}

bool_t dir_delete(struct object_heap* oh, char* dir) {
#ifdef WIN32
  return FALSE; /*fixme*/
#else
  return (rmdir(dir) == 0 ? TRUE : FALSE);
#endif
}

word_t slate_direntry_type(unsigned char d_type) {
    /*develop a convention that works with other platforms*/
  return d_type;
}

#if 0 /*this is already implemented with dir_open/read/close */
/*this code is useful if you want to see how to make a primitive to return nested arrays*/
struct Object* dir_contents(struct object_heap* oh, char* dirpath) {
#ifdef WIN32
  return FALSE; /*fixme*/
#else
  DIR* dir;
  int count, i;
  word_t lenfilename;
  struct dirent* direntry;
  struct OopArray* array;

  dir = opendir(dirpath);
  if (dir == NULL) {
    return oh->cached.nil;
  }
  /*get a count of the number of entries but we can't trust this*/
  count = 0;
  while ((direntry = readdir(dir))) count++;
  rewinddir(dir);
  i = 0;

  array = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), count);
  heap_fixed_add(oh, (struct Object*)array);

  while ((direntry = readdir(dir)) && i < count) {
    array->elements[i] = (struct Object*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), 2);
    heap_store_into(oh, (struct Object*)array, (struct Object*)array->elements[i]);
    ((struct OopArray*)array->elements[i])->elements[0] = smallint_to_object(slate_direntry_type(direntry->d_type));
    heap_store_into(oh, (struct Object*)array->elements[i], (struct Object*)((struct OopArray*)array->elements[i])->elements[1]);
    lenfilename = strlen(direntry->d_name);
    ((struct OopArray*)array->elements[i])->elements[1] = (struct Object*) heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), lenfilename);
    copy_bytes_into((byte_t*)direntry->d_name, lenfilename, ((struct ByteArray*)((struct OopArray*)array->elements[i])->elements[1])->elements);
    i++;
  }

  closedir(dir);

  heap_fixed_remove(oh, (struct Object*)array);

  return (struct Object*)array;
#endif
}
#endif

bool_t dir_rename_to(struct object_heap* oh, char* src, char* dest) {
#ifdef WIN32
  return FALSE; /*fixme*/
#else
  return (rename(src, dest) == 0 ? TRUE : FALSE);
#endif
}
