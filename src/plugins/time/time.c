#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

struct SlateTimeval {
  long seconds;
  long microseconds;
};

int slate_GetTimeOfDay(struct SlateTimeval *stv) {
  struct timeval tv;
  int result = gettimeofday(&tv, NULL);
  stv->seconds = tv.tv_sec;
  stv->microseconds = tv.tv_usec;
  return result;
}

int slate_GetClockTime () {
  return clock();
}

int slate_GetClocksPerSecond () {
  return CLOCKS_PER_SEC;
}

char* slate_GetCalendarTimeStringInSeconds () {
  /* Returns "DDD MMM dd hh:mm:ss YYYY\n\0" */
  const time_t current_time = time(NULL);
  return ctime(&current_time);
}

char* slate_GetStandardTimeZoneName () {
  return tzname[0];
}

char* slate_GetDSTTimeZoneName () {
#if 0    /* POSIX */
  const time_t current_time = time(NULL);
  struct tm * local_time = localtime(&current_time);
  return local_time->tm_zone;
#endif   /* ANSI */
  return tzname[1];
}

long int slate_GetTimeZoneOffsetInSeconds () {
#ifdef __CYGWIN__
  tzset();
  return -(int)timezone;
#else
  const time_t current_time = time(NULL);
  struct tm * local_time = localtime(&current_time);
  return local_time->tm_gmtoff;
#endif
}

int slate_IsDSTRelevant () {
  const time_t current_time = time(NULL);
  struct tm * local_time = localtime(&current_time);
  return local_time->tm_isdst;
}
