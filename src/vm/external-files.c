#include "slate.h"
#ifdef WIN32
static LARGE_INTEGER liZero;
#endif

bool_t valid_handle(struct object_heap* oh, word_t file) {
  return (0 <= file && file < oh->file_index_size && oh->file_index[file] != NULL);
}

word_t allocate_file(struct object_heap* oh) {
  word_t file;
  word_t initial_size = oh->file_index_size;
  for (file = 0; file < initial_size; ++ file) {
    if (!(valid_handle(oh, file)))
      return file;
  }
  oh->file_index_size *= 2;
  oh->file_index = realloc(oh->file_index, oh->file_index_size * sizeof(FILE*));
  assert(oh->file_index);

  for (file = initial_size; file < oh->file_index_size; file++) {
    oh->file_index[file] = NULL;
  }
  
  return initial_size;
}

void closeFile(struct object_heap* oh, word_t file) {
  if (valid_handle(oh, file)) {
#ifdef WIN32
    CloseHandle(oh->file_index[file]);
#else
    fclose (oh->file_index[file]);
#endif
    oh->file_index[file] = NULL;
  }
}

word_t openFile(struct object_heap* oh, struct ByteArray * name, word_t flags) {
  byte_t nameString[SLATE_FILE_NAME_LENGTH];
  word_t nameLength;
#ifdef WIN32
  DWORD rwMode = 0;
  DWORD openMode = 0;
#else
  char mode[8];
  word_t modeIndex = 0;
#endif
  word_t file;

  nameLength = extractCString(name, nameString, sizeof(nameString));

  if (nameLength <= 0)
    return SLATE_ERROR_RETURN;

  file = allocate_file(oh);
  if (file < 0)
    return SLATE_ERROR_RETURN;

  /* (CLEAR \/ CREATE) /\ !WRITE */
  if ((flags & SF_CLEAR || flags & SF_CREATE) && (! (flags & SF_WRITE)))
    error("ANSI does not support clearing or creating files that are not opened for writing");

  /* WRITE /\ !READ */
  if (flags & SF_WRITE && (! (flags & SF_READ)))
    error("ANSI does not support opening files for writing only");

  if (flags & SF_WRITE) {
    if (flags & SF_CLEAR) {
#ifdef WIN32
      rwMode |= GENERIC_WRITE;
#else
      mode[modeIndex++] = 'w';
#endif
      if (flags & SF_READ)
#ifdef WIN32
	rwMode |= GENERIC_READ;
#else
	mode[modeIndex++] = '+';
#endif
    } else {
#ifdef WIN32
      rwMode |= GENERIC_READ;
#else
      mode[modeIndex++] = 'r';
      mode[modeIndex++] = '+';
#endif
    }
  } else if (flags & SF_READ)
#ifdef WIN32
    rwMode |= GENERIC_READ;
#else
    mode[modeIndex++] = 'r';
#endif
  else {
    fprintf(stderr, "Slate: Unexpected mode flags for ANSI file module: %" PRIdPTR ", falling back to \"r\"", flags);
#ifdef WIN32
    rwMode |= GENERIC_READ;
#else
    mode[modeIndex++] = 'r';
#endif
  };

#ifdef WIN32
  openMode = ((flags & SF_CREATE) ? OPEN_ALWAYS : OPEN_EXISTING);
#else
  mode[modeIndex++] = 'b';
  mode[modeIndex++] = 0;
#endif

#ifdef WIN32
  oh->file_index[file] = CreateFile(nameString, rwMode, FILE_SHARE_READ, 0, openMode, FILE_ATTRIBUTE_NORMAL, 0);
#else
  oh->file_index[file] = fopen((char*)nameString, mode);
#endif

  if (valid_handle(oh, file)) {

#ifdef WIN32
    if (flags & SF_CLEAR) {
      if (!SetFilePointerEx(oh->file_index[file], liZero, NULL, FILE_BEGIN) || !SetEndOfFile(oh->file_index[file])) {
	CloseHandle(oh->file_index[file]);
	oh->file_index[file] = 0;
	return SLATE_ERROR_RETURN;
      }
    }
#endif
    return file;
  } else
    return SLATE_ERROR_RETURN;
}

word_t writeFile(struct object_heap* oh, word_t file, word_t n, char * bytes) {
#ifdef WIN32
  DWORD bytesWritten = 0;
  return (WriteFile(oh->file_index[file], bytes, (DWORD)n, &bytesWritten, NULL)
	  ? bytesWritten : SLATE_ERROR_RETURN);
#else
  return (valid_handle(oh, file) ? fwrite (bytes, 1, n, oh->file_index[file])
	  : SLATE_ERROR_RETURN);
#endif
}

word_t readFile(struct object_heap* oh, word_t file, word_t n, char * bytes) {
#ifdef WIN32
  DWORD bytesRead = 0;
  return (valid_handle(oh, file) && ReadFile(oh->file_index[file], bytes, (DWORD)n, &bytesRead, NULL)
	  ? bytesRead : SLATE_ERROR_RETURN);
#else
  return (valid_handle(oh, file) ? fread (bytes, 1, n, oh->file_index[file])
	  : SLATE_ERROR_RETURN);
#endif
}

word_t sizeOfFile(struct object_heap* oh, word_t file) {
#ifdef WIN32
  LARGE_INTEGER size;
#else
  word_t pos, size;
#endif
  if (!(valid_handle(oh, file)))
    return SLATE_ERROR_RETURN;
#ifdef WIN32
  size.QuadPart = 0;
  return (GetFileSizeEx(oh->file_index[file], &size) ? size.QuadPart : SLATE_ERROR_RETURN);
#else
  pos = ftell (oh->file_index[file]);
  fseek (oh->file_index[file], 0, SEEK_END);
  size = ftell (oh->file_index[file]);
  fseek (oh->file_index[file], pos, SEEK_SET);
  return size;
#endif
}

word_t seekFile(struct object_heap* oh, word_t file, word_t offset) {
#ifdef WIN32
  LARGE_INTEGER pos;
  LARGE_INTEGER win_offset;
  pos.QuadPart = 0;
  win_offset.QuadPart = offset;
  return (SetFilePointerEx(oh->file_index[file], win_offset, &pos, FILE_BEGIN)
	  ? pos.QuadPart : SLATE_ERROR_RETURN);
#else
  return (valid_handle(oh, file) && fseek (oh->file_index[file], offset, SEEK_SET) == 0
	  ? ftell (oh->file_index[file]) : SLATE_ERROR_RETURN);
#endif
}

word_t tellFile(struct object_heap* oh, word_t file) {
#ifdef WIN32
  LARGE_INTEGER pos;
#endif
  if (!valid_handle(oh, file))
    return SLATE_ERROR_RETURN;
#ifdef WIN32
  pos.QuadPart = 0;
  return (SetFilePointerEx(oh->file_index[file], liZero, &pos, FILE_CURRENT)
	  ? pos.QuadPart : SLATE_ERROR_RETURN);
#else
  return ftell(oh->file_index[file]);
#endif
}

bool_t endOfFile(struct object_heap* oh, word_t file) {
#ifdef WIN32
  LARGE_INTEGER size;
  LARGE_INTEGER pos;
  size.QuadPart = 0;
  pos.QuadPart = 0;
  // seek to current position to read current position (I love M$)
  if (!SetFilePointerEx(oh->file_index[file], liZero, &pos, FILE_CURRENT))
    return SLATE_ERROR_RETURN;
  else
    return (GetFileSizeEx(oh->file_index[file], &size)
	    ? pos.QuadPart >= size.QuadPart : SLATE_ERROR_RETURN);
#else
  word_t c;
  if (!(valid_handle(oh, file)))
    return TRUE;
  c = fgetc (oh->file_index[file]);
  if (c == EOF)
    return TRUE;
  else {
    ungetc (c, oh->file_index[file]);
    return FALSE;
  }
#endif
}

int getCurrentDirectory(struct ByteArray *pathBuffer) {
#ifdef WIN32
  return GetCurrentDirectory((DWORD)payload_size((struct Object *) pathBuffer), pathBuffer->elements);
#else
  return ((getcwd((char *)pathBuffer->elements, byte_array_size(pathBuffer)) == NULL)
	  ? -errno : strlen((const char *)pathBuffer->elements));
#endif
}

int setCurrentDirectory(struct ByteArray *newpath) {
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
