#ifdef WIN32
#  define EXPORT __declspec(dllexport)
#else
#  define EXPORT
#endif

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>


EXPORT GtkTreeIter *wrapper_gtk_tree_iter_new(void) {
	return g_new0(GtkTreeIter, 1);
}

EXPORT GtkTextIter *wrapper_gtk_text_iter_new(void) {
	return g_new0(GtkTextIter, 1);
}

void wrapper_gtk_main( void ) {
	gdk_threads_enter(); //The book says to call this begore gtk_main
	gtk_main();
	gdk_threads_leave();
}

EXPORT void wrapper_gtk_lib_init( void ) {
	gtk_init ( 0, NULL);
	g_thread_create((GThreadFunc)wrapper_gtk_main, NULL, FALSE, NULL);
}

