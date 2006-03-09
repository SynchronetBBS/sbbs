#include <gtk/gtk.h>


void
on_MainWindow_destroy                  (GtkObject       *object,
                                        gpointer         user_data);

gboolean
on_LocalText_key_press_event           (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);

void
on_CancelButton_clicked                (GtkButton       *button,
                                        gpointer         user_data);

gint
get_from_remote(gpointer data);


gint
connect_wait(gpointer data);
