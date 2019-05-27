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

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr,
                              gpointer user_data)

{

    cairo_text_extents_t ext;

    cairo_set_font_size(cr, 50);

    cairo_text_extents(cr, "Hello, world", &ext);
    cairo_move_to(cr, gtk_widget_get_allocated_width(widget)/2-ext.width/2,
                    gtk_widget_get_allocated_height(widget)/2+ext.height/2);
    cairo_show_text(cr, "Hello, world");

    return FALSE;

}

void destroy( GtkWidget *widget,
              gpointer   data )
{
    gtk_main_quit();
}

int main(int argc, char *argv[])

{

    GtkWidget *window;
    GtkWidget *darea;

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    darea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), darea);

    gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);

    g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), NULL);
    g_signal_connect (G_OBJECT(window), "destroy", G_CALLBACK (destroy), NULL);

    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request(GTK_WIDGET(window), 170, 0);
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 400);
    gtk_window_set_title(GTK_WINDOW(window), "Hello, world");

    gtk_widget_show_all(window);

    gtk_main();

    return 0;

}
