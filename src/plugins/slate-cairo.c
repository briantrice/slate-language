// Workaround for OS X ExternalLibrary float problem.

#ifdef _MSC_VER
#  pragma warning(disable: 4005) // warning in Cairo headers
#endif

#include <cairo.h>
#include <stdint.h>
typedef  intptr_t slate_int_t;

#ifndef EXPORT
#  ifdef _WIN32
#    define EXPORT __declspec(dllexport)
#  else
#    define EXPORT
#  endif
#endif

/******************************************************************************
* Construction / Destruction
******************************************************************************/

EXPORT cairo_surface_t *image_surface_create_for_data(
    unsigned char *data, slate_int_t format, slate_int_t width, slate_int_t height, slate_int_t stride)
{
    return cairo_image_surface_create_for_data(data, format, width, height, stride);
}

EXPORT void surface_destroy(cairo_surface_t *surface)
{
    cairo_surface_destroy(surface);
}

EXPORT cairo_t *create(cairo_surface_t *surface)
{
    return cairo_create(surface);
}

EXPORT void destroy(cairo_t *context)
{
    cairo_destroy(context);
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
    float f = (float)cairo_get_line_width(context);
    return *(slate_int_t*)&f;
}

EXPORT void set_line_width(cairo_t *context, slate_int_t width)
{
    cairo_set_line_width(context, *(float*)&width);
}

EXPORT void set_source_rgb(cairo_t *context, slate_int_t r, slate_int_t g, slate_int_t b)
{
    cairo_set_source_rgb(context, *(float*)&r, *(float*)&g, *(float*)&b);
}

EXPORT void set_source_rgba(cairo_t *context, slate_int_t r, slate_int_t g, slate_int_t b, slate_int_t a)
{
    cairo_set_source_rgba(context, *(float*)&r, *(float*)&g, *(float*)&b, *(float*)&a);
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
    cairo_line_to(context, *(float*)&x, *(float*)&y);
}

EXPORT void move_to(cairo_t *context, slate_int_t x, slate_int_t y)
{
    cairo_move_to(context, *(float*)&x, *(float*)&y);
}

EXPORT void new_path(cairo_t *context)
{
    cairo_new_path(context);
}

EXPORT void rectangle(cairo_t *context, slate_int_t x, slate_int_t y, slate_int_t w, slate_int_t h)
{
    cairo_rectangle(context, *(float*)&x, *(float*)&y, *(float*)&w, *(float*)&h);
}

/******************************************************************************
* Transformation
******************************************************************************/

EXPORT void translate(cairo_t *context, slate_int_t tx, slate_int_t ty)
{
    cairo_translate(context, *(float*)&tx, *(float*)&ty);
}

EXPORT void scale(cairo_t *context, slate_int_t sx, slate_int_t sy)
{
    cairo_scale(context, *(float*)&sx, *(float*)&sy);
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
    cairo_set_font_size(context, *(float*)&size);
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

