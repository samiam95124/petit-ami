/*******************************************************************************
*                                                                              *
*                      GTK 3.0 HELLO WORLD DEMO PROGRAM                        *
*                                                                              *
* Demonstrates an unbuffered GTK program, that is, one that draws directly to  *
* the screen.                                                                  *
*                                                                              *
*******************************************************************************/

#include <cairo.h>
#include <gtk/gtk.h>

static cairo_surface_t *surf = NULL;

static gboolean draw_event(GtkWidget *widget, cairo_t *cr,
                              gpointer user_data)

{

    printf("draw event\n");

    cairo_save(cr);
    cairo_set_source_surface(cr, surf, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);

    return FALSE;

}

static gint configure_event (GtkWidget *widget, GdkEventConfigure *event)

{

    cairo_t *cr;
    cairo_text_extents_t ext;
    int x, y;

    printf ("configure_event\n");

    if (surf) cairo_surface_destroy(surf);
    x = gtk_widget_get_allocated_width(widget);
    y = gtk_widget_get_allocated_height(widget);
    /* create new buffer to match window */
    surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, x, y);
    cr = cairo_create(surf);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_rectangle(cr, 0, 0, x, y);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_font_size(cr, 50);
    cairo_text_extents(cr, "Hello, world", &ext);
    cairo_move_to(cr, x/2-ext.width/2, y/2+ext.height/2);
    cairo_show_text(cr, "Hello, world");
    cairo_destroy(cr);

    return TRUE;
}

static void destroy_event( GtkWidget *widget,
              gpointer   data )
{
    gtk_main_quit();
}

int main(int argc, char *argv[])

{

    GtkWidget *window;
    GtkWidget *darea;
    cairo_t *cr;
    cairo_text_extents_t ext;

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    darea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), darea);

    gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);

    g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(draw_event), surf);
    g_signal_connect (G_OBJECT(window), "destroy", G_CALLBACK(destroy_event), NULL);
    g_signal_connect (G_OBJECT(darea),"configure_event", G_CALLBACK(configure_event), NULL);

    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request(GTK_WIDGET(window), 170, 0);
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 400);
    gtk_window_set_title(GTK_WINDOW(window), "Hello, world");

    gtk_widget_show_all(window);

    gtk_main();

    return 0;

}
