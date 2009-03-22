#include "slate.h"

void prim_fixme(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {

  printf("unimplemented primitive... dying\n");
  assert(0);

}


void prim_closePipe(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[0]);
  int retval;

  ASSURE_SMALLINT_ARG(0);
  retval = close(handle);
  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(retval);

}

void prim_readFromPipe(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* array = (struct ByteArray*) args[0];
  word_t handle = object_to_smallint(args[1]);
  word_t start = object_to_smallint(args[2]), end = object_to_smallint(args[3]);
  ssize_t retval;

  ASSURE_TYPE_ARG(0, TYPE_BYTE_ARRAY);
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  ASSURE_SMALLINT_ARG(3);

  if (start < 0 || start >= byte_array_size(array)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), args[2], args[0], NULL, resultStackPointer);
    return;
  }

  if (end < start || end > byte_array_size(array)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), args[3], args[0], NULL, resultStackPointer);
    return;
  }

  retval = recv(handle, byte_array_elements(array)+start, end - start, 0);


  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(retval);


}

void prim_writeToPipe(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* array = (struct ByteArray*) args[0];
  word_t handle = object_to_smallint(args[1]);
  word_t start = object_to_smallint(args[2]), end = object_to_smallint(args[3]);
  ssize_t retval;

  ASSURE_TYPE_ARG(0, TYPE_BYTE_ARRAY);
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  ASSURE_SMALLINT_ARG(3);

  if (start < 0 || start >= byte_array_size(array)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), args[2], args[0], NULL, resultStackPointer);
    return;
  }

  if (end < start || end > byte_array_size(array)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), args[3], args[0], NULL, resultStackPointer);
    return;
  }

  retval = send(handle, byte_array_elements(array)+start, end - start, 0);

  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(retval);

}

/*fixme this is a copy of the last function with only the select call changed*/
void prim_selectOnWritePipesFor(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct OopArray* selectOn = (struct OopArray*) args[0];
  struct OopArray* readyPipes;
  word_t waitTime = object_to_smallint(args[1]);
  int retval, fdCount, maxFD;
  struct timeval tv;
  fd_set fdList;
  maxFD = 0;

  ASSURE_SMALLINT_ARG(1);

  if ((fdCount = socket_select_setup(selectOn, &fdList, &maxFD)) < 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }

  
  tv.tv_sec = waitTime / 1000000;
  tv.tv_usec = waitTime % 1000000;
  retval = select(maxFD+1, NULL, &fdList, NULL, &tv); 
  
  if (retval < 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }


  readyPipes = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), retval);
  socket_select_find_available(selectOn, &fdList, readyPipes, retval);

  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)readyPipes;

}



void prim_cloneSystem(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  pid_t retval;
  int pipes[2];
  struct OopArray* array;

  /* make two pipes that we can use exclusively in each process to talk to the other */
  /*fixme remap fds for safety*/
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipes) == -1) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }

  retval = fork2();
  
  if (retval == (pid_t)-1) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }

  array = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), 2);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)array;

  if (!retval) {  /* child */
    array->elements[0] = oh->cached.false_object;
    array->elements[1] = smallint_to_object(pipes[0]);
  } else { /* parent */
    array->elements[0] = oh->cached.true_object;
    array->elements[1] = smallint_to_object(pipes[1]);
  }



}

void prim_socketCreate(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t domain = object_to_smallint(args[0]);
  word_t type = object_to_smallint(args[1]);
  word_t protocol = object_to_smallint(args[2]);
  word_t ret = socket(socket_lookup_domain(domain), socket_lookup_type(type), socket_lookup_protocol(protocol));
  int ret2 = 0;

  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  
  if (ret >= 0) {
    ret2 = socket_set_nonblocking(ret);
  } else {
    perror("socket create");
  }

  if (ret2 < 0) {
    perror("set nonblocking");
    oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(-1);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);
  }
}

void prim_socketListen(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t fd = object_to_smallint(args[0]);
  word_t size = object_to_smallint(args[1]);
  word_t ret;

  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);

  ret = listen(fd, size);
  
  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);

}

void prim_socketAccept(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t fd = object_to_smallint(args[0]);
  word_t ret;
  struct sockaddr_storage addr;
  socklen_t len;
  struct ByteArray* addrArray;
  struct OopArray* result;

  ASSURE_SMALLINT_ARG(0);

  len = sizeof(addr);
  ret = accept(fd, (struct sockaddr*)&addr, &len);
  
  if (ret >= 0) {
    addrArray = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), sizeof(struct sockaddr_in));
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);
    return;
  }
  
  heap_fixed_add(oh, (struct Object*)addrArray);
  result = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), 2);
  heap_fixed_remove(oh, (struct Object*)addrArray);

  object_array_set_element(oh, (struct Object*)result, 0, smallint_to_object(ret));
  object_array_set_element(oh, (struct Object*)result, 1, (struct Object*)addrArray);

  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)result;

}

void prim_socketBind(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t fd = object_to_smallint(args[0]);
  struct ByteArray* address = (struct ByteArray*) args[1];
  word_t ret;

  ASSURE_SMALLINT_ARG(0);

  ret = bind(fd, (const struct sockaddr*)byte_array_elements(address), (socklen_t)byte_array_size(address));
  if (ret < 0) perror("bind");
  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);
}

void prim_socketConnect(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t fd = object_to_smallint(args[0]);
  struct ByteArray* address = (struct ByteArray*) args[1];
  word_t ret;

  ASSURE_SMALLINT_ARG(0);

  ret = connect(fd, (const struct sockaddr*)byte_array_elements(address), (socklen_t)byte_array_size(address));
  
  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);

}

void prim_socketGetError(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t fd = object_to_smallint(args[0]);
  word_t ret;
  int optval;
  socklen_t optlen;
  optlen = 4;
  ASSURE_SMALLINT_ARG(0);

  ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, (socklen_t*)&optlen);

  if (ret == 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(optval);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);
  }

}

void prim_getAddrInfo(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* hostname = (struct ByteArray*)args[1];
  struct ByteArray* service = (struct ByteArray*)args[2];
  word_t family = object_to_smallint(args[3]);
  word_t type = object_to_smallint(args[4]);
  word_t protocol = object_to_smallint(args[5]);
  word_t flags = object_to_smallint(args[6]);
  word_t ret, serviceSize, hostnameSize;

  ASSURE_TYPE_ARG(1, TYPE_BYTE_ARRAY);
  ASSURE_SMALLINT_ARG(3);
  ASSURE_SMALLINT_ARG(4);
  ASSURE_SMALLINT_ARG(5);
  ASSURE_SMALLINT_ARG(6);

  if ((struct Object*)hostname == oh->cached.nil) {
    hostnameSize = 0;
  } else {
    hostnameSize = byte_array_size(hostname)+1;
  }

  if ((struct Object*)service == oh->cached.nil) {
    serviceSize = 0;
  } else {
    ASSURE_TYPE_ARG(2, TYPE_BYTE_ARRAY);
    serviceSize = byte_array_size(service)+1;
  }

  ret = socket_getaddrinfo(oh, hostname, hostnameSize, service, serviceSize, family, type, protocol, flags);

  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);

}


void prim_getAddrInfoResult(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t ticket = object_to_smallint(args[0]);
  if (ticket >= oh->socketTicketCount || ticket < 0
      || oh->socketTickets[ticket].inUse == 0 || oh->socketTickets[ticket].finished == 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }
  if (oh->socketTickets[ticket].result < 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(socket_return(oh->socketTickets[ticket].result));
  } else {
    word_t count, i;
    struct addrinfo* ai = oh->socketTickets[ticket].addrResult;
    struct addrinfo* current = ai;
    struct OopArray* retval;
    count = 0;
    while (current != NULL) {
      current = current->ai_next;
      count++;
    }
    current = ai;
    retval = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), count);
    heap_fixed_add(oh, (struct Object*)retval);
    oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)retval;
    heap_store_into(oh, (struct Object*)oh->cached.interpreter->stack, (struct Object*)retval);
    
    for (i = 0; i < count; i++) {
      struct OopArray* aResult = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), 6);
      struct ByteArray* aResultAddr;
      struct ByteArray* aResultCanonName;
      word_t canonNameLen = (current->ai_canonname == NULL)? 0 : strlen(current->ai_canonname);
      retval->elements[i] = (struct Object*)aResult;
      heap_store_into(oh, (struct Object*)retval, retval->elements[i]);
      aResult->elements[0] = smallint_to_object(current->ai_flags);
      aResult->elements[1] = smallint_to_object(socket_reverse_lookup_domain(current->ai_family));
      aResult->elements[2] = smallint_to_object(socket_reverse_lookup_type(current->ai_socktype));
      aResult->elements[3] = smallint_to_object(socket_reverse_lookup_protocol(current->ai_protocol));

      aResultAddr = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), current->ai_addrlen);
      aResult->elements[4] = (struct Object*)aResultAddr;
      heap_store_into(oh, (struct Object*)aResult, aResult->elements[4]);
      copy_bytes_into((byte_t*)current->ai_addr, current->ai_addrlen, aResultAddr->elements);
      if (canonNameLen == 0) {
        aResult->elements[5] = oh->cached.nil;
      } else {
        aResultCanonName = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), canonNameLen);
        aResult->elements[5] = (struct Object*)aResultCanonName;
        heap_store_into(oh, (struct Object*)aResult, aResult->elements[5]);
        copy_bytes_into((byte_t*)current->ai_canonname, canonNameLen, aResultCanonName->elements);
      }

      current = current->ai_next;
    }

    heap_fixed_remove(oh, (struct Object*)retval);


  }
}

void prim_freeAddrInfoResult(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t ticket = object_to_smallint(args[0]);
  if (ticket >= oh->socketTicketCount || ticket < 0
      || oh->socketTickets[ticket].inUse == 0 || oh->socketTickets[ticket].finished == 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
    return;
  }
  free(oh->socketTickets[ticket].hostname);
  oh->socketTickets[ticket].hostname = 0;
  free(oh->socketTickets[ticket].service);
  oh->socketTickets[ticket].service = 0;
  freeaddrinfo(oh->socketTickets[ticket].addrResult);
  oh->socketTickets[ticket].addrResult = 0;
  
  oh->socketTickets[ticket].inUse = 0;
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
}


void prim_socketCreateIP(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t domain = object_to_smallint(args[0]);
  struct Object* address = args[1];
  word_t port = object_to_smallint(args[2]);
  /*  struct OopArray* options = (struct OopArray*) args[3];*/
  struct sockaddr_in* sin;
  struct sockaddr_in6* sin6;
  struct sockaddr_un* sun;
  struct ByteArray* ret;
  
  ASSURE_SMALLINT_ARG(0);

  switch (domain) {

  case SLATE_DOMAIN_LOCAL:
    if (byte_array_size((struct ByteArray*)address) > 100) {
      ret = (struct ByteArray*)oh->cached.nil;
      break;
    }
    ret = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), sizeof(struct sockaddr_un));
    sun = (struct sockaddr_un*)byte_array_elements(ret);
    sun->sun_family = socket_lookup_domain(domain);
    ASSURE_TYPE_ARG(1, TYPE_BYTE_ARRAY);
    strncpy(sun->sun_path, (char*)byte_array_elements((struct ByteArray*)address), 100);
    sun->sun_path[byte_array_size((struct ByteArray*)address)] = '\0';
    break;

  case SLATE_DOMAIN_IPV4:
    ASSURE_SMALLINT_ARG(2);
    if (object_array_size(address) < 4) {
      ret = (struct ByteArray*)oh->cached.nil;
      break;
    }
    ret = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), sizeof(struct sockaddr_in));
    sin = (struct sockaddr_in*)byte_array_elements(ret);
    sin->sin_family = socket_lookup_domain(domain);
    sin->sin_port = htons((uint16_t)port);
    ASSURE_TYPE_ARG(1, TYPE_OOP_ARRAY);
    sin->sin_addr.s_addr = htonl(((object_to_smallint(object_array_get_element(address, 0)) & 0xFF) << 24)
      | ((object_to_smallint(object_array_get_element(address, 1)) & 0xFF) << 16)
      | ((object_to_smallint(object_array_get_element(address, 2)) & 0xFF) << 8)
      | (object_to_smallint(object_array_get_element(address, 3)) & 0xFF));
    break;

    /*fixme ipv6*/
  case SLATE_DOMAIN_IPV6:
    ASSURE_SMALLINT_ARG(2);
    if (object_array_size(address) < 16) {
      ret = (struct ByteArray*)oh->cached.nil;
      break;
    }
    ret = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), sizeof(struct sockaddr_in6));
    sin6 = (struct sockaddr_in6*)byte_array_elements(ret);
    sin6->sin6_family = socket_lookup_domain(domain);
    sin6->sin6_port = htons((uint16_t)port);
    ASSURE_TYPE_ARG(1, TYPE_OOP_ARRAY);
    {
      int i;
      for (i = 0; i < 16; i++)
        sin6->sin6_addr.s6_addr[i] = object_to_smallint(object_array_get_element(address, i)) & 0xFF;
    }
    break;
    
  default:
    ret = (struct ByteArray*)oh->cached.nil;
    break;
  }

  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)ret;

}

void prim_write_to_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *console=args[0], *n=args[1], *handle=args[2], *seq=args[3], *start=args[4];
  byte_t* bytes = &((struct ByteArray*)seq)->elements[0] + object_to_smallint(start);
  word_t size = object_to_smallint(n);


  ASSURE_SMALLINT_ARG(2);
  ASSURE_SMALLINT_ARG(4);

  assert(arity == 5 && console != NULL);

  oh->cached.interpreter->stack->elements[resultStackPointer] =
                         smallint_to_object(fwrite(bytes, 1, size, (object_to_smallint(handle) == 0)? stdout : stderr));

}


void prim_close(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]);
  ASSURE_SMALLINT_ARG(1);

  closeFile(oh, handle);
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

}

void prim_readConsole_from_into_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t /*handle = object_to_smallint(args[2]),*/ n = object_to_smallint(args[1]), start = object_to_smallint(args[4]);
  struct ByteArray* bytes = (struct ByteArray*)args[3];
  word_t retval;

  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(4);

  retval = fread((char*)(byte_array_elements(bytes) + start), 1, n, stdin);
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);

}

void prim_read_from_into_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[2]), n = object_to_smallint(args[1]), start = object_to_smallint(args[4]);
  struct ByteArray* bytes = (struct ByteArray*)args[3];
  word_t retval;
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(4);
  retval = readFile(oh, handle, n, (char*)(byte_array_elements(bytes) + start));
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}

void prim_write_to_from_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[2]), n = object_to_smallint(args[1]), start = object_to_smallint(args[4]);
  struct ByteArray* bytes = (struct ByteArray*)args[3];
  word_t retval;
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(4);
  retval = writeFile(oh, handle, n, (char*)(byte_array_elements(bytes) + start));
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}


void prim_reposition_to(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]), n = object_to_smallint(args[2]);
  word_t retval = seekFile(oh, handle, n);
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}

void prim_positionOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]);
  word_t retval = tellFile(oh, handle);
  ASSURE_SMALLINT_ARG(1);
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}

void prim_bytesPerWord(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(sizeof(word_t));
}

void prim_timeSinceEpoch(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t time;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  time = (word_t)tv.tv_sec * 1000000 + (word_t)tv.tv_usec;
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(time);
}

void prim_atEndOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]);
  ASSURE_SMALLINT_ARG(1);
  if (endOfFile(oh, handle)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
  }
}

void prim_sizeOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]);
  word_t retval = sizeOfFile(oh, handle);
  ASSURE_SMALLINT_ARG(1);
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}



void prim_flush_output(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  /*struct Object *console=args[0];*/
  fflush(stdout);
  fflush(stderr);
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
}

void prim_handle_for(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle;
  struct Object /**file=args[0],*/ *fname=args[1];

  handle = openFile(oh, (struct ByteArray*)fname, SF_READ|SF_WRITE);
  if (handle >= 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(handle);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

void prim_handleForNew(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle;
  struct Object /**file=args[0],*/ *fname=args[1];

  handle = openFile(oh, (struct ByteArray*)fname, SF_READ|SF_WRITE|SF_CLEAR|SF_CREATE);
  if (handle >= 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(handle);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}


void prim_handle_for_input(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle;
  struct Object /**file=args[0],*/ *fname=args[1];

  handle = openFile(oh, (struct ByteArray*)fname, SF_READ);
  if (handle >= 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(handle);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}


void prim_addressOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *handle=args[1], *offset=args[2];
  struct ByteArray* addressBuffer=(struct ByteArray*) args[3];
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  if (object_is_smallint(handle) && object_is_smallint(offset) && byte_array_size(addressBuffer) >= sizeof(word_t)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] =
                           smallint_to_object(addressOfMemory(oh,
                                                              object_to_smallint(handle), 
                                                              object_to_smallint(offset),
                                                              byte_array_elements(addressBuffer)));
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

void prim_loadLibrary(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *libname=args[1], *handle = args[2];

  if (openExternalLibrary(oh, (struct ByteArray*)libname, (struct ByteArray*)handle)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
  }

}

void prim_closeLibrary(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *handle=args[1];

  if (closeExternalLibrary(oh, (struct ByteArray*) handle)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
  }

}


void prim_procAddressOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *handle=args[2], *symname=args[1];
  struct ByteArray* addressBuffer=(struct ByteArray*) args[3];

  if (lookupExternalLibraryPrimitive(oh, (struct ByteArray*) handle, (struct ByteArray *) symname, addressBuffer)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
  }

}

void prim_extlibError(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* messageBuffer=(struct ByteArray*) args[1];

  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(readExternalLibraryError(messageBuffer));
}


void prim_applyExternal(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  oh->cached.interpreter->stack->elements[resultStackPointer] =
                         applyExternalLibraryPrimitive(oh, (struct ByteArray*)args[1], 
                                                       (struct OopArray*)args[2],
                                                       args[3],
                                                       args[4],
                                                       (struct OopArray*)args[5]);

}

void prim_newFixedArea(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *size=args[1];
  word_t handle;

  if (!object_is_smallint(size)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }

  handle = openMemory(oh, object_to_smallint(size));
  if (handle >= 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(handle);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

void prim_closeFixedArea(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct Object* handle = args[1];
  if (object_is_smallint(handle)) {
    closeMemory(oh, object_to_smallint(handle));
  }
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

}

void prim_fixedAreaSize(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct Object* handle = args[1];
  if (object_is_smallint(handle)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(sizeOfMemory(oh, object_to_smallint(handle)));
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

void prim_fixedAreaAddRef(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct Object* handle = args[1];
  if (object_is_smallint(handle)) {
    addRefMemory(oh, object_to_smallint(handle));
  }

  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

}

void prim_fixedReadFromStarting(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct ByteArray* buf = (struct ByteArray*)args[0];
  word_t amount = object_to_smallint(args[1]), startingAt = object_to_smallint(args[3]),
    handle = object_to_smallint(args[2]);

  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  ASSURE_SMALLINT_ARG(3);

  if (!validMemoryHandle(oh, handle) 
      || byte_array_size(buf) < amount 
      || startingAt + amount >= oh->memory_sizes [handle]) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(-1);
    return;
  }
  
  oh->cached.interpreter->stack->elements[resultStackPointer] =
    smallint_to_object(writeMemory(oh, handle, startingAt, amount, byte_array_elements(buf)));

}

void prim_fixedWriteFromStarting(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct ByteArray* buf = (struct ByteArray*)args[0];
  word_t amount = object_to_smallint(args[1]), startingAt = object_to_smallint(args[3]),
    handle = object_to_smallint(args[2]);

  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  ASSURE_SMALLINT_ARG(3);

  if (!validMemoryHandle(oh, handle) 
      || byte_array_size(buf) < amount 
      || startingAt + amount >= oh->memory_sizes [handle]) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(-1);
    return;
  }
  
  oh->cached.interpreter->stack->elements[resultStackPointer] = 
    smallint_to_object(readMemory(oh, handle, startingAt, amount, byte_array_elements(buf)));

}


void prim_fixedAreaResize(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct Object* handle = args[1], *size = args[2];
  if (object_is_smallint(handle) && object_is_smallint(size)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = 
                           smallint_to_object(resizeMemory(oh, object_to_smallint(handle), object_to_smallint(size)));

  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

void prim_smallint_at_slot_named(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj;
  struct Object* name;
  struct SlotEntry * se;
  struct Object * proto;
  
  obj = args[0];
  name = args[1];
  proto = get_special(oh, SPECIAL_OOP_SMALL_INT_PROTO);
  se = slot_table_entry_for_name(oh, proto->map->slotTable, (struct Symbol*)name);
  if (se == NULL) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
  } else {
    word_t offset = object_to_smallint(se->offset);
    oh->cached.interpreter->stack->elements[resultStackPointer] =  object_slot_value_at_offset(proto, offset);
  }


}



void prim_bytesize(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(payload_size(args[0]));
}

void prim_findon(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct MethodDefinition* def;
  struct Symbol* selector= (struct Symbol*) args[0];
  struct OopArray* arguments= (struct OopArray*) args[1];


  def = method_dispatch_on(oh, selector, arguments->elements, array_size(arguments), NULL);

  oh->cached.interpreter->stack->elements[resultStackPointer] = (def == NULL ? oh->cached.nil : (struct Object*) def->method);
}



void prim_ensure(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {

  struct Closure* body = (struct Closure*) args[0];
  struct Object* ensureHandler = args[1];
  interpreter_apply_to_arity_with_optionals(oh, oh->cached.interpreter, body, NULL, 0, NULL, resultStackPointer);
  /*the registers are already allocated on the stack so we don't worry about overwriting them*/
  interpreter_stack_push(oh, oh->cached.interpreter, oh->cached.interpreter->ensureHandlers);
  interpreter_stack_push(oh, oh->cached.interpreter, ensureHandler);
  oh->cached.interpreter->ensureHandlers = smallint_to_object(oh->cached.interpreter->stackPointer - 2);
#ifdef PRINT_DEBUG_ENSURE
  printf("ensure handlers at %" PRIdPTR "\n", oh->cached.interpreter->stackPointer - 2);
#endif

}


void prim_frame_pointer_of(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {

  struct Interpreter* i = (struct Interpreter*)args[0];
  struct Symbol* selector = (struct Symbol*) args[1];
  struct CompiledMethod* method;
  word_t frame = i->framePointer;



  while (frame > FUNCTION_FRAME_SIZE) {
    method = (struct CompiledMethod*) i->stack->elements[frame-3];
    method = method->method; /*incase it's a closure and not a compiledmethod*/
    if (method->selector == selector) {
      oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(frame);
      return;
    }
    frame = object_to_smallint(i->stack->elements[frame-1]);
  }

  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

}


void prim_bytearray_newsize(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj, *i;
  obj = args[0];
  i = args[1];
  
  if (!object_is_smallint(i)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }

  oh->cached.interpreter->stack->elements[resultStackPointer] = 
                         (struct Object*)heap_clone_byte_array_sized(oh, obj, (object_to_smallint(i) < 0) ? 0 : object_to_smallint(i));
}


void prim_byteat_put(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj= args[0], *i=args[1], *val = args[2];
  word_t index;

  index = object_to_smallint(i);

  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);

  if (object_is_immutable(obj)) {
    interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_IMMUTABLE), obj, NULL, resultStackPointer);
    return;
  }
  
  if (index < byte_array_size((struct ByteArray*)obj)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(byte_array_set_element((struct ByteArray*)obj, index, object_to_smallint(val)));
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), i, obj, NULL, resultStackPointer);
  }

}


void prim_byteat(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj, *i;
  word_t index;

  obj = args[0];
  i = args[1];
  index = object_to_smallint(i);
  
  ASSURE_SMALLINT_ARG(1);

  if (index < byte_array_size((struct ByteArray*)obj)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(byte_array_get_element(obj, index));
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), i, obj, NULL, resultStackPointer);
  }

}


void prim_map(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj;
  obj = args[0];
  
  if (object_is_smallint(obj)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)obj->map;
  }


}

void prim_set_map(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj;
  struct Map* map;
  obj = args[0];
  map = (struct Map*)args[1];
  
  if (object_is_smallint(obj) || object_is_immutable(obj)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  } else {
    object_change_map(oh, obj, map);
    heap_store_into(oh, args[0], args[1]);
    oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)map;
  }

}

/*obsolete*/
void prim_run_args_into(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* arguments = (struct ByteArray*)args[1];
  oh->cached.interpreter->stack->elements[resultStackPointer] = 
                         smallint_to_object(write_args_into(oh, (char*)byte_array_elements(arguments), byte_array_size(arguments)));

  
}

void prim_vmArgCount(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(oh->argcSaved);
}

void prim_vmArg(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  word_t i;
  int len;
  struct ByteArray* array;
  ASSURE_SMALLINT_ARG(1);  
  i = object_to_smallint(args[1]);
  len = strlen(oh->argvSaved[i]);

  array = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), len);
  copy_bytes_into((byte_t*)oh->argvSaved[i], len, array->elements);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)array;
  heap_store_into(oh, (struct Object*)oh->cached.interpreter->stack, (struct Object*)array);
}

void prim_exit(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  /*  print_stack_types(oh, 128);*/
  /*  print_backtrace(oh);*/
  printf("Slate process %d exiting...\n", getpid());
  exit(0);
}

void prim_isIdenticalTo(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = (args[0]==args[1])? oh->cached.true_object : oh->cached.false_object;
}

void prim_identity_hash(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  /*fix*/
  /*  print_detail(oh, args[0]);
      print_backtrace(oh);*/
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(object_hash(args[0]));
}

void prim_identity_hash_univ(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  /*fix*/
  /*  print_detail(oh, args[0]);
      print_backtrace(oh);*/
  if (object_is_smallint(args[0])) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = args[0];
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(object_hash(args[0]));
  }
}


void prim_clone(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  if (object_is_smallint(args[0])) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = args[0];
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = heap_clone(oh, args[0]);
  }
}

void prim_applyto(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals, word_t resultStackPointer) {
  struct Closure *method = (struct Closure*)args[0];
  struct OopArray* argArray = (struct OopArray*) args[1];
  struct OopArray* opts = NULL;

  if (optionals != NULL && optionals->elements[1] != oh->cached.nil) {
    opts = (struct OopArray*) optionals->elements[1];
  }
  interpreter_apply_to_arity_with_optionals(oh, oh->cached.interpreter, method,
                                            argArray->elements, array_size(argArray), opts, resultStackPointer);


}


void prim_at(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* array;
  word_t i;

  array = args[0];
  i = object_to_smallint(args[1]);
  ASSURE_SMALLINT_ARG(1);  
  if (i < object_array_size(array) && i >= 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = ((struct OopArray*)array)->elements[i];
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), args[1], args[0], NULL, resultStackPointer);

  }


}


void prim_at_put(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *array = args[0], *i = args[1], *val = args[2];
  word_t index = object_to_smallint(i);

  ASSURE_SMALLINT_ARG(1);
  if (object_is_immutable(array)) {
    interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_IMMUTABLE), array, NULL, resultStackPointer);
    return;
  }
  
  if (index < object_array_size(array)) {
    heap_store_into(oh, array, val);
    oh->cached.interpreter->stack->elements[resultStackPointer] =  ((struct OopArray*)array)->elements[index] = val;
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), i, array, NULL, resultStackPointer);
  }
}


void prim_ooparray_newsize(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  /*struct Object* array = args[0];*/
  struct Object* i = args[1];
  if (object_is_smallint(i)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)
                           heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO),
                                                      object_to_smallint(i));
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }
}


void prim_equals(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = (args[0] == args[1])?oh->cached.true_object:oh->cached.false_object;
}

void prim_less_than(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {

  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);
  oh->cached.interpreter->stack->elements[resultStackPointer] = 
    (object_to_smallint(args[0])<object_to_smallint(args[1]))?oh->cached.true_object:oh->cached.false_object;
}


void prim_size(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(object_array_size(args[0]));
}

void prim_bitand(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)((word_t)args[0] & (word_t)args[1]);
}
void prim_bitor(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)((word_t)args[0] | (word_t)args[1]);
}
void prim_bitxor(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)(((word_t)args[0] ^ (word_t)args[1])|SMALLINT_MASK);
}
void prim_bitnot(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  ASSURE_SMALLINT_ARG(0);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)(~((word_t)args[0]) | SMALLINT_MASK);
}

void prim_smallIntegerMinimum(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)(((word_t)1<< (sizeof(word_t)*8-1))|1); /*top and smallint bit set*/
}

void prim_smallIntegerMaximum(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)LONG_MAX; /*has all bits except the top set*/
}

void prim_plus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  word_t z = object_to_smallint(x) + object_to_smallint(y);


  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);


  if (smallint_fits_object(z)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(z);
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_ADD_OVERFLOW), x, y, NULL, resultStackPointer);
  }
}

void prim_removefrom(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {

  struct Object *method = args[0], *traitsWindow;
  struct OopArray* roles = (struct OopArray*)args[1];
  struct Symbol* selector = (struct Symbol*)oh->cached.nil;
  struct MethodDefinition* def;
  word_t i;

  traitsWindow = method->map->delegates->elements[0];

  if (traitsWindow == oh->cached.closure_method_window || traitsWindow == oh->cached.compiled_method_window) {
    selector = ((struct Closure*)method)->method->selector;
  } else {
    /*May only remove a CompiledMethod or Closure.*/
    assert(0);
  }

  def = method_is_on_arity(oh, method, selector, array_elements(roles), array_size(roles));
  if (def == NULL) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = method;
    return;
  }

  for (i = 0; i < array_size(roles); i++) {
    struct Object* role = array_elements(roles)[i];
    if (!object_is_smallint(role)) {
      object_remove_role(oh, role, selector, def);
    }
  }

  method_flush_cache(oh, selector);
  oh->cached.interpreter->stack->elements[resultStackPointer] = method;

}


void prim_exponent(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* x = (struct ByteArray*)args[0];
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object((*(word_t*)float_part(x) >> FLOAT_EXPONENT_OFFSET) & 0xFF);

}

void prim_significand(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* x = (struct ByteArray*)args[0];
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(*(word_t*)float_part(x) & FLOAT_SIGNIFICAND);

}

void prim_getcwd(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* buf = (struct ByteArray*)args[1];
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(getCurrentDirectory(buf));
}
void prim_setcwd(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* buf = (struct ByteArray*)args[1];
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(setCurrentDirectory(buf));
}


void prim_float_equals(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  if (*float_part(x) == *float_part(y)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
  }
}

void prim_float_less_than(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  if (*float_part(x) < *float_part(y)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
  }
}

void prim_float_plus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) + *float_part(y);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_minus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) - *float_part(y);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}


void prim_float_times(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) * *float_part(y);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_divide(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) / *float_part(y);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_raisedTo(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = pow(*float_part(x), *float_part(y));
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_ln(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = log(*float_part(x));
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_exp(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = exp(*float_part(x));
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_sin(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = sin(*float_part(x));
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_withSignificand_exponent(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  /*this is really a bytearray*/
  word_t significand = object_to_smallint(args[1]), exponent = object_to_smallint(args[2]);
  struct ByteArray* f = heap_new_float(oh);
  *((word_t*)float_part(f)) = significand | exponent << FLOAT_EXPONENT_OFFSET;

  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)f;

}

void prim_bitshift(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  word_t bits = object_to_smallint(args[0]);
  word_t shift = object_to_smallint(args[1]);
  word_t z;

  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);

  if (shift >= 0) {
    if (shift >= __WORDSIZE && bits != 0) {
      interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_BIT_SHIFT_OVERFLOW), args[0], args[1], NULL, resultStackPointer);
      return;
    }

    z = bits << shift;

    if (!smallint_fits_object(z) || z >> shift != bits) {
      interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_BIT_SHIFT_OVERFLOW), args[0], args[1], NULL, resultStackPointer);
      return;
    }

  } else if (shift <= -__WORDSIZE) {
    z = bits >> (__WORDSIZE-1);
  } else {
    z = bits >> -shift;
  }
  
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(z);

}


void prim_minus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  word_t z = object_to_smallint(x) - object_to_smallint(y);

  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);

  if (smallint_fits_object(z)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(z);
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SUBTRACT_OVERFLOW), x, y, NULL, resultStackPointer);
  }
}


void prim_times(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  word_t x = object_to_smallint(args[0]);
  word_t y = object_to_smallint(args[1]);
  word_t z = x * y;


  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);


  if (y != 0 && (z / y != x || !smallint_fits_object(z))) { /*thanks slava*/
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_MULTIPLY_OVERFLOW), args[0], args[1], NULL, resultStackPointer);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(z);
  }
}

void prim_quo(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* x = args[0];
  struct Object* y = args[1];

  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);

  if (object_to_smallint(y) == 0) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_DIVIDE_BY_ZERO), x, y, NULL, resultStackPointer);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(object_to_smallint(x) / object_to_smallint(y));
  }
}


void prim_forward_to(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  oh->cached.interpreter->stack->elements[resultStackPointer] = y;

  if (!object_is_smallint(x) && !object_is_smallint(y) && x != y) {
    heap_forward(oh, x, y);
    heap_gc(oh);
  }

}


void prim_clone_setting_slots(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *obj = args[0], *slotArray = args[1], *valueArray = args[2], *newObj;
  word_t i;

  if (object_is_smallint(obj)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = obj;
    return;
  }
  newObj = heap_clone(oh, obj);

  /*fix, check that arrays are same size, and signal errors*/

  for (i = 0; i < object_array_size(slotArray); i++) {
    struct Symbol* name = (struct Symbol*)object_array_get_element(slotArray, i);
    struct SlotEntry* se = slot_table_entry_for_name(oh, obj->map->slotTable, name);
    if (se == NULL) {
      interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, (struct Object*)name, NULL, resultStackPointer);
    } else {
      /*since the object was just cloned, we aren't expecting a tenured obj to point to a new one*/
      object_slot_value_at_offset_put(oh, newObj, object_to_smallint(se->offset), object_array_get_element(valueArray, i));
    }
  }
  
  oh->cached.interpreter->stack->elements[resultStackPointer] = newObj;
}



void prim_as_method_on(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct MethodDefinition* def;
  struct Object *method = args[0], *roles=args[2];
  struct Symbol *selector = (struct Symbol*)args[1];
  struct Object* traitsWindow = method->map->delegates->elements[0];

   
  if (traitsWindow == get_special(oh, SPECIAL_OOP_CLOSURE_WINDOW)) {
    struct Closure* closure = (struct Closure*)heap_clone(oh, method);
    heap_fixed_add(oh, (struct Object*)closure);
    closure->method = (struct CompiledMethod*)heap_clone(oh, (struct Object*)closure->method);
    heap_fixed_remove(oh, (struct Object*)closure);
    heap_store_into(oh, (struct Object*)closure, (struct Object*)closure->method);
    closure->method->method = closure->method;
    closure->method->selector = selector;
    method = (struct Object*)closure;
  } else {
    struct CompiledMethod* closure= (struct CompiledMethod*)heap_clone(oh, method);
    closure->method = closure;
    closure->selector = selector;
    method = (struct Object*) closure;
  }
  def = method_define(oh, method, (struct Symbol*)selector, ((struct OopArray*)roles)->elements, object_array_size(roles));
  def->slotAccessor = oh->cached.nil;
  method_flush_cache(oh, selector);
#ifdef PRINT_DEBUG_DEFUN
  printf("Defining function '"); print_symbol(selector);
  printf("' on: ");
  if (!print_printname(oh, ((struct OopArray*)roles)->elements[0])) printf("NoRole");
  {
    word_t i;
    for (i = 1; i < object_array_size(roles); i++) {
      printf(", ");
      if (!print_printname(oh, ((struct OopArray*)roles)->elements[i])) printf("NoRole");
    }
  }
  printf("\n");
#endif

  oh->cached.interpreter->stack->elements[resultStackPointer] = method;
}


void prim_at_slot_named(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj;
  struct Object* name;
  struct SlotEntry * se;
  
  obj = args[0];
  name = args[1];
  
  if (object_is_smallint(obj)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
  } else {
    se = slot_table_entry_for_name(oh, obj->map->slotTable, (struct Symbol*)name);
    if (se == NULL) {
      interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
    } else {
      word_t offset = object_to_smallint(se->offset);
      oh->cached.interpreter->stack->elements[resultStackPointer] = object_slot_value_at_offset(obj, offset);
    }
  }


}


void prim_at_slot_named_put(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj=args[0], *val=args[2];
  struct Object* name = args[1];
  struct SlotEntry * se;
  struct Map* map;
  
  if (object_is_smallint(obj)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
    return;
  }

  if (object_is_immutable(obj)) {
    interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_IMMUTABLE), obj, NULL, resultStackPointer);
    return;
  }

  map = obj->map;
  se = slot_table_entry_for_name(oh, map->slotTable, (struct Symbol*)name);

  if (se == NULL) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
  } else {
    word_t offset = object_to_smallint(se->offset);
    oh->cached.interpreter->stack->elements[resultStackPointer] = object_slot_value_at_offset_put(oh, obj, offset, val);
  }

 /*note: not supporting delegate slots*/

}




void prim_clone_with_slot_valued(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj = args[0], *value = args[2];
  struct Symbol* name = (struct Symbol*)args[1];

  if (object_is_smallint(obj)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, (struct Object*)name, NULL, resultStackPointer);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = object_add_slot_named(oh, obj, name, value);
  }
}

void prim_clone_without_slot(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj = args[0];
  struct Symbol* name = (struct Symbol*)args[1];

  if (object_is_smallint(obj)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, (struct Object*)name, NULL, resultStackPointer);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = object_remove_slot(oh, obj, name);
  }
}



void prim_send_to(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals, word_t resultStackPointer) {
  struct Symbol* selector = (struct Symbol*)args[0];
  struct OopArray* opts, *arguments = (struct OopArray*)args[1];

  if (optionals == NULL) {
    opts = NULL;
  } else {
    opts = (struct OopArray*)optionals->elements[1];
    if (opts == (struct OopArray*)oh->cached.nil) opts = NULL;
  }
  send_to_through_arity_with_optionals(oh, selector, array_elements(arguments), array_elements(arguments), array_size(arguments), opts, resultStackPointer); 
}

void prim_send_to_through(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals, word_t resultStackPointer) {
  struct Symbol* selector = (struct Symbol*)args[0];
  struct OopArray* opts, * arguments = (struct OopArray*)args[1], *dispatchers = (struct OopArray*)args[2];

  if (optionals == NULL) {
    opts = NULL;
  } else {
    opts = (struct OopArray*)optionals->elements[1];
    if (opts == (struct OopArray*)oh->cached.nil) opts = NULL;
  }
  /*fix check array sizes are the same*/
  send_to_through_arity_with_optionals(oh, selector, array_elements(arguments), array_elements(dispatchers), array_size(arguments), opts, resultStackPointer); 
}




void prim_as_accessor(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *method = args[0], *slot = args[2];
  struct OopArray *roles = (struct OopArray*)args[3];
  struct Symbol* selector = (struct Symbol*)args[1];
  struct Object* traitsWindow = method->map->delegates->elements[0];
  struct MethodDefinition* def;
  
  if (traitsWindow == oh->cached.closure_method_window) {
    struct Closure* closure = (struct Closure*)heap_clone(oh, method);
    heap_fixed_add(oh, (struct Object*)closure);
    closure->method = (struct CompiledMethod*)heap_clone(oh, (struct Object*)closure->method);
    heap_fixed_remove(oh, (struct Object*)closure);
    heap_store_into(oh, (struct Object*)closure, (struct Object*)closure->method);
    closure->method->method = closure->method;
    closure->method->selector = selector;
    method = (struct Object*)closure;
  } else if (traitsWindow == oh->cached.compiled_method_window){
    struct CompiledMethod* closure = (struct CompiledMethod*)heap_clone(oh, method);
    closure->method = closure;
    closure->selector = selector;
    method = (struct Object*) closure;
  }

  def = method_define(oh, method, selector, roles->elements, array_size(roles));
  def->slotAccessor = slot;
  method_flush_cache(oh, selector);
  oh->cached.interpreter->stack->elements[resultStackPointer] = method;

#ifdef PRINT_DEBUG_DEFUN
  printf("Defining accessor '"); print_symbol(selector);
  printf("' on: ");
  if (!print_printname(oh, ((struct OopArray*)roles)->elements[0])) printf("NoRole");
  {
    word_t i;
    for (i = 1; i < array_size(roles); i++) {
      printf(", ");
      if (!print_printname(oh, ((struct OopArray*)roles)->elements[i])) printf("NoRole");
    }
  }
  printf("\n");
#endif

}



void prim_save_image(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  char nameString [SLATE_FILE_NAME_LENGTH];
  struct slate_image_header sih;
  struct Object* name = args[1];
  size_t nameLength = payload_size(name);
  FILE * imageFile;

  word_t totalSize, forwardPointerEntryCount;
  byte_t* memoryStart;
  struct Object *writeObject;
  struct ForwardPointerEntry* forwardPointers;
  /* do a full gc, allocate a new chunk of memory the size of the young and old combined,
   * copy all the non-free objects to the new memory while keeping an array of the position changes,
   * go through the memory and fix up the pointers, adjust points to start from 0 instead of memoryStart,
   * and write the header and the memory out to disk
   */

  /*push true so if it resumes from the save image, it will do init code*/
  /*fixme*/
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;

  if (nameLength >= sizeof(nameString)) {
    /*interpreter_stack_pop(oh, oh->cached.interpreter);*/
    /*push nil*/
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }
  memcpy(nameString, (char*)byte_array_elements((struct ByteArray*)name), nameLength);
  nameString[nameLength] = '\0';

  imageFile = fopen(nameString, "wb");
  if (!imageFile) {
    /*interpreter_stack_pop(oh, oh->cached.interpreter);*/
    /*push nil*/
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

    return;
  }
  printf("Saving image to %s\n", nameString);
  heap_full_gc(oh);
  totalSize = oh->memoryOldSize + oh->memoryYoungSize;
  forwardPointerEntryCount = ((totalSize / 4) + sizeof(struct ForwardPointerEntry) - 1) / sizeof(struct ForwardPointerEntry);
  memoryStart = calloc(1, totalSize);
  writeObject = (struct Object*)memoryStart;
  forwardPointers = calloc(1, forwardPointerEntryCount * sizeof(struct ForwardPointerEntry));
  assert(memoryStart != NULL);
  copy_used_objects(oh, &writeObject, oh->memoryOld, oh->memoryOldSize, forwardPointers, forwardPointerEntryCount);
  copy_used_objects(oh, &writeObject, oh->memoryYoung, oh->memoryYoungSize, forwardPointers, forwardPointerEntryCount);
  totalSize = (byte_t*)writeObject - memoryStart;
  adjust_object_fields_with_table(oh, memoryStart, totalSize, forwardPointers, forwardPointerEntryCount);
  adjust_oop_pointers_from(oh, 0-(word_t)memoryStart, memoryStart, totalSize);
  sih.magic = SLATE_IMAGE_MAGIC;
  sih.size = totalSize;
  sih.next_hash = heap_new_hash(oh);
  sih.special_objects_oop = (byte_t*) (forward_pointer_hash_get(forwardPointers, forwardPointerEntryCount, (struct Object*)oh->special_objects_oop)->toObj) - memoryStart;
  sih.current_dispatch_id = oh->current_dispatch_id;

  fwrite(&sih, sizeof(struct slate_image_header), 1, imageFile);
  fwrite(memoryStart, 1, totalSize, imageFile);
  fclose(imageFile);
  free(forwardPointers);
  free(memoryStart);

  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
  /*
  interpreter_stack_pop(oh, oh->cached.interpreter);
  interpreter_push_false(oh, oh->cached.interpreter);
  */
  

}



void (*primitives[]) (struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) = {

 /*0-9*/ prim_as_method_on, prim_as_accessor, prim_map, prim_set_map, prim_fixme, prim_removefrom, prim_clone, prim_clone_setting_slots, prim_clone_with_slot_valued, prim_fixme, 
 /*10-9*/ prim_fixme, prim_fixme, prim_clone_without_slot, prim_at_slot_named, prim_smallint_at_slot_named, prim_at_slot_named_put, prim_forward_to, prim_bytearray_newsize, prim_bytesize, prim_byteat, 
 /*20-9*/ prim_byteat_put, prim_ooparray_newsize, prim_size, prim_at, prim_at_put, prim_ensure, prim_applyto, prim_send_to, prim_send_to_through, prim_findon, 
 /*30-9*/ prim_fixme, prim_run_args_into, prim_exit, prim_isIdenticalTo, prim_identity_hash, prim_identity_hash_univ, prim_equals, prim_less_than, prim_bitor, prim_bitand, 
 /*40-9*/ prim_bitxor, prim_bitnot, prim_bitshift, prim_plus, prim_minus, prim_times, prim_quo, prim_fixme, prim_fixme, prim_frame_pointer_of, 
 /*50-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_bytesPerWord, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, 
 /*60-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_readConsole_from_into_starting_at, prim_write_to_starting_at, prim_flush_output, prim_handle_for, prim_handle_for_input, prim_fixme, 
 /*70-9*/ prim_handleForNew, prim_close, prim_read_from_into_starting_at, prim_write_to_from_starting_at, prim_reposition_to, prim_positionOf, prim_atEndOf, prim_sizeOf, prim_save_image, prim_fixme, 
 /*80-9*/ prim_fixme, prim_fixme, prim_getcwd, prim_setcwd, prim_significand, prim_exponent, prim_withSignificand_exponent, prim_float_equals, prim_float_less_than, prim_float_plus, 
 /*90-9*/ prim_float_minus, prim_float_times, prim_float_divide, prim_float_raisedTo, prim_float_ln, prim_float_exp, prim_float_sin, prim_fixme, prim_fixme, prim_fixme, 
 /*00-9*/ prim_fixme, prim_fixme, prim_fixme, prim_newFixedArea, prim_closeFixedArea, prim_fixedAreaAddRef, prim_fixedWriteFromStarting, prim_fixedReadFromStarting, prim_fixedAreaSize, prim_fixedAreaResize,
 /*10-9*/ prim_addressOf, prim_loadLibrary, prim_closeLibrary, prim_procAddressOf, prim_extlibError, prim_applyExternal, prim_timeSinceEpoch, prim_cloneSystem, prim_readFromPipe, prim_writeToPipe,
 /*20-9*/ prim_selectOnReadPipesFor, prim_selectOnWritePipesFor, prim_closePipe, prim_socketCreate, prim_socketListen, prim_socketAccept, prim_socketBind, prim_socketConnect, prim_socketCreateIP, prim_smallIntegerMinimum,
 /*30-9*/ prim_smallIntegerMaximum, prim_socketGetError, prim_getAddrInfo, prim_getAddrInfoResult, prim_freeAddrInfoResult, prim_vmArgCount, prim_vmArg, prim_fixme, prim_fixme, prim_fixme,
 /*40-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme,
 /*50-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme,

};
