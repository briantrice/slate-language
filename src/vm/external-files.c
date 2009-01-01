#include "slate.h"

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
    fclose (oh->file_index[file]);
    oh->file_index[file] = NULL;
  }
}

word_t openFile(struct object_heap* oh, struct ByteArray * name, word_t flags) {
  byte_t nameString[SLATE_FILE_NAME_LENGTH];
  word_t nameLength;
  char mode[8];
  word_t modeIndex = 0;
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
      mode[modeIndex++] = 'w';
      if (flags & SF_READ)
	mode[modeIndex++] = '+';
    } else {
      mode[modeIndex++] = 'r';
      mode[modeIndex++] = '+';
    }
  } else if (flags & SF_READ)
    mode[modeIndex++] = 'r';
  else {
    fprintf(stderr, "Slate: Unexpected mode flags for ANSI file module: %" PRIdPTR ", falling back to \"r\"", flags);
    mode[modeIndex++] = 'r';
  }

  mode[modeIndex++] = 'b';
  mode[modeIndex++] = 0;

  oh->file_index[file] = fopen((char*)nameString, mode);
  return (valid_handle(oh, file) ? file : SLATE_ERROR_RETURN);
}

word_t writeFile(struct object_heap* oh, word_t file, word_t n, char * bytes) {
  return (valid_handle(oh, file) ? fwrite (bytes, 1, n, oh->file_index[file])
	  : SLATE_ERROR_RETURN);
}

word_t readFile(struct object_heap* oh, word_t file, word_t n, char * bytes) {
  return (valid_handle(oh, file) ? fread (bytes, 1, n, oh->file_index[file])
	  : SLATE_ERROR_RETURN);
}

word_t sizeOfFile(struct object_heap* oh, word_t file) {
  word_t pos, size;
  if (!(valid_handle(oh, file)))
    return SLATE_ERROR_RETURN;
  pos = ftell (oh->file_index[file]);
  fseek (oh->file_index[file], 0, SEEK_END);
  size = ftell (oh->file_index[file]);
  fseek (oh->file_index[file], pos, SEEK_SET);
  return size;
}

word_t seekFile(struct object_heap* oh, word_t file, word_t offset) {
  return (valid_handle(oh, file) && fseek (oh->file_index[file], offset, SEEK_SET) == 0
	  ? ftell (oh->file_index[file]) : SLATE_ERROR_RETURN);
}

word_t tellFile(struct object_heap* oh, word_t file) {
  return (valid_handle(oh, file) ?  ftell (oh->file_index[file]) : SLATE_ERROR_RETURN);
}

bool_t endOfFile(struct object_heap* oh, word_t file) {
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
}



int getCurrentDirectory(struct ByteArray *wdBuffer) {
  return ((getcwd((char *)wdBuffer->elements, byte_array_size(wdBuffer)) == NULL)
	  ? -errno : strlen((const char *)wdBuffer->elements));
}

int setCurrentDirectory(struct ByteArray *newWd) {
  word_t wdLen = byte_array_size(newWd);
  char *wd = malloc(wdLen + 1);
  if (wd == NULL)
    return -errno;

  memcpy(wd, newWd->elements, wdLen);
  wd[wdLen] = '\0';
  if (chdir(wd) == -1) {
    int savedErrno = errno;
    free(wd);
    return -savedErrno;
  } else {
    free(wd);
    return 0;
  }
}
