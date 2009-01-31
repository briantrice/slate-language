
#include "slate.h"



/* remap platform specific errors to slate errors */
word_t socket_return(word_t ret) {

  if (ret >= 0) return ret;
  perror("socket_return");
  switch (-ret) {
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

void prim_selectOnReadPipesFor(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
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
  case SLATE_DOMAIN_LOCAL: return AF_LOCAL;
  case SLATE_DOMAIN_IPV4: return AF_INET;
  case SLATE_DOMAIN_IPV6: return AF_INET6;
  default: return AF_INET;
  }
}

int socket_lookup_type(word_t type) {
  switch (type) {
  case SLATE_TYPE_STREAM: return SOCK_STREAM;
  default: return SOCK_STREAM;
  }
}

int socket_lookup_protocol(word_t protocol) {
  switch (protocol) {
  default: return 0;
  }
}

int socket_set_nonblocking(int fd) {
    int flags;

    if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


int socket_getaddrinfo(struct object_heap* oh, struct ByteArray* hostname, word_t hostnameSize, struct ByteArray* service, word_t serviceSize, word_t family, word_t type, word_t protocol, word_t flags) {
  int i, pret;
  pthread_t thread;
#if 0
  pthread_mutex_lock(&oh->socketTicketMutex);
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
        oh->socketTickets[i].hostname = calloc(hostnameSize, 1);
        extractCString(hostname, (byte_t*)oh->socketTickets[i].hostname, hostnameSize);
      }
      if (serviceSize == 0) {
        oh->socketTickets[i].service = NULL;
      } else {
        oh->socketTickets[i].service = calloc(hostnameSize, 1);
        extractCString(service, (byte_t*)oh->socketTickets[i].service, serviceSize);
      }

#if 0
      pthread_mutex_unlock(&oh->socketTicketMutex);
#endif
      pret = pthread_create(&thread, NULL, socket_getaddrinfo_callback, (void*) &oh->socketTickets[i]);
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
#if 0
  pthread_mutex_unlock(&oh->socketTicketMutex);
#endif

  return -1;

}


void *socket_getaddrinfo_callback( void *ptr ) {
  struct slate_addrinfo_request* req = ptr;
  /*  struct object_heap* oh = req->oh;*/
  struct addrinfo ai;
  memset(&ai, sizeof(ai), 0);
  ai.ai_flags = req->flags;
  ai.ai_family = socket_lookup_domain(req->family);
  ai.ai_socktype = socket_lookup_type(req->type);
  ai.ai_protocol = socket_lookup_protocol(req->protocol);
  
  req->result = getaddrinfo(req->hostname, req->service, &ai, &req->addrResult);
  req->finished = 1;
  return NULL;
}
