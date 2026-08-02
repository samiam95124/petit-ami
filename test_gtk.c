/*******************************************************************************
*                                                                              *
*                          WIDGET SAMPLER, GTK VERSION                         *
*                                                                              *
* The companion to test.c: the same assortment of widgets, in the same layout, *
* built with GTK instead of Petit-Ami. Run the two side by side to compare the *
* widget theme against the desktop's own, which is the point of css2theme.     *
*                                                                              *
*     bin/css2theme          # convert the desktop theme for Petit-Ami         *
*     make testg test_gtk                                                      *
*     ./testg & ./test_gtk &                                                   *
*                                                                              *
* Both programs show every widget in the states a theme colors differently:    *
* enabled and disabled, selected and not, and hovered when the mouse is over   *
* them. The - and + buttons move the progress bar.                             *
*                                                                              *
* There is also a menu bar with one menu, File, holding one item, Open,        *
* which raises the GTK open file dialog and reports the name it returns.       *
* That is the reference for the other half of the comparison: putting up a     *
* menu, raising the file dialog, and getting a file name back out of it.       *
*                                                                              *
* This program is not part of Petit-Ami and is not built by "make all": it     *
* links GTK, which Petit-Ami deliberately does not. It exists only as the      *
* reference to compare against.                                                *
*                                                                              *
* Build:                                                                       *
*                                                                              *
*     make test_gtk                                                            *
*                                                                              *
* or by hand:                                                                  *
*                                                                              *
*     gcc test_gtk.c `pkg-config --cflags --libs gtk+-3.0` -o test_gtk         *
*                                                                              *
*******************************************************************************/

#include <gtk/gtk.h>

#define TITLE "GTK widget sampler"

static GtkWidget* progbar; /* progress bar, moved by the - and + buttons */
static double     prog;    /* progress bar position, 0 to 1 */

/*******************************************************************************

Move the progress bar

Advances or retards the progress bar so its active colors can be seen. The
button that was pressed carries the direction.

*******************************************************************************/

static void movprog(GtkWidget* w, gpointer data)

{

    prog += GPOINTER_TO_INT(data)*0.1;
    if (prog > 1.0) prog = 1.0;
    if (prog < 0.0) prog = 0.0;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), prog);

}

/*******************************************************************************

Open a file

Raises the GTK open file dialog from the File menu, and reports the name
it gives back. The dialog runs modally: gtk_dialog_run keeps the
interface alive while it waits, and returns which button ended it.

*******************************************************************************/

static GtkWidget* filerep; /* where the chosen name is reported */

static void openfile(GtkWidget* w, gpointer data)

{

    GtkWidget* dialog;
    gint       res;

    dialog = gtk_file_chooser_dialog_new("Open File", GTK_WINDOW(data),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Open", GTK_RESPONSE_ACCEPT,
                                         NULL);
    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {

        char* name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char  line[1024];

        g_snprintf(line, sizeof(line), "Open: %s", name);
        gtk_label_set_text(GTK_LABEL(filerep), line);
        g_print("%s\n", line);
        g_free(name);

    } else {

        gtk_label_set_text(GTK_LABEL(filerep), "Open: cancelled");
        g_print("Open: cancelled\n");

    }
    gtk_widget_destroy(dialog);

}

/*******************************************************************************

Quit the program

*******************************************************************************/

static void quit(GtkWidget* w, gpointer data)

{

    gtk_main_quit();

}

/*******************************************************************************

Make a section label

Returns a left aligned label, used as the heading over a group of widgets.

*******************************************************************************/

static GtkWidget* label(const char* s)

{

    GtkWidget* l;

    l = gtk_label_new(s);
    gtk_widget_set_halign(l, GTK_ALIGN_START);
    gtk_widget_set_margin_top(l, 10);

    return (l);

}

int main(int argc, char* argv[])

{

    GtkWidget* win;      /* top window */
    GtkWidget* outer;    /* button row above the columns */
    GtkWidget* cols;     /* the three columns */
    GtkWidget* buts;     /* button row */
    GtkWidget* cola;     /* first column */
    GtkWidget* colb;     /* second column */
    GtkWidget* colc;     /* third column */
    GtkWidget* row;      /* a row within a column */
    GtkWidget* w;        /* widget being built */
    GtkWidget* frame;    /* group box */
    GtkWidget* scroll;   /* list box scroller */
    GSList*    grp;      /* radio button group */

    gtk_init(&argc, &argv);

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), TITLE);
    gtk_window_set_default_size(GTK_WINDOW(win), 1120, 725);
    gtk_container_set_border_width(GTK_CONTAINER(win), 15);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_add(GTK_CONTAINER(win), outer);

    /* the menu: one menu, File, with one item, Open, which raises the
       file dialog */
    {

        GtkWidget* menubar = gtk_menu_bar_new();
        GtkWidget* filemenu = gtk_menu_new();
        GtkWidget* fileitem = gtk_menu_item_new_with_mnemonic("_File");
        GtkWidget* openitem = gtk_menu_item_new_with_mnemonic("_Open");

        gtk_menu_item_set_submenu(GTK_MENU_ITEM(fileitem), filemenu);
        gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), openitem);
        gtk_menu_shell_append(GTK_MENU_SHELL(menubar), fileitem);
        g_signal_connect(openitem, "activate", G_CALLBACK(openfile), win);
        gtk_box_pack_start(GTK_BOX(outer), menubar, FALSE, FALSE, 0);
        filerep = label("File > Open raises the GTK file dialog");
        gtk_box_pack_start(GTK_BOX(outer), filerep, FALSE, FALSE, 0);

    }

    /* buttons: normal, held selected, disabled, and the quit button. The
       selected button is a toggle held down, which is how GTK shows a button
       with a persistent active state */
    gtk_box_pack_start(GTK_BOX(outer),
                       label("Buttons: normal, selected, disabled"),
                       FALSE, FALSE, 0);
    buts = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(outer), buts, FALSE, FALSE, 0);

    w = gtk_button_new_with_label("Normal");
    gtk_box_pack_start(GTK_BOX(buts), w, FALSE, FALSE, 0);

    w = gtk_toggle_button_new_with_label("Selected");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
    gtk_box_pack_start(GTK_BOX(buts), w, FALSE, FALSE, 0);

    w = gtk_button_new_with_label("Disabled");
    gtk_widget_set_sensitive(w, FALSE);
    gtk_box_pack_start(GTK_BOX(buts), w, FALSE, FALSE, 0);

    w = gtk_button_new_with_label("Quit");
    g_signal_connect(w, "clicked", G_CALLBACK(quit), NULL);
    gtk_box_pack_start(GTK_BOX(buts), w, FALSE, FALSE, 0);

    /* the three columns below the button row */
    cols = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 25);
    gtk_box_pack_start(GTK_BOX(outer), cols, TRUE, TRUE, 0);
    cola = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    colb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    colc = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(cols), cola, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cols), colb, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cols), colc, FALSE, FALSE, 0);

    /* first column: checkboxes, text entry, progress bar */
    gtk_box_pack_start(GTK_BOX(cola), label("Checkboxes"), FALSE, FALSE, 0);
    w = gtk_check_button_new_with_label("Checked");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
    gtk_box_pack_start(GTK_BOX(cola), w, FALSE, FALSE, 0);
    w = gtk_check_button_new_with_label("Clear");
    gtk_box_pack_start(GTK_BOX(cola), w, FALSE, FALSE, 0);
    w = gtk_check_button_new_with_label("Disabled");
    gtk_widget_set_sensitive(w, FALSE);
    gtk_box_pack_start(GTK_BOX(cola), w, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(cola), label("Edit box, number select box"),
                       FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(cola), row, FALSE, FALSE, 0);
    w = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(w), "Editable text");
    gtk_box_pack_start(GTK_BOX(row), w, FALSE, FALSE, 0);
    w = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_box_pack_start(GTK_BOX(row), w, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(cola), label("Progress bar (- and + move it)"),
                       FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(cola), row, FALSE, FALSE, 0);
    prog = 0.4; /* start part way along, as the Petit-Ami sampler does */
    progbar = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), prog);
    gtk_widget_set_valign(progbar, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(progbar, 200, -1);
    gtk_box_pack_start(GTK_BOX(row), progbar, FALSE, FALSE, 0);
    w = gtk_button_new_with_label(" - ");
    g_signal_connect(w, "clicked", G_CALLBACK(movprog), GINT_TO_POINTER(-1));
    gtk_box_pack_start(GTK_BOX(row), w, FALSE, FALSE, 0);
    w = gtk_button_new_with_label(" + ");
    g_signal_connect(w, "clicked", G_CALLBACK(movprog), GINT_TO_POINTER(1));
    gtk_box_pack_start(GTK_BOX(row), w, FALSE, FALSE, 0);

    /* second column: scroll bar, slider, drop box, tab bar */
    gtk_box_pack_start(GTK_BOX(colb), label("Scroll bar and slider"),
                       FALSE, FALSE, 0);
    w = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, NULL);
    gtk_range_set_range(GTK_RANGE(w), 0, 100);
    gtk_range_set_value(GTK_RANGE(w), 25);
    gtk_widget_set_size_request(w, 300, -1);
    gtk_box_pack_start(GTK_BOX(colb), w, FALSE, FALSE, 0);
    w = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE(w), FALSE);
    gtk_range_set_value(GTK_RANGE(w), 10);
    gtk_widget_set_size_request(w, 300, -1);
    gtk_box_pack_start(GTK_BOX(colb), w, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(colb), label("Drop box"), FALSE, FALSE, 0);
    w = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w), "First");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w), "Second");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w), "Third");
    gtk_combo_box_set_active(GTK_COMBO_BOX(w), 0);
    gtk_widget_set_halign(w, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(colb), w, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(colb), label("Tab bar"), FALSE, FALSE, 0);
    w = gtk_notebook_new();
    gtk_notebook_append_page(GTK_NOTEBOOK(w), gtk_label_new(""),
                             gtk_label_new("One"));
    gtk_notebook_append_page(GTK_NOTEBOOK(w), gtk_label_new(""),
                             gtk_label_new("Two"));
    gtk_notebook_append_page(GTK_NOTEBOOK(w), gtk_label_new(""),
                             gtk_label_new("Three"));
    gtk_widget_set_size_request(w, 300, 100);
    gtk_box_pack_start(GTK_BOX(colb), w, FALSE, FALSE, 0);

    /* third column: radio buttons, group, list box */
    gtk_box_pack_start(GTK_BOX(colc), label("Radio buttons"), FALSE, FALSE, 0);
    w = gtk_radio_button_new_with_label(NULL, "Station 1");
    grp = gtk_radio_button_get_group(GTK_RADIO_BUTTON(w));
    gtk_box_pack_start(GTK_BOX(colc), w, FALSE, FALSE, 0);
    w = gtk_radio_button_new_with_label(grp, "Station 2");
    grp = gtk_radio_button_get_group(GTK_RADIO_BUTTON(w));
    gtk_box_pack_start(GTK_BOX(colc), w, FALSE, FALSE, 0);
    w = gtk_radio_button_new_with_label(grp, "Station 3");
    gtk_box_pack_start(GTK_BOX(colc), w, FALSE, FALSE, 0);

    frame = gtk_frame_new("Group");
    gtk_widget_set_size_request(frame, 160, 80);
    gtk_widget_set_margin_top(frame, 10);
    gtk_box_pack_start(GTK_BOX(colc), frame, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(colc), label("List box"), FALSE, FALSE, 0);
    w = gtk_list_box_new();
    gtk_list_box_insert(GTK_LIST_BOX(w), gtk_label_new("Red"), -1);
    gtk_list_box_insert(GTK_LIST_BOX(w), gtk_label_new("Green"), -1);
    gtk_list_box_insert(GTK_LIST_BOX(w), gtk_label_new("Blue"), -1);
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, 110, 120);
    gtk_container_add(GTK_CONTAINER(scroll), w);
    gtk_box_pack_start(GTK_BOX(colc), scroll, FALSE, FALSE, 0);

    gtk_widget_show_all(win);
    gtk_main();

    return (0);

}
