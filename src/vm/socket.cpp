#include "slate.hpp"

void socket_module_init(struct object_heap* oh) {
  oh->socketTicketCount = SLATE_NETTICKET_MAXIMUM;
  oh->socketTickets = (struct slate_addrinfo_request*)calloc(oh->socketTicketCount * sizeof(struct slate_addrinfo_request), 1);
#if 0
  oh->socketTicketMutex = PTHREAD_MUTEX_INITIALIZER;
#endif
}

/* remap platform specific errors to slate errors */
word_t socket_return(word_t ret) {

  if (ret >= 0) return ret;
  
  switch (-ret) {
#ifdef WIN32
          // TODO WIN32 port
#else
  case EACCES: return -2;
  case EAFNOSUPPORT: return -3;
  case EINVAL: return -4;
  case EMFILE: return -5;
  case ENFILE: return -5;
  case ENOMEM: return -6;
  case EPROTONOSUPPORT: return -3;
  case EADDRINUSE: return -7;
  case EBADF: return -8;
  case ENOTSOCK: return -8;
  case EFAULT: return -4;
  case EOPNOTSUPP: return -3;
  case EAGAIN: return -9;
    /*  case EWOULDBLOCK: return -10;*/
  case ECONNABORTED: return -11;
  case EINTR: return -12;
  case EPERM: return -2;
  case EALREADY: return -13;
  case ECONNREFUSED: return -14;
  case EINPROGRESS: return -15;
  case EISCONN: return -16;
  case ENETUNREACH: return -17;
  case ETIMEDOUT: return -18;
#endif

  default: return -1;
  }

}

int socket_select_setup(struct OopArray* selectOn, fd_set* fdList, int* maxFD) {
  word_t fdCount, i;
 
  FD_ZERO(fdList);
  fdCount = array_size(selectOn);

  for (i = 0; i < fdCount; i++) {
    struct Object* fdo = object_array_get_element((struct Object*)selectOn, i);
    word_t fd = object_to_smallint(fdo);
    if (!object_is_smallint(fdo) || fd >= FD_SETSIZE) {
      return -1;
    }
    *maxFD = max(*maxFD, fd);
    FD_SET(fd, fdList);
    assert(FD_ISSET(fd, fdList));
  }

  return fdCount;
}

void socket_select_find_available(struct OopArray* selectOn, fd_set* fdList, struct OopArray* readyPipes, word_t readyCount) {
  word_t fdCount, i, readyIndex;
  fdCount = array_size(selectOn);

  for (i = 0, readyIndex = 0; i < fdCount && readyIndex < readyCount; i++) {
    if (FD_ISSET(object_to_smallint(object_array_get_element((struct Object*)selectOn, i)), fdList)) {
      readyPipes->elements[readyIndex++] = object_array_get_element((struct Object*)selectOn, i);
    }
  }
}

void prim_selectOnReadPipesFor(struct object_heap* oh, struct Object* args[], word_t arity, struct Object* opts[], word_t optCount, word_t resultStackPointer) {
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
  retval = select(maxFD+1, &fdList, NULL, NULL, &tv); 
  
  if (retval < 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }


  readyPipes = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), retval);
  socket_select_find_available(selectOn, &fdList, readyPipes, retval);

  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)readyPipes;

}


int socket_lookup_domain(word_t domain) {
  switch (domain) {
  case SLATE_DOMAIN_LOCAL: return AF_UNIX;
  case SLATE_DOMAIN_IPV4: return AF_INET;
  case SLATE_DOMAIN_IPV6: return AF_INET6;
  default: return AF_INET;
  }
}

int socket_reverse_lookup_domain(word_t domain) {
  switch (domain) {
  case AF_UNIX: return SLATE_DOMAIN_LOCAL;
  case AF_INET: return SLATE_DOMAIN_IPV4;
  case AF_INET6: return SLATE_DOMAIN_IPV6;
  default: return SLATE_DOMAIN_IPV4;
  }
}

int socket_lookup_type(word_t type) {
  switch (type) {
  case SLATE_TYPE_STREAM: return SOCK_STREAM;
  default: return SOCK_STREAM;
  }
}

int socket_reverse_lookup_type(word_t type) {
  switch (type) {
  case SOCK_STREAM: return SLATE_TYPE_STREAM;
  default: return SLATE_TYPE_STREAM;
  }
}

int socket_lookup_protocol(word_t protocol) {
  switch (protocol) {
  default: return 0;
  }
}

int socket_reverse_lookup_protocol(word_t protocol) {
  switch (protocol) {
  default: return 0;
  }
}

int socket_set_nonblocking(int fd) {
    int flags;

#ifdef WIN32
#pragma message("TODO WIN32 port set-nonblocking on socket")
        return 0;
#else
        if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

#ifdef WIN32
DWORD WINAPI socket_getaddrinfo_callback(LPVOID ptr) {
#else
void *socket_getaddrinfo_callback(void *ptr) {
#endif
  struct slate_addrinfo_request* req = (struct slate_addrinfo_request*)ptr;
  /*  struct object_heap* oh = req->oh;*/
  struct addrinfo ai;
  memset(&ai, 0, sizeof(ai));
  ai.ai_flags = req->flags;
  ai.ai_family = socket_lookup_domain(req->family);
  ai.ai_socktype = socket_lookup_type(req->type);
  ai.ai_protocol = socket_lookup_protocol(req->protocol);
  
  req->result = getaddrinfo(req->hostname, req->service, &ai, &req->addrResult);
  req->finished = 1;
#ifdef WIN32
  return 0;
#else
  return NULL;
#endif
}

int socket_getaddrinfo(struct object_heap* oh, struct ByteArray* hostname, word_t hostnameSize, struct ByteArray* service, word_t serviceSize, word_t family, word_t type, word_t protocol, word_t flags) {
  int i;
#ifdef WIN32
  HANDLE thread;
  DWORD pret;
  oh->socketThreadMutex = CreateMutex(NULL, FALSE, TEXT("SocketThreadMutex"));
#else
  pthread_t thread;
  int pret;
#if 0
  pthread_mutex_lock(&oh->socketTicketMutex);
#endif
#endif
  for (i = 0; i < oh->socketTicketCount; i++) {
    if (oh->socketTickets[i].inUse == 0) {
      /*use this one*/
      oh->socketTickets[i].inUse = 1;
      oh->socketTickets[i].result = 0;
      oh->socketTickets[i].finished = 0;
      oh->socketTickets[i].addrResult = 0;
      oh->socketTickets[i].family = family;
      oh->socketTickets[i].type = type;
      oh->socketTickets[i].protocol = protocol;
      oh->socketTickets[i].flags = flags;
      oh->socketTickets[i].oh = oh;
      oh->socketTickets[i].ticketNumber = i;
      if (hostnameSize == 0) {
        oh->socketTickets[i].hostname = NULL;
      } else {
        oh->socketTickets[i].hostname = (char*)calloc(hostnameSize, 1);
        extractCString(hostname, (byte_t*)oh->socketTickets[i].hostname, hostnameSize);
      }
      if (serviceSize == 0) {
        oh->socketTickets[i].service = NULL;
      } else {
        oh->socketTickets[i].service = (char*)calloc(hostnameSize, 1);
        extractCString(service, (byte_t*)oh->socketTickets[i].service, serviceSize);
      }

#ifdef WIN32
          ReleaseMutex(oh->socketThreadMutex);
          thread = CreateThread(NULL, 0, socket_getaddrinfo_callback, (void*) &oh->socketTickets[i], 0, &pret);
#else
#if 0
      pthread_mutex_unlock(&oh->socketTicketMutex);
#endif
      pret = pthread_create(&thread, NULL, socket_getaddrinfo_callback, (void*) &oh->socketTickets[i]);
#endif
          if (pret != 0) {
        free(oh->socketTickets[i].hostname);
        free(oh->socketTickets[i].service);
        oh->socketTickets[i].hostname = NULL;
        oh->socketTickets[i].service = NULL;
        return -1;
      }
      return i;
    }
  }
#ifdef WIN32
  ReleaseMutex(oh->socketThreadMutex);
#else
#if 0
  pthread_mutex_unlock(&oh->socketTicketMutex);
#endif
#endif

  return -1;

}
