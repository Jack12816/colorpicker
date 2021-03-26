/* Copyright (c) 2015 Jonas Kulla <Nyocurio@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>

#include <cairo.h>
#include <cairo-xlib.h>

// X11 vars
Display * x_display;
Window x_overlay_window;
Window x_root_window;

// GTK vars
GdkWindow * g_root_window;
GdkCursor * g_cursor;

gint preview_rectangle_timer = 0;
gboolean short_format        = 0;
gboolean one_shot            = 0;
gboolean preview_rectangle   = 0;

// Cairo vars
cairo_t * c_state;
cairo_surface_t * c_surface;

// Vars
int window_width  = 0;
int window_height = 0;

GOptionEntry option_entries[] = {
    {
        .long_name   = "short",
        .arg         = G_OPTION_ARG_NONE,
        .arg_data    = &short_format,
        .description = "Use #RRGGBB output format",
    },
    {
        .long_name   = "one-shot",
        .arg         = G_OPTION_ARG_NONE,
        .arg_data    = &one_shot,
        .description = "Exit after picking one color",
    },
    {
        .long_name   = "preview",
        .arg         = G_OPTION_ARG_NONE,
        .arg_data    = &preview_rectangle,
        .description = "Show the current color in preview rectangle",
    },
    { }
};

static void
print_color_at(GdkDrawable * drawable, gint x, gint y)
{
    // Extract the color from the pixel
    GdkImage * pixel = gdk_drawable_get_image(drawable, x, y, 1, 1);
    guint32 color = gdk_image_get_pixel(pixel, 0, 0);

    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    char *color_string = (char*)malloc(13 * sizeof(char));
    sprintf(color_string, "#%06X", color);

    if (short_format) {
        printf("#%06X\n", color);
    } else {
        printf("R: %3d, G: %3d, B: %3d | Hex: #%06X\n",
          (color >> 0x10) & 0xFF,
          (color >> 0x08) & 0xFF,
          (color >> 0x00) & 0xFF,
          color);
      gtk_clipboard_set_text(clipboard, color_string, -1);
    }

    gdk_image_destroy(pixel);
}

static GdkFilterReturn
region_filter_func(GdkXEvent * xevent, GdkEvent * event,
                   GdkWindow * g_root_window)
{
    XEvent * _xevent = (XEvent *) xevent;
    gint x, y;

    if (_xevent->type != ButtonPress) {
        return GDK_FILTER_CONTINUE;
    }

    switch (_xevent->xbutton.button) {
        case Button1:
            x = _xevent->xbutton.x_root;
            y = _xevent->xbutton.y_root;

            /* Print color */
            print_color_at(GDK_DRAWABLE(g_root_window), x, y);
            if (one_shot) {
                gtk_main_quit();
                return GDK_FILTER_REMOVE;
            } else {
                return GDK_FILTER_CONTINUE;
            }

        default:
            gtk_main_quit();
            return GDK_FILTER_REMOVE;
    }

    return GDK_FILTER_CONTINUE;
}

void
allow_input_passthrough(Window w)
{
    XserverRegion region = XFixesCreateRegion(x_display, NULL, 0);

    XFixesSetWindowShapeRegion(x_display, w, ShapeBounding, 0, 0, 0);
    XFixesSetWindowShapeRegion(x_display, w, ShapeInput, 0, 0, region);

    XFixesDestroyRegion(x_display, region);
}

void
preview_rectangle_redraw(double red, double green, double blue)
{
    int size = 50;
    int x    = 0;
    int y    = window_height - size;

    // Just draw the rectangle on the left bottom corner
    cairo_set_source_rgb(c_state, red, green, blue);
    cairo_rectangle(c_state, x, y, size, size);
    cairo_fill(c_state);
}

gint
preview_rectangle_refresh()
{
    // Mouse position
    int x, y;

    // Get the pixel for the current mouse position
    gdk_window_get_pointer(g_root_window, &x, &y, NULL);

    GdkImage * pixel = gdk_drawable_get_image(GDK_DRAWABLE(g_root_window),
                                              x, y, 1, 1);
    guint32 color    = gdk_image_get_pixel(pixel, 0, 0);

    // Get the current overlay window
    x_overlay_window = XCompositeGetOverlayWindow(x_display, x_root_window);

    // Redraw the preview rectangle with the current pixel color
    preview_rectangle_redraw((double) ((color >> 0x10) & 0xFF) / 255,
                             (double) ((color >> 0x08) & 0xFF) / 255,
                             (double) ((color >> 0x00) & 0xFF) / 255);

    // Release the overlay window
    XCompositeReleaseOverlayWindow(x_display, x_root_window);

    // We need to return >0 for gtk to keep the timer alive
    return 1;
}

void
color_grab_init(int argc, char * argv[])
{
    gtk_init_with_args(&argc, &argv, NULL, option_entries, NULL, NULL);

    g_root_window = gdk_get_default_root_window();
    g_cursor      = gdk_cursor_new(GDK_TCROSS);

    gdk_pointer_grab(g_root_window, FALSE, GDK_BUTTON_PRESS_MASK,
                     NULL, g_cursor, GDK_CURRENT_TIME);
    gdk_window_add_filter(g_root_window,
                          (GdkFilterFunc) region_filter_func,
                          g_root_window);
    gdk_flush();
}

void
color_grab_cleanup(void)
{
    // Cleanup the GTK stuff
    gdk_window_remove_filter(g_root_window,
                             (GdkFilterFunc) region_filter_func,
                             g_root_window);
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
    gdk_cursor_destroy(g_cursor);
}

void
preview_rectangle_init(void)
{
    if (!preview_rectangle) {
        return;
    }

    x_display     = XOpenDisplay(NULL);
    int s         = DefaultScreen(x_display);
    x_root_window = RootWindow(x_display, s);

    XCompositeRedirectSubwindows(x_display, x_root_window,
                                 CompositeRedirectAutomatic);
    XSelectInput(x_display, x_root_window, SubstructureNotifyMask);

    window_width     = DisplayWidth(x_display, s);
    window_height    = DisplayHeight(x_display, s);
    x_overlay_window = XCompositeGetOverlayWindow(x_display, x_root_window);

    allow_input_passthrough(x_overlay_window);

    c_surface = cairo_xlib_surface_create(x_display, x_overlay_window,
                                          DefaultVisual(x_display, s),
                                          window_width, window_height);
    c_state   = cairo_create(c_surface);

    XSelectInput(x_display, x_overlay_window, ExposureMask);
    preview_rectangle_redraw(0.0, 0.0, 0.0);

    // Redraw the rectangle every 5ms
    preview_rectangle_timer = gtk_timeout_add(5, preview_rectangle_refresh,
                                              NULL);
}

void
preview_rectangle_cleanup(void)
{
    if (!preview_rectangle) {
        return;
    }

    // Stop the redrawing of the preview rectangle
    gtk_timeout_remove(preview_rectangle_timer);

    // Free the X11 and Cairo resources
    cairo_destroy(c_state);
    cairo_surface_destroy(c_surface);
    XCloseDisplay(x_display);
}

int
main(int argc, char * argv[])
{
    // Init the color grabbing and the preview rectangle
    color_grab_init(argc, argv);
    preview_rectangle_init();

    // Run the gtk main loop
    gtk_main();

    // Clean up our resources
    preview_rectangle_cleanup();
    color_grab_cleanup();

    return 0;
}
