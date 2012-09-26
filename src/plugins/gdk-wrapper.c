#ifdef WIN32
#  define EXPORT __declspec(dllexport)
#else
#  define EXPORT
#endif

#include <gdk/gdk.h>
#include <stdio.h>_lib
#include <stdlib.h>

EXPORT void wrapper_gdk_lib_init( void ) {
        if( g_thread_supported() ) {
                printf("g_thread NOT supported\n");
                exit(-1);
        }
        g_thread_init(NULL);
        gdk_threads_init();
}
