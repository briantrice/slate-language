


#ifdef WIN32
#  define EXPORT __declspec(dllexport)
#else
#  define EXPORT
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <stdint.h>
#include <inttypes.h>

 
typedef  intptr_t slate_int_t;


union float_int_union {

  slate_int_t i;
  float f;

};

int main(void) {
   Display *d;
   Window w;
   XEvent e;
   char *msg = "Hello, World!";
   int s;
 
                        /* open connection with the server */
   d = XOpenDisplay(NULL);
   if (d == NULL) {
     fprintf(stderr, "Cannot open display\n");
     exit(1);
   }
 
   s = DefaultScreen(d);
 
                        /* create window */
   w = XCreateSimpleWindow(d, RootWindow(d, s), 10, 10, 100, 100, 1,
                           BlackPixel(d, s), WhitePixel(d, s));
 
                        /* select kind of events we are interested in */
   XSelectInput(d, w, ExposureMask | KeyPressMask);
 
                        /* map (show) the window */
   XMapWindow(d, w);
 
                        /* event loop */
   while (1) {
     XNextEvent(d, &e);
                        /* draw or redraw the window */
     if (e.type == Expose) {
       XFillRectangle(d, w, DefaultGC(d, s), 20, 20, 10, 10);
       XDrawString(d, w, DefaultGC(d, s), 50, 50, msg, strlen(msg));
     }
                        /* exit on key press */
     if (e.type == KeyPress)
       break;
   }
 
                        /* close connection to server */
   XCloseDisplay(d);
 
   return 0;
 }


EXPORT Display* x_open_display(char* name) {
  Display* d;
  d = XOpenDisplay(name);
  printf("Opening X Display '%s' ==> %p\n", name, (void*)d);
  return d;
}


EXPORT void x_close_display(Display* d) {
  XCloseDisplay(d);
}


/*fixme per display*/
char* globalSelection = 0;
int globalSelectionSize = 0;

EXPORT void slate_clipboard_update(Display* d, Window w) {
  Window selectionOwner;
  Atom type;
  int format, result;
  unsigned long len, bytes_left, size;
  unsigned char* data;
  selectionOwner = XGetSelectionOwner (d, XA_PRIMARY);

  if (selectionOwner == None) return;
  if (selectionOwner == w) return; /*already on our globalSelection*/

  XConvertSelection(d, XA_PRIMARY, XA_STRING, None, selectionOwner, CurrentTime);
  XFlush(d);
  XGetWindowProperty(d, selectionOwner, XA_STRING, 0, 0, 0, /*don't grab any*/
                      AnyPropertyType, &type, &format, &len, &bytes_left, &data);
  printf ("type:%i len:%i format:%i byte_left:%i\n", (int)type, (int)len, (int)format, (int)bytes_left);
  if (bytes_left > 0) {
    size = bytes_left;
    result = XGetWindowProperty (d, selectionOwner, XA_STRING, 0, size, 0,
                                 AnyPropertyType, &type, &format, &len, &bytes_left, &data);
    if (result == Success) {
      free(globalSelection);
      globalSelection = malloc(size);
      if (globalSelection == NULL) {
        globalSelectionSize = 0;
        XFree(data);
        return;
      }
      globalSelectionSize = size;
      memcpy(globalSelection, data, size);
    }
    else printf ("FAIL\n");
 
  }
    
}

EXPORT int slate_clipboard_size(Display* d, Window w) {
  return globalSelectionSize;
}

EXPORT void slate_clipboard_copy(Display* d, Window w, char* bytes, int size) {
  free(globalSelection);
  globalSelection = malloc(size);
  if (globalSelection == NULL) {
    globalSelectionSize = 0;
    return;
  }
  globalSelectionSize = size;
  memcpy(globalSelection, bytes, size);
  XSetSelectionOwner(d, XA_PRIMARY, w, CurrentTime);
}


EXPORT void slate_clipboard_paste(Display* d, Window w, char* bytes, int size) {
  if (globalSelection == NULL) return;
  memcpy(bytes, globalSelection, (size < globalSelectionSize) ? size : globalSelectionSize);
}

EXPORT Window x_create_window(Display* d, int width, int height) {
  Window w;
  int s;
  s = DefaultScreen(d);
  
  /* create window */
  w = XCreateSimpleWindow(d, RootWindow(d, s), 10, 10, width, height, 1,
                          BlackPixel(d, s), WhitePixel(d, s));
  
  /* select kind of events we are interested in */
  XSelectInput(d, w, ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask
               | ButtonReleaseMask | PointerMotionMask | KeymapStateMask | StructureNotifyMask);
  
  /* map (show) the window */
  XMapWindow(d, w);
  
  return w;
   
}

XEvent globalEvent;

EXPORT void x_next_event(Display* d) {
  /*we should probably fix this and make it per window...
    also... should we through out extra mouse movements for speed?*/
  for (;;) {
    XEvent reply;

    XNextEvent(d, &globalEvent);
    if (globalEvent.type != SelectionRequest) break;

    printf ("prop:%i tar:%i sel:%i\n", 
            (int)globalEvent.xselectionrequest.property,
            (int)globalEvent.xselectionrequest.target, 
            (int)globalEvent.xselectionrequest.selection);
    if (globalEvent.xselectionrequest.target == XA_STRING)
      {
        XChangeProperty (d,
                         globalEvent.xselectionrequest.requestor,
                         globalEvent.xselectionrequest.property,
                         XA_STRING,
                         8,
                         PropModeReplace,
                         (unsigned char*) globalSelection,
                         globalSelectionSize);
        reply.xselection.property=globalEvent.xselectionrequest.property;
      }
    else
      {
        printf ("No String %i\n",
                (int)globalEvent.xselectionrequest.target);
        reply.xselection.property= None;
      }
    reply.xselection.type= SelectionNotify;
    reply.xselection.display= globalEvent.xselectionrequest.display;
    reply.xselection.requestor= globalEvent.xselectionrequest.requestor;
    reply.xselection.selection=globalEvent.xselectionrequest.selection;
    reply.xselection.target= globalEvent.xselectionrequest.target;
    reply.xselection.time = globalEvent.xselectionrequest.time;
    XSendEvent(d, globalEvent.xselectionrequest.requestor,0,0,&reply);

  }


}

EXPORT int x_event_type() {

  return globalEvent.type;

}

EXPORT int x_event_keyboard_key(Display* display) {
  //KeySym XKeycodeToKeysym(Display *display, KeyCode keycode, int index);
  KeyCode kc;
  KeySym ks, ksHigh, ksLow;
  char buf[16];
  XComposeStatus composeStatus;

  kc = globalEvent.xkey.keycode;
  if (0 != XLookupString(&globalEvent.xkey, buf, 1, &ks, &composeStatus)) {
    return (int)buf[0];
  }

  ks = XKeycodeToKeysym(display, kc, 0);
  if (ks > 0xFF00) {
    return ks & 0xFF;
  }
  XConvertCase(ks, &ksLow, &ksHigh);
  if (globalEvent.xkey.state & 3) { /*shift or capslock?*/
    return ksHigh;
  } else {
    return ksLow;
  }

}

EXPORT int x_event_keyboard_modifiers() {

  return globalEvent.xkey.state;

}

EXPORT int x_event_pointer_x() {

  return globalEvent.xbutton.x;

}

EXPORT int x_event_pointer_y() {

  return globalEvent.xbutton.y;

}

EXPORT int x_event_pointer_state() {

  return globalEvent.xbutton.state;

}

EXPORT int x_event_pointer_button() {

  return globalEvent.xbutton.button;

}

EXPORT int x_event_resize_width() {

  return globalEvent.xresizerequest.width;

}
EXPORT int x_event_resize_height() {

  return globalEvent.xresizerequest.height;

}

EXPORT int x_event_configure_width() {

  return globalEvent.xconfigure.width;

}
EXPORT int x_event_configure_height() {

  return globalEvent.xconfigure.height;

}

EXPORT cairo_t * slate_cairo_create_context(cairo_surface_t *surface)
{
    return cairo_create(surface);
}

EXPORT void slate_cairo_destroy_context(cairo_t *context)
{
    cairo_destroy(context);
}


EXPORT void slate_cairo_paint(cairo_t* c) {

  cairo_show_page(c);


}

EXPORT void slate_cairo_resize(cairo_surface_t* surface, int width, int height) {

  cairo_xlib_surface_set_size(surface, width, height);

}


EXPORT cairo_surface_t * slate_cairo_create_surface(Display* d, Window w, int width, int height) {
  return cairo_xlib_surface_create(d, w, DefaultVisual(d, 0), width, height);

}

EXPORT void slate_cairo_destroy_surface(cairo_surface_t *surface) {
    cairo_surface_destroy(surface);
}



/******************************************************************************
* Context
******************************************************************************/

EXPORT void clip(cairo_t *context)
{
    cairo_clip(context);
}

EXPORT void clip_preserve(cairo_t *context)
{
    cairo_clip_preserve(context);
}


EXPORT void fill(cairo_t *context)
{
    cairo_fill(context);
}

EXPORT void fill_preserve(cairo_t *context)
{
    cairo_fill_preserve(context);
}

EXPORT void restore(cairo_t *context)
{
    cairo_restore(context);
}

EXPORT void save(cairo_t *context)
{
    cairo_save(context);
}

EXPORT slate_int_t get_line_width(cairo_t *context)
{
  union float_int_union u;
  u.f = (float)cairo_get_line_width(context);
  return u.i;
}

EXPORT void set_line_width(cairo_t *context, slate_int_t width)
{
  union float_int_union u;
  u.i = width;
  cairo_set_line_width(context, u.f);
}

EXPORT void set_source_rgb(cairo_t *context, slate_int_t r, slate_int_t g, slate_int_t b)
{
  union float_int_union ru,bu,gu;
  ru.i = r;
  bu.i = b;
  gu.i = g;

  cairo_set_source_rgb(context, ru.f, gu.f, bu.f);
}

EXPORT void set_source_rgba(cairo_t *context, slate_int_t r, slate_int_t g, slate_int_t b, slate_int_t a)
{
  union float_int_union ru,bu,gu,au;
  ru.i = r;
  bu.i = b;
  gu.i = g;
  au.i = a;
  cairo_set_source_rgba(context, ru.f, gu.f, bu.f, au.f);
}

EXPORT void stroke(cairo_t *context)
{
    cairo_stroke(context);
}

EXPORT void stroke_preserve(cairo_t *context)
{
    cairo_stroke_preserve(context);
}

/******************************************************************************
* Path
******************************************************************************/

EXPORT void close_path(cairo_t *context)
{
    cairo_close_path(context);
}

EXPORT void line_to(cairo_t *context, slate_int_t x, slate_int_t y)
{
  union float_int_union xu, yu;
  xu.i = x;
  yu.i = y;
  cairo_line_to(context, xu.f, yu.f);
}

EXPORT void move_to(cairo_t *context, slate_int_t x, slate_int_t y)
{
  union float_int_union xu, yu;
  xu.i = x;
  yu.i = y;
  cairo_move_to(context, xu.f, yu.f);
}

EXPORT void new_path(cairo_t *context)
{
    cairo_new_path(context);
}

EXPORT void rectangle(cairo_t *context, slate_int_t x, slate_int_t y, slate_int_t w, slate_int_t h)
{
  union float_int_union xu, yu, wu, hu;
  xu.i = x;
  yu.i = y;
  wu.i = w;
  hu.i = h;
  cairo_rectangle(context, xu.f, yu.f, wu.f, hu.f);
}

/******************************************************************************
* Transformation
******************************************************************************/

EXPORT void translate(cairo_t *context, slate_int_t x, slate_int_t y)
{
  union float_int_union xu, yu;
  xu.i = x;
  yu.i = y;
  cairo_translate(context, xu.f, yu.f);
}

EXPORT void scale(cairo_t *context, slate_int_t x, slate_int_t y)
{
  union float_int_union xu, yu;
  xu.i = x;
  yu.i = y;
  cairo_scale(context, xu.f, yu.f);
}

/******************************************************************************
* Text
******************************************************************************/

EXPORT void select_font_face(cairo_t *context, const char *family, slate_int_t italic, slate_int_t bold)
{
    cairo_select_font_face(
        context, 
        family, 
        italic ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL, 
        bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
}

EXPORT void set_font_size(cairo_t *context, slate_int_t size)
{
  union float_int_union su;
  su.i = size;
  cairo_set_font_size(context, su.f);
}

EXPORT void show_text(cairo_t *context, const char *text)
{
    if(!*text) // Cairo has insufficient checking
        return;
    cairo_show_text(context, text);
}




/* TODO list:

Context
==========================================================
cairo_status_t cairo_status                 (cairo_t *cr);
cairo_surface_t* cairo_get_target           (cairo_t *cr);
void        cairo_set_source                (cairo_t *cr,
                                             cairo_pattern_t *source);
void        cairo_set_source_surface        (cairo_t *cr,
                                             cairo_surface_t *surface,
                                             double x,
                                             double y);
cairo_pattern_t* cairo_get_source           (cairo_t *cr);
void        cairo_set_antialias             (cairo_t *cr,
                                             cairo_antialias_t antialias);
cairo_antialias_t cairo_get_antialias       (cairo_t *cr);
void        cairo_set_dash                  (cairo_t *cr,
                                             double *dashes,
                                             int num_dashes,
                                             double offset);
void        cairo_set_fill_rule             (cairo_t *cr,
                                             cairo_fill_rule_t fill_rule);
cairo_fill_rule_t cairo_get_fill_rule       (cairo_t *cr);
void        cairo_set_line_cap              (cairo_t *cr,
                                             cairo_line_cap_t line_cap);
cairo_line_cap_t cairo_get_line_cap         (cairo_t *cr);
void        cairo_set_line_join             (cairo_t *cr,
                                             cairo_line_join_t line_join);
cairo_line_join_t cairo_get_line_join       (cairo_t *cr);
void        cairo_set_miter_limit           (cairo_t *cr,
                                             double limit);
double      cairo_get_miter_limit           (cairo_t *cr);
void        cairo_set_operator              (cairo_t *cr,
                                             cairo_operator_t op);
cairo_operator_t cairo_get_operator         (cairo_t *cr);
void        cairo_set_tolerance             (cairo_t *cr,
                                             double tolerance);
double      cairo_get_tolerance             (cairo_t *cr);
void        cairo_reset_clip                (cairo_t *cr);
void        cairo_fill_extents              (cairo_t *cr,
                                             double *x1,
                                             double *y1,
                                             double *x2,
                                             double *y2);
cairo_bool_t cairo_in_fill                  (cairo_t *cr,
                                             double x,
                                             double y);
void        cairo_mask                      (cairo_t *cr,
                                             cairo_pattern_t *pattern);
void        cairo_mask_surface              (cairo_t *cr,
                                             cairo_surface_t *surface,
                                             double surface_x,
                                             double surface_y);
void        cairo_paint                     (cairo_t *cr);
void        cairo_paint_with_alpha          (cairo_t *cr,
                                             double alpha);
void        cairo_stroke_extents            (cairo_t *cr,
                                             double *x1,
                                             double *y1,
                                             double *x2,
                                             double *y2);
cairo_bool_t cairo_in_stroke                (cairo_t *cr,
                                             double x,
                                             double y);
void        cairo_copy_page                 (cairo_t *cr);
void        cairo_show_page                 (cairo_t *cr);

Path
==========================================================
            cairo_path_t;
union       cairo_path_data_t;
enum        cairo_path_data_type_t;
cairo_path_t* cairo_copy_path               (cairo_t *cr);
cairo_path_t* cairo_copy_path_flat          (cairo_t *cr);
void        cairo_path_destroy              (cairo_path_t *path);
void        cairo_append_path               (cairo_t *cr,
                                             cairo_path_t *path);
void        cairo_get_current_point         (cairo_t *cr,
                                             double *x,
                                             double *y);
void        cairo_new_path                  (cairo_t *cr);
void        cairo_close_path                (cairo_t *cr);
void        cairo_arc                       (cairo_t *cr,
                                             double xc,
                                             double yc,
                                             double radius,
                                             double angle1,
                                             double angle2);
void        cairo_arc_negative              (cairo_t *cr,
                                             double xc,
                                             double yc,
                                             double radius,
                                             double angle1,
                                             double angle2);
void        cairo_curve_to                  (cairo_t *cr,
                                             double x1,
                                             double y1,
                                             double x2,
                                             double y2,
                                             double x3,
                                             double y3);
void        cairo_line_to                   (cairo_t *cr,
                                             double x,
                                             double y);
void        cairo_move_to                   (cairo_t *cr,
                                             double x,
                                             double y);
void        cairo_rectangle                 (cairo_t *cr,
                                             double x,
                                             double y,
                                             double width,
                                             double height);
void        cairo_glyph_path                (cairo_t *cr,
                                             cairo_glyph_t *glyphs,
                                             int num_glyphs);
void        cairo_text_path                 (cairo_t *cr,
                                             const char *utf8);
void        cairo_rel_curve_to              (cairo_t *cr,
                                             double dx1,
                                             double dy1,
                                             double dx2,
                                             double dy2,
                                             double dx3,
                                             double dy3);
void        cairo_rel_line_to               (cairo_t *cr,
                                             double dx,
                                             double dy);
void        cairo_rel_move_to               (cairo_t *cr,
                                             double dx,
                                             double dy);

Pattern
==========================================================
typedef     cairo_pattern_t;
void        cairo_pattern_add_color_stop_rgb
                                            (cairo_pattern_t *pattern,
                                             double offset,
                                             double red,
                                             double green,
                                             double blue);
void        cairo_pattern_add_color_stop_rgba
                                            (cairo_pattern_t *pattern,
                                             double offset,
                                             double red,
                                             double green,
                                             double blue,
                                             double alpha);
cairo_pattern_t* cairo_pattern_create_rgb   (double red,
                                             double green,
                                             double blue);
cairo_pattern_t* cairo_pattern_create_rgba  (double red,
                                             double green,
                                             double blue,
                                             double alpha);
cairo_pattern_t* cairo_pattern_create_for_surface
                                            (cairo_surface_t *surface);
cairo_pattern_t* cairo_pattern_create_linear
                                            (double x0,
                                             double y0,
                                             double x1,
                                             double y1);
cairo_pattern_t* cairo_pattern_create_radial
                                            (double cx0,
                                             double cy0,
                                             double radius0,
                                             double cx1,
                                             double cy1,
                                             double radius1);
void        cairo_pattern_destroy           (cairo_pattern_t *pattern);
cairo_pattern_t* cairo_pattern_reference    (cairo_pattern_t *pattern);
cairo_status_t cairo_pattern_status         (cairo_pattern_t *pattern);
enum        cairo_extend_t;
void        cairo_pattern_set_extend        (cairo_pattern_t *pattern,
                                             cairo_extend_t extend);
cairo_extend_t cairo_pattern_get_extend     (cairo_pattern_t *pattern);
enum        cairo_filter_t;
void        cairo_pattern_set_filter        (cairo_pattern_t *pattern,
                                             cairo_filter_t filter);
cairo_filter_t cairo_pattern_get_filter     (cairo_pattern_t *pattern);
void        cairo_pattern_set_matrix        (cairo_pattern_t *pattern,
                                             const cairo_matrix_t *matrix);
void        cairo_pattern_get_matrix        (cairo_pattern_t *pattern,
                                             cairo_matrix_t *matrix);

Transformation
==========================================================
void        cairo_translate                 (cairo_t *cr,
                                             double tx,
                                             double ty);
void        cairo_rotate                    (cairo_t *cr,
                                             double angle);
void        cairo_transform                 (cairo_t *cr,
                                             const cairo_matrix_t *matrix);
void        cairo_set_matrix                (cairo_t *cr,
                                             const cairo_matrix_t *matrix);
void        cairo_get_matrix                (cairo_t *cr,
                                             cairo_matrix_t *matrix);
void        cairo_identity_matrix           (cairo_t *cr);
void        cairo_user_to_device            (cairo_t *cr,
                                             double *x,
                                             double *y);
void        cairo_user_to_device_distance   (cairo_t *cr,
                                             double *dx,
                                             double *dy);
void        cairo_device_to_user            (cairo_t *cr,
                                             double *x,
                                             double *y);
void        cairo_device_to_user_distance   (cairo_t *cr,
                                             double *dx,
                                             double *dy);

Text
==========================================================
            cairo_glyph_t;
enum        cairo_font_slant_t;
enum        cairo_font_weight_t;
void        cairo_set_font_matrix           (cairo_t *cr,
                                             const cairo_matrix_t *matrix);
void        cairo_get_font_matrix           (cairo_t *cr,
                                             cairo_matrix_t *matrix);
void        cairo_set_font_options          (cairo_t *cr,
                                             const cairo_font_options_t *options);
void        cairo_get_font_options          (cairo_t *cr,
                                             cairo_font_options_t *options);
void        cairo_show_glyphs               (cairo_t *cr,
                                             cairo_glyph_t *glyphs,
                                             int num_glyphs);
cairo_font_face_t* cairo_get_font_face      (cairo_t *cr);
void        cairo_font_extents              (cairo_t *cr,
                                             cairo_font_extents_t *extents);
void        cairo_set_font_face             (cairo_t *cr,
                                             cairo_font_face_t *font_face);
void        cairo_text_extents              (cairo_t *cr,
                                             const char *utf8,
                                             cairo_text_extents_t *extents);
void        cairo_glyph_extents             (cairo_t *cr,
                                             cairo_glyph_t *glyphs,
                                             int num_glyphs,
                                             cairo_text_extents_t *extents);

==========================================================
*/


