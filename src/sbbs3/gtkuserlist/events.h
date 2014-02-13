#ifndef _EVENTS_H_
#define _EVENTS_H_

#include <gtk/gtk.h>

void update_userlist_sensitive_callback(GtkTreeSelection *wiggy, gpointer data);
void update_userlist_callback(GtkWidget *wiggy, gpointer data);
void display_message(char *title, char *message, char *icon);

#endif
