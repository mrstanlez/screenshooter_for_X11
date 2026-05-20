/*
Program name: Screenshooter for X11 with GUI - GTK
Author: Stanislav Petrek
Date: 19. may. 2026
Download: https://github.com/mrstanlez/screenshooter-for-X11

Compile: gcc main.c -o screenshot_app `pkg-config --cflags --libs gtk+-3.0 gdk-x11-3.0 x11` -lXfixes
Run: ./screenshot_app
*/

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

GtkWidget *radio_full;
GtkWidget *radio_active;
GtkWidget *radio_select;
GtkWidget *spin_delay;
GtkWidget *check_pointer;
GtkWidget *check_border;
GtkWidget *win_app_global;
GtkWidget *entry_path;

GtkWidget *radio_jpg;
GtkWidget *radio_png;
GtkWidget *radio_bmp;
GtkWidget *radio_webp;
GtkWidget *radio_avif;

int start_x = -1, start_y = -1;
int end_x = -1, end_y = -1;
gboolean is_drawing = FALSE;
GdkPixbuf *bg_pixbuf = NULL;

int saved_crop_x = 0;
int saved_crop_y = 0;
int saved_crop_w = 0;
int saved_crop_h = 0;

void save_config() {
    FILE *f = fopen("init.txt", "w");
    if (!f) return;
    fprintf(f, "%s\n", gtk_entry_get_text(GTK_ENTRY(entry_path)));
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_full))) fprintf(f, "full\n");
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_active))) fprintf(f, "active\n");
    else fprintf(f, "select\n");
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_jpg))) fprintf(f, "jpeg\n");
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_png))) fprintf(f, "png\n");
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_bmp))) fprintf(f, "bmp\n");
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_webp))) fprintf(f, "webp\n");
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_avif))) fprintf(f, "avif\n");
    fprintf(f, "%d\n", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_delay)));
    fclose(f);
}

void load_config() {
    FILE *f = fopen("init.txt", "r");
    static char path[512];
    static char region[32];
    static char format[32];
    static char delay_str[32];

    strcpy(region, "active");
    strcpy(format, "jpeg");
    strcpy(delay_str, "0");

    const char *homedir = getenv("HOME");
    if (homedir) {
        snprintf(path, sizeof(path), "%s/Pictures", homedir);
    } else {
        strcpy(path, "/tmp");
    }

    if (f) {
        if (fgets(path, sizeof(path), f)) path[strcspn(path, "\n")] = 0;
        if (fgets(region, sizeof(region), f)) region[strcspn(region, "\n")] = 0;
        if (fgets(format, sizeof(format), f)) format[strcspn(format, "\n")] = 0;
        if (fgets(delay_str, sizeof(delay_str), f)) delay_str[strcspn(delay_str, "\n")] = 0;
        fclose(f);
    }
    gtk_entry_set_text(GTK_ENTRY(entry_path), path);
    if (strcmp(region, "full") == 0) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_full), TRUE);
    else if (strcmp(region, "select") == 0) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_select), TRUE);
    else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_active), TRUE);
    if (strcmp(format, "png") == 0) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_png), TRUE);
    else if (strcmp(format, "bmp") == 0) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_bmp), TRUE);
    else if (strcmp(format, "webp") == 0) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_webp), TRUE);
    else if (strcmp(format, "avif") == 0) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_avif), TRUE);
    else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_jpg), TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_delay), atoi(delay_str));
}

void get_screenshot_filename(char *buffer, int max_len, const char *ext) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    const char *base_path = gtk_entry_get_text(GTK_ENTRY(entry_path));
    char format_str[1024];
    snprintf(format_str, sizeof(format_str), "%s/screenshot_%%Y%%m%%d_%%H%%M%%S.%s", base_path, ext);
    strftime(buffer, max_len, format_str, tm_info);
}

void draw_mouse_pointer(GdkPixbuf *pixbuf, int offset_x, int offset_y) {
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_pointer))) return;
    GdkDisplay *gdk_disp = gdk_display_get_default();
    Display *display = GDK_DISPLAY_XDISPLAY(gdk_disp);
    XFixesCursorImage *cursor = XFixesGetCursorImage(display);
    if (!cursor) return;
    int cursor_x = cursor->x - cursor->xhot - offset_x;
    int cursor_y = cursor->y - cursor->yhot - offset_y;
    int pb_w = gdk_pixbuf_get_width(pixbuf);
    int pb_h = gdk_pixbuf_get_height(pixbuf);
    GdkPixbuf *c_pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, cursor->width, cursor->height);
    if (c_pb) {
        guchar *pixels = gdk_pixbuf_get_pixels(c_pb);
        int stride = gdk_pixbuf_get_rowstride(c_pb);
        for (int y = 0; y < cursor->height; y++) {
            for (int x = 0; x < cursor->width; x++) {
                unsigned long p = cursor->pixels[y * cursor->width + x];
                guchar *dst = pixels + y * stride + x * 4;
                dst[0] = (p >> 16) & 0xFF;
                dst[1] = (p >> 8) & 0xFF;
                dst[2] = p & 0xFF;
                dst[3] = (p >> 24) & 0xFF;
            }
        }
        int render_x = MAX(0, cursor_x);
        int render_y = MAX(0, cursor_y);
        int src_x = render_x - cursor_x;
        int src_y = render_y - cursor_y;
        int render_w = MIN(cursor->width - src_x, pb_w - render_x);
        int render_h = MIN(cursor->height - src_y, pb_h - render_y);
        if (render_w > 0 && render_h > 0) {
            gdk_pixbuf_composite(c_pb, pixbuf, render_x, render_y, render_w, render_h, cursor_x, cursor_y, 1.0, 1.0, GDK_INTERP_BILINEAR, 255);
        }
        g_object_unref(c_pb);
    }
    XFree(cursor);
}

void print_installation_hint(const char *format) {
    g_print("\n==================================================\n");
    g_print("CHYBA: Formát '%s' nie je podporovaný.\n", format);
    g_print("==================================================\n\n");
}

void save_pixbuf_to_file(GdkPixbuf *pixbuf, int ox, int oy) {
    if (!pixbuf) return;
    draw_mouse_pointer(pixbuf, ox, oy);
    const char *format = "jpeg";
    const char *ext = "jpg";
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_png))) { format = "png"; ext = "png"; }
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_bmp))) { format = "bmp"; ext = "bmp"; }
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_webp))) { format = "webp"; ext = "webp"; }
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_avif))) { format = "avif"; ext = "avif"; }
    GdkPixbuf *final_pixbuf = pixbuf;
    if (strcmp(format, "jpeg") == 0 && gdk_pixbuf_get_has_alpha(pixbuf)) {
        int w = gdk_pixbuf_get_width(pixbuf);
        int h = gdk_pixbuf_get_height(pixbuf);
        final_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
        if (final_pixbuf) {
            gdk_pixbuf_fill(final_pixbuf, 0xFFFFFFFF);
            gdk_pixbuf_composite(pixbuf, final_pixbuf, 0, 0, w, h, 0, 0, 1.0, 1.0, GDK_INTERP_BILINEAR, 255);
        } else {
            final_pixbuf = pixbuf;
        }
    }
    char filename[2048];
    get_screenshot_filename(filename, sizeof(filename), ext);
    GError *error = NULL;
    if (strcmp(format, "jpeg") == 0) gdk_pixbuf_save(final_pixbuf, filename, format, &error, "quality", "90", NULL);
    else gdk_pixbuf_save(final_pixbuf, filename, format, &error, NULL);
    if (error) {
        if (error->domain == GDK_PIXBUF_ERROR && (error->code == GDK_PIXBUF_ERROR_UNKNOWN_TYPE || strstr(error->message, "format") != NULL)) print_installation_hint(format);
        else g_print("Chyba: %s\n", error->message);
        g_error_free(error);
    } else g_print("Uložené: %s\n", filename);
    if (final_pixbuf != pixbuf) g_object_unref(final_pixbuf);
    g_object_unref(pixbuf);
    save_config();
}

void capture_full() {
    GdkWindow *root_win = gdk_get_default_root_window();
    int w = gdk_window_get_width(root_win);
    int h = gdk_window_get_height(root_win);
    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_window(root_win, 0, 0, w, h);
    save_pixbuf_to_file(pixbuf, 0, 0);
}

Window get_active_toplevel_window(Display *display) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    Window result = None;
    Atom active_atom = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    Window root = DefaultRootWindow(display);

    if (XGetWindowProperty(display, root, active_atom, 0, 1, False, XA_WINDOW,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
        if (nitems > 0) {
            result = *((Window *)prop);
        }
        XFree(prop);
    }
    return result;
}

void capture_active() {
    GdkDisplay *gdk_disp = gdk_display_get_default();
    Display *display = GDK_DISPLAY_XDISPLAY(gdk_disp);
    Window toplevel = get_active_toplevel_window(display);
    if (toplevel == None) return;
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(display, toplevel, &attrs) || attrs.map_state != IsViewable) return;

    int x = 0, y = 0;
    Window child;
    XTranslateCoordinates(display, toplevel, attrs.root, 0, 0, &x, &y, &child);
    int w = attrs.width;
    int h = attrs.height;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_border))) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = NULL;
        Atom extents_atom = XInternAtom(display, "_NET_FRAME_EXTENTS", True);

        if (extents_atom != None && XGetWindowProperty(display, toplevel, extents_atom, 0, 4, False, AnyPropertyType,
                                    &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
            if (nitems == 4) {
                long *extents = (long *)prop;
                int pad_l = extents[0];
                int pad_r = extents[1];
                int pad_t = extents[2];
                int pad_b = extents[3];
                x += pad_l;
                y += pad_t;
                w -= (pad_l + pad_r);
                h -= (pad_t + pad_b);
            }
            XFree(prop);
        }
    }

    if (w > 0 && h > 0) {
        GdkWindow *root_win = gdk_get_default_root_window();
        GdkPixbuf *pixbuf = gdk_pixbuf_get_from_window(root_win, x, y, w, h);
        save_pixbuf_to_file(pixbuf, x, y);
    }
}

gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.35);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    if (is_drawing && start_x != -1 && start_y != -1) {
        int x = MIN(start_x, end_x);
        int y = MIN(start_y, end_y);
        int w = ABS(end_x - start_x);
        int h = ABS(end_y - start_y);
        if (w > 0 && h > 0) {
            cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
            cairo_rectangle(cr, x, y, w, h);
            cairo_fill(cr);
            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
            cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
            cairo_set_line_width(cr, 2.0);
            cairo_rectangle(cr, x, y, w, h);
            cairo_stroke(cr);
        }
    }
    return FALSE;
}

gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    if (event->button == 1) {
        start_x = event->x; start_y = event->y;
        end_x = event->x; end_y = event->y;
        is_drawing = TRUE;
    }
    return TRUE;
}

gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    if (is_drawing) {
        end_x = event->x; end_y = event->y;
        gtk_widget_queue_draw(widget);
    }
    return TRUE;
}

gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    if (event->button == 1 && is_drawing) {
        is_drawing = FALSE;
        end_x = event->x; end_y = event->y;
        saved_crop_x = MIN(start_x, end_x);
        saved_crop_y = MIN(start_y, end_y);
        saved_crop_w = ABS(end_x - start_x);
        saved_crop_h = ABS(end_y - start_y);
        gtk_widget_destroy(widget);
        while (gtk_events_pending()) gtk_main_iteration();
        if (bg_pixbuf) { g_object_unref(bg_pixbuf); bg_pixbuf = NULL; }
        if (saved_crop_w > 5 && saved_crop_h > 5) {
            int delay = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_delay));
            if (delay > 0) g_usleep(delay * 1000000);
            else g_usleep(200000);
            GdkWindow *root_win = gdk_get_default_root_window();
            GdkPixbuf *pixbuf = gdk_pixbuf_get_from_window(root_win, saved_crop_x, saved_crop_y, saved_crop_w, saved_crop_h);
            save_pixbuf_to_file(pixbuf, saved_crop_x, saved_crop_y);
        }
        gtk_widget_show(win_app_global);
    }
    return TRUE;
}

void capture_select() {
    GdkWindow *root_win = gdk_get_default_root_window();
    int sw = gdk_window_get_width(root_win);
    int sh = gdk_window_get_height(root_win);
    bg_pixbuf = gdk_pixbuf_get_from_window(root_win, 0, 0, sw, sh);
    GtkWidget *overlay = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_app_paintable(overlay, TRUE);
    GdkScreen *screen = gtk_widget_get_screen(overlay);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual) gtk_widget_set_visual(overlay, visual);
    gtk_window_set_decorated(GTK_WINDOW(overlay), FALSE);
    gtk_window_fullscreen(GTK_WINDOW(overlay));
    gtk_window_set_keep_above(GTK_WINDOW(overlay), TRUE);
    gtk_widget_add_events(overlay, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    g_signal_connect(overlay, "draw", G_CALLBACK(on_draw_event), NULL);
    g_signal_connect(overlay, "button-press-event", G_CALLBACK(on_button_press), NULL);
    g_signal_connect(overlay, "motion-notify-event", G_CALLBACK(on_motion_notify), NULL);
    g_signal_connect(overlay, "button-release-event", G_CALLBACK(on_button_release), NULL);
    gtk_widget_show_all(overlay);
    GdkDisplay *display = gdk_display_get_default();
    GdkSeat *seat = gdk_display_get_default_seat(display);
    gdk_seat_grab(seat, gtk_widget_get_window(overlay), GDK_SEAT_CAPABILITY_POINTER, FALSE, NULL, NULL, NULL, NULL);
}

void on_ok_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *main_window = GTK_WIDGET(data);
    int delay = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_delay));
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_select))) {
        gtk_widget_hide(main_window);
        while (gtk_events_pending()) gtk_main_iteration();
        g_usleep(200000);
        capture_select();
    } else {
        gtk_widget_hide(main_window);
        while (gtk_events_pending()) gtk_main_iteration();
        g_usleep(300000);
        if (delay > 0) g_usleep(delay * 1000000);
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_full))) capture_full();
        else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_active))) capture_active();
        gtk_widget_show(main_window);
    }
}

void on_help_clicked(GtkWidget *widget, gpointer data) {
    GtkWindow *parent = GTK_WINDOW(data);
    GtkWidget *dialog = gtk_message_dialog_new(parent,
                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                 GTK_MESSAGE_INFO,
                                 GTK_BUTTONS_OK,
                                 NULL);
    
    gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dialog),
        "<b>Program name:</b> Screenshooter for X11\n"
        "<b>Author:</b> Stanislav Petrek\n"
        "<b>Date:</b> 19. may. 2026\n"
        "<b>Download:</b> <a href=\"https://github.com/mrstanlez/screenshooter_for_X11\">GitHub Repository</a>");

    gtk_window_set_title(GTK_WINDOW(dialog), "Help / About");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    win_app_global = window;
    gtk_window_set_title(GTK_WINDOW(window), "Screenshooter - for X11");
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(window), main_vbox);
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 40);
    gtk_box_pack_start(GTK_BOX(main_vbox), grid, TRUE, TRUE, 0);
    GtkWidget *vbox_left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *lbl_region = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_region), "<b>Region to capture</b>");
    gtk_label_set_xalign(GTK_LABEL(lbl_region), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox_left), lbl_region, FALSE, FALSE, 0);
    radio_full = gtk_radio_button_new_with_label(NULL, "Entire screen");
    radio_active = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_full), "Active window");
    radio_select = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_full), "Select a region");
    gtk_box_pack_start(GTK_BOX(vbox_left), radio_full, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_left), radio_active, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_left), radio_select, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), vbox_left, 0, 0, 1, 1);
    GtkWidget *vbox_right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *lbl_delay = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_delay), "<b>Delay before capturing</b>");
    gtk_label_set_xalign(GTK_LABEL(lbl_delay), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox_right), lbl_delay, FALSE, FALSE, 0);
    GtkWidget *hbox_delay = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkAdjustment *adj = gtk_adjustment_new(0, 0, 60, 1, 5, 0);
    spin_delay = gtk_spin_button_new(adj, 1, 0);
    GtkWidget *lbl_sec = gtk_label_new("seconds");
    gtk_box_pack_start(GTK_BOX(hbox_delay), spin_delay, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_delay), lbl_sec, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_right), hbox_delay, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), vbox_right, 1, 0, 1, 1);
    GtkWidget *vbox_formats = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *lbl_formats = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_formats), "<b>Save to format</b>");
    gtk_label_set_xalign(GTK_LABEL(lbl_formats), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox_formats), lbl_formats, FALSE, FALSE, 0);
    GtkWidget *hbox_formats_rb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    radio_jpg = gtk_radio_button_new_with_label(NULL, "jpg");
    radio_png = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_jpg), "png");
    radio_bmp = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_jpg), "bmp");
    radio_webp = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_jpg), "webp");
    radio_avif = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_jpg), "avif");
    gtk_box_pack_start(GTK_BOX(hbox_formats_rb), radio_jpg, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_formats_rb), radio_png, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_formats_rb), radio_bmp, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_formats_rb), radio_webp, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_formats_rb), radio_avif, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_formats), hbox_formats_rb, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), vbox_formats, FALSE, FALSE, 0);
    GtkWidget *vbox_options = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *lbl_opts = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_opts), "<b>Options</b>");
    gtk_label_set_xalign(GTK_LABEL(lbl_opts), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox_options), lbl_opts, FALSE, FALSE, 0);
    check_pointer = gtk_check_button_new_with_label("Capture the mouse pointer");
    check_border = gtk_check_button_new_with_label("Capture the window border");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_border), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_options), check_pointer, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_options), check_border, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), vbox_options, FALSE, FALSE, 0);
    GtkWidget *hbox_path = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *lbl_path = gtk_label_new("Save to path: ");
    entry_path = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox_path), lbl_path, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_path), entry_path, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), hbox_path, FALSE, FALSE, 0);
    GtkWidget *bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 6);
    gtk_box_pack_end(GTK_BOX(main_vbox), bbox, FALSE, FALSE, 0);
    GtkWidget *btn_help = gtk_button_new_with_label("Help");
    GtkWidget *btn_canc = gtk_button_new_with_label("Close");
    GtkWidget *btn_ok = gtk_button_new_with_label("OK");
    g_signal_connect(btn_help, "clicked", G_CALLBACK(on_help_clicked), window);
    g_signal_connect_swapped(btn_canc, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(btn_ok, "clicked", G_CALLBACK(on_ok_clicked), window);
    gtk_container_add(GTK_CONTAINER(bbox), btn_help);
    gtk_container_add(GTK_CONTAINER(bbox), btn_canc);
    gtk_container_add(GTK_CONTAINER(bbox), btn_ok);
    load_config();
    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}


