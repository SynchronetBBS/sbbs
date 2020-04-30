#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

#define GLADE_HOOKUP_OBJECT(component,widget,name) \
  g_object_set_data_full (G_OBJECT (component), name, \
    g_object_ref (widget), (GDestroyNotify) g_object_unref)

#define GLADE_HOOKUP_OBJECT_NO_REF(component,widget,name) \
  g_object_set_data (G_OBJECT (component), name, widget)

GtkWidget *MessageLabel;

GtkWidget*
create_MainWindow (void)
{
  GtkWidget *MainWindow;
  GtkWidget *SplitPane;
  GtkWidget *RemoteText;
  GtkWidget *LocalText;

  MainWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (MainWindow, 400, 300);
  gtk_window_set_title (GTK_WINDOW (MainWindow), "Synchronet Sysop Chat");
  gtk_window_set_icon_name (GTK_WINDOW (MainWindow), "user-available");

  SplitPane = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_widget_show (SplitPane);
  gtk_container_add (GTK_CONTAINER (MainWindow), SplitPane);
  gtk_paned_set_position (GTK_PANED (SplitPane), 148);

  RemoteText = gtk_text_view_new ();
  gtk_widget_show (RemoteText);
  gtk_paned_pack1 (GTK_PANED (SplitPane), RemoteText, FALSE, TRUE);
  gtk_widget_set_can_focus (RemoteText, FALSE);
  gtk_widget_set_tooltip_text (RemoteText, "Remote Text Window");
  gtk_text_view_set_editable (GTK_TEXT_VIEW (RemoteText), FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (RemoteText), GTK_WRAP_WORD);

  LocalText = gtk_text_view_new ();
  gtk_widget_show (LocalText);
  gtk_paned_pack2 (GTK_PANED (SplitPane), LocalText, TRUE, TRUE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (LocalText), GTK_WRAP_WORD);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (LocalText), FALSE);

  g_signal_connect ((gpointer) MainWindow, "destroy",
                    G_CALLBACK (on_MainWindow_destroy),
                    NULL);
  g_signal_connect ((gpointer) LocalText, "key_press_event",
                    G_CALLBACK (on_LocalText_key_press_event),
                    NULL);

  /* Store pointers to all widgets, for use by lookup_widget(). */
  GLADE_HOOKUP_OBJECT_NO_REF (MainWindow, MainWindow, "MainWindow");
  GLADE_HOOKUP_OBJECT (MainWindow, SplitPane, "SplitPane");
  GLADE_HOOKUP_OBJECT (MainWindow, RemoteText, "RemoteText");
  GLADE_HOOKUP_OBJECT (MainWindow, LocalText, "LocalText");

  gtk_widget_grab_focus (LocalText);

  g_timeout_add(50, get_from_remote, RemoteText);

  return MainWindow;
}

GtkWidget*
create_WaitWindow (void)
{
  GtkWidget *WaitWindow;
  GtkWidget *fixed1;
  GtkWidget *CancelButton;

  WaitWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (WaitWindow), "Synchronet Sysop Chat");
  gtk_window_set_icon_name (GTK_WINDOW (WaitWindow), "user-away");

  fixed1 = gtk_fixed_new ();
  gtk_widget_show (fixed1);
  gtk_container_add (GTK_CONTAINER (WaitWindow), fixed1);
  gtk_widget_set_size_request (fixed1, 400, 40);

  MessageLabel = gtk_label_new ("Waiting for user to connect.");
  gtk_widget_show (MessageLabel);
  gtk_fixed_put (GTK_FIXED (fixed1), MessageLabel, 0, 0);
  gtk_widget_set_size_request (MessageLabel, 400, 16);

  CancelButton = gtk_widget_new (GTK_TYPE_BUTTON, "label", "_Cancel", "use-underline", TRUE, "xalign", 0.5, "yalign", 0.5, NULL);
  gtk_widget_show (CancelButton);
  gtk_fixed_put (GTK_FIXED (fixed1), CancelButton, 160, 16);
  gtk_widget_set_size_request (CancelButton, 88, 24);

  g_signal_connect ((gpointer) CancelButton, "clicked",
                    G_CALLBACK (on_CancelButton_clicked),
                    NULL);

  /* Store pointers to all widgets, for use by lookup_widget(). */
  GLADE_HOOKUP_OBJECT_NO_REF (WaitWindow, WaitWindow, "WaitWindow");
  GLADE_HOOKUP_OBJECT (WaitWindow, fixed1, "fixed1");
  GLADE_HOOKUP_OBJECT (WaitWindow, MessageLabel, "MessageLabel");
  GLADE_HOOKUP_OBJECT (WaitWindow, CancelButton, "CancelButton");

  g_timeout_add(50, connect_wait, WaitWindow);
  return WaitWindow;
}

