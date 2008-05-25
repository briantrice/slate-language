#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

/* Safe because errno values are positive */
#define ENCODE_ERRNO()			(-(errno))
/* Careful, though - evaluates its argument more than once! */
#define RESULT_OR_ERRNO(result)		(((result) == -1) ? ENCODE_ERRNO() : (result))

static char *safe_strdup(char const *src, int len) {
  char *result = malloc(len + 1);
  if (result == NULL) {
    return NULL;
  } else {
    memcpy(result, src, len);
    result[len] = '\0';
    return result;
  }
}

int socket_Receive(int count, int handle, char *array, int start) {
  int result = recv(handle, array + start, count, 0);
  return RESULT_OR_ERRNO(result);
}

int socket_Send(int count, int handle, char *array, int start) {
#ifdef linux
  int result = send(handle, array + start, count, MSG_NOSIGNAL);
#else
  int result = send(handle, array + start, count, 0);
#endif
  return RESULT_OR_ERRNO(result);
}

int socket_Close(int handle) {
  int result = close(handle);
  return RESULT_OR_ERRNO(result);
}

int socket_Shutdown(int handle, int how) {
  int result = shutdown(handle, how);
  return RESULT_OR_ERRNO(result);
}

int socket_GetHostByNameIp4(char const *name, int namelen, char *addressBytes) {
  char *safename = safe_strdup(name, namelen);
  if (safename == NULL) {
    return -1;
  } else {
    struct hostent *he = gethostbyname(name);
    int result;

    if (he == NULL) {
      result = 0;
    } else {
      /* h_addr_list[0] is in network order - big end first. We want
	 little end first, since BigIntegers in slate are
	 little-endian. */
      addressBytes[3] = he->h_addr_list[0][0];
      addressBytes[2] = he->h_addr_list[0][1];
      addressBytes[1] = he->h_addr_list[0][2];
      addressBytes[0] = he->h_addr_list[0][3];
      result = 1;
    }

    free(safename);
    return result;
  }
}

int socket_SocketIp4Stream() {
  int result = socket(PF_INET, SOCK_STREAM, 0);
  return RESULT_OR_ERRNO(result);
}

static void fillIp4Sockaddr(struct sockaddr_in *addr, unsigned int address, int portNumber) {
  memset(addr, 0, sizeof(struct sockaddr_in));
  addr->sin_family = AF_INET;
  addr->sin_port = htons(portNumber);
  addr->sin_addr.s_addr = htonl(address);
}

static int extractIp4Sockaddr(struct sockaddr_in *addr, char *addressBytes) {
  int value = ntohl(addr->sin_addr.s_addr);
  addressBytes[0] = (unsigned char) (value & 0xFF);
  addressBytes[1] = (unsigned char) ((value >> 8) & 0xFF);
  addressBytes[2] = (unsigned char) ((value >> 16) & 0xFF);
  addressBytes[3] = (unsigned char) ((value >> 24) & 0xFF);
  return ntohs(addr->sin_port);
}

int socket_ConnectIp4(int handle, unsigned int address, int portNumber) {
  struct sockaddr_in addr;
  int result;

  fillIp4Sockaddr(&addr, address, portNumber);
  result = connect(handle, (struct sockaddr *) &addr, sizeof(addr));
  return RESULT_OR_ERRNO(result);
}

static int SetSockOptHelper(int handle, int value, int level, int optname) {
  int i = (value ? 1 : 0);
  int result = setsockopt(handle, level, optname, &i, sizeof(i));
  return RESULT_OR_ERRNO(result);
}

int socket_SetReuseAddr(int handle, int value) {
  return SetSockOptHelper(handle, value, SOL_SOCKET, SO_REUSEADDR);
}

int socket_BindIp4(int handle, unsigned int address, int portNumber) {
  struct sockaddr_in addr;
  int result;

  fillIp4Sockaddr(&addr, address, portNumber);
  result = bind(handle, (struct sockaddr *) &addr, sizeof(addr));
  return RESULT_OR_ERRNO(result);
}

int socket_Listen(int handle, int queue) {
  int result = listen(handle, queue ? queue : 5); /* default to 5 if 0 is passed in */
  return RESULT_OR_ERRNO(result);
}

int socket_AcceptIp4(int handle, char *addressBytes, char *portBytes) {
  struct sockaddr_in sa;
  socklen_t salen = sizeof(sa);
  int result = accept(handle, (struct sockaddr *) &sa, &salen);
  if (result == -1) {
    return ENCODE_ERRNO();
  }

  int p = extractIp4Sockaddr(&sa, addressBytes);
  portBytes[0] = (char) (p & 0xFF);
  portBytes[1] = (char) ((p >> 8) & 0xFF);
  return result;
}

int socket_SetNonBlocking(int handle, int value) {
  int flags = fcntl(handle, F_GETFL, O_NONBLOCK);
  int result = fcntl(handle, F_SETFL, (value ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)));
  return RESULT_OR_ERRNO(result);
}

int socket_SetTcpNoDelay(int handle, int value) {
  return SetSockOptHelper(handle, value, IPPROTO_TCP, TCP_NODELAY);
}

int socket_GetSockError(int handle) {
  int sockerr = 0;
  socklen_t sockerrlen = sizeof(sockerr);
  if (getsockopt(handle, SOL_SOCKET, SO_ERROR, &sockerr, &sockerrlen) == -1) {
    return ENCODE_ERRNO();
  } else {
    return sockerr;
  }
}

#define MAX_HANDLESETS 3
static fd_set HandleSets[MAX_HANDLESETS]; /* rset, wset, eset */

int socket_HandleSetClear(int set) {
  if (set >= MAX_HANDLESETS) return -1;
  FD_ZERO(&HandleSets[set]); return 0;
}

int socket_HandleSetIncludes(int set, int fd) {
  if (set >= MAX_HANDLESETS) return -1;
  return FD_ISSET(fd, &HandleSets[set]);
}

int socket_HandleSetAdd(int set, int fd) {
  if (set >= MAX_HANDLESETS) return -1;
  FD_SET(fd, &HandleSets[set]); return 0;
}

int socket_HandleSetRemove(int set, int fd) {
  if (set >= MAX_HANDLESETS) return -1;
  FD_CLR(fd, &HandleSets[set]); return 0;
}

int socket_Select(int count, int timeoutmillis) {
  struct timeval timeout;
  int result;
  timeout.tv_sec = timeoutmillis / 1000;
  timeout.tv_usec = (timeoutmillis % 1000) * 1000; /* microseconds. */
  result = select(count, &HandleSets[0], &HandleSets[1], &HandleSets[2],
		  (timeoutmillis == -1) ? NULL : &timeout);
  return RESULT_OR_ERRNO(result);
}
