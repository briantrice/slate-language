#include "slate.hpp"
#ifdef WIN32
static LARGE_INTEGER liZero;
#endif

void file_module_init(struct object_heap* oh) {
  oh->file_index_size = SLATE_FILES_MAXIMUM;
  oh->file_index = (FILE**)calloc(oh->file_index_size, sizeof(FILE*));
}

bool_t file_handle_isvalid(struct object_heap* oh, word_t file) {
  return (0 <= file && file < oh->file_index_size && oh->file_index[file] != NULL);
}

word_t file_allocate(struct object_heap* oh) {
  word_t file;
  word_t initial_size = oh->file_index_size;
  for (file = 0; file < initial_size; ++ file) {
    if (!(file_handle_isvalid(oh, file)))
      return file;
  }
  oh->file_index_size *= 2;
  oh->file_index = (FILE**)realloc(oh->file_index, oh->file_index_size * sizeof(FILE*));
  assert(oh->file_index);

  for (file = initial_size; file < oh->file_index_size; file++) {
    oh->file_index[file] = NULL;
  }
  
  return initial_size;
}

void file_close(struct object_heap* oh, word_t file) {
  if (file_handle_isvalid(oh, file)) {
#ifdef WIN32
    CloseHandle(oh->file_index[file]);
#else
    fclose (oh->file_index[file]);
#endif
    oh->file_index[file] = NULL;
  }
}

word_t file_open(struct object_heap* oh, struct ByteArray * name, word_t flags) {
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

  file = file_allocate(oh);
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

  if (file_handle_isvalid(oh, file)) {

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

word_t file_write(struct object_heap* oh, word_t file, word_t n, char * bytes) {
#ifdef WIN32
  DWORD bytesWritten = 0;
  return (WriteFile(oh->file_index[file], bytes, (DWORD)n, &bytesWritten, NULL)
	  ? bytesWritten : SLATE_ERROR_RETURN);
#else
  return (file_handle_isvalid(oh, file) ? fwrite (bytes, 1, n, oh->file_index[file])
	  : SLATE_ERROR_RETURN);
#endif
}

word_t file_read(struct object_heap* oh, word_t file, word_t n, char * bytes) {
#ifdef WIN32
  DWORD bytesRead = 0;
  return (file_handle_isvalid(oh, file) && ReadFile(oh->file_index[file], bytes, (DWORD)n, &bytesRead, NULL)
	  ? bytesRead : SLATE_ERROR_RETURN);
#else
  return (file_handle_isvalid(oh, file) ? fread (bytes, 1, n, oh->file_index[file])
	  : SLATE_ERROR_RETURN);
#endif
}

word_t file_sizeof(struct object_heap* oh, word_t file) {
#ifdef WIN32
  LARGE_INTEGER size;
#else
  word_t pos, size;
#endif
  if (!(file_handle_isvalid(oh, file)))
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

word_t file_seek(struct object_heap* oh, word_t file, word_t offset) {
#ifdef WIN32
  LARGE_INTEGER pos;
  LARGE_INTEGER win_offset;
  pos.QuadPart = 0;
  win_offset.QuadPart = offset;
  return (SetFilePointerEx(oh->file_index[file], win_offset, &pos, FILE_BEGIN)
	  ? pos.QuadPart : SLATE_ERROR_RETURN);
#else
  return (file_handle_isvalid(oh, file) && fseek (oh->file_index[file], offset, SEEK_SET) == 0
	  ? ftell (oh->file_index[file]) : SLATE_ERROR_RETURN);
#endif
}

word_t file_tell(struct object_heap* oh, word_t file) {
#ifdef WIN32
  LARGE_INTEGER pos;
#endif
  if (!file_handle_isvalid(oh, file))
    return SLATE_ERROR_RETURN;
#ifdef WIN32
  pos.QuadPart = 0;
  return (SetFilePointerEx(oh->file_index[file], liZero, &pos, FILE_CURRENT)
	  ? pos.QuadPart : SLATE_ERROR_RETURN);
#else
  return ftell(oh->file_index[file]);
#endif
}

bool_t file_isatend(struct object_heap* oh, word_t file) {
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
  if (!(file_handle_isvalid(oh, file)))
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


bool_t file_delete(struct object_heap* oh, char* filename) {
#ifdef WIN32
  return FALSE; /*fixme*/
#else
  return (unlink(filename) == 0 ? TRUE : FALSE);
#endif
}

word_t slate_file_mode(mode_t mode) {
  /*develop a convention that works with other platforms*/
  return mode;
}

struct Object* file_information(struct object_heap* oh, char* filename) {
#ifdef WIN32
  return FALSE; /*fixme*/
#else
  struct stat fileInfo;
  struct OopArray* array;
  if (stat(filename, &fileInfo) != 0) return oh->cached.nil;
  array = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), 5);
  array->elements[0] = smallint_to_object(fileInfo.st_size); /*file size in bytes*/
  array->elements[1] = smallint_to_object(fileInfo.st_atime); /*access time*/
  array->elements[2] = smallint_to_object(fileInfo.st_mtime); /*modification time*/
  array->elements[3] = smallint_to_object(fileInfo.st_ctime); /*creation time/status change*/
  array->elements[4] = smallint_to_object(slate_file_mode(fileInfo.st_mode)); /*mode or file type*/
  
  return (struct Object*)array;
#endif
}

bool_t file_rename_to(struct object_heap* oh, char* src, char* dest) {
#ifdef WIN32
  return FALSE; /*fixme*/
#else
  return (rename(src, dest) == 0 ? TRUE : FALSE);
#endif
}
