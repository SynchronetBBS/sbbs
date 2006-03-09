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
    gtk_widget_ref (widget), (GDestroyNotify) gtk_widget_unref)

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
  GtkTooltips *tooltips;

  tooltips = gtk_tooltips_new ();

  MainWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (MainWindow, 400, 300);
  gtk_window_set_title (GTK_WINDOW (MainWindow), "Synchronet Sysop Chat");
  gtk_window_set_icon_name (GTK_WINDOW (MainWindow), "stock_help-chat");
  gtk_window_set_type_hint (GTK_WINDOW (MainWindow), GDK_WINDOW_TYPE_HINT_UTILITY);

  SplitPane = gtk_vpaned_new ();
  gtk_widget_show (SplitPane);
  gtk_container_add (GTK_CONTAINER (MainWindow), SplitPane);
  gtk_paned_set_position (GTK_PANED (SplitPane), 148);

  RemoteText = gtk_text_view_new ();
  gtk_widget_show (RemoteText);
  gtk_paned_pack1 (GTK_PANED (SplitPane), RemoteText, FALSE, TRUE);
  GTK_WIDGET_UNSET_FLAGS (RemoteText, GTK_CAN_FOCUS);
  gtk_tooltips_set_tip (tooltips, RemoteText, "Remote Text Window", NULL);
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
  GLADE_HOOKUP_OBJECT_NO_REF (MainWindow, tooltips, "tooltips");

  gtk_widget_grab_focus (LocalText);

  gtk_timeout_add(50, get_from_remote, RemoteText);

  return MainWindow;
}

GtkWidget*
create_WaitWindow (void)
{
  GtkWidget *WaitWindow;
  GtkWidget *fixed1;
  GtkWidget *CancelButton;
  GtkWidget *alignment1;
  GtkWidget *hbox1;
  GtkWidget *image1;
  GtkWidget *label2;

  WaitWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (WaitWindow), "Waiting for user");

  fixed1 = gtk_fixed_new ();
  gtk_widget_show (fixed1);
  gtk_container_add (GTK_CONTAINER (WaitWindow), fixed1);
  gtk_widget_set_size_request (fixed1, 400, 40);

  MessageLabel = gtk_label_new ("Waiting for user to connect.");
  gtk_widget_show (MessageLabel);
  gtk_fixed_put (GTK_FIXED (fixed1), MessageLabel, 0, 0);
  gtk_widget_set_size_request (MessageLabel, 400, 16);

  CancelButton = gtk_button_new ();
  gtk_widget_show (CancelButton);
  gtk_fixed_put (GTK_FIXED (fixed1), CancelButton, 160, 16);
  gtk_widget_set_size_request (CancelButton, 88, 24);

  alignment1 = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_widget_show (alignment1);
  gtk_container_add (GTK_CONTAINER (CancelButton), alignment1);

  hbox1 = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox1);
  gtk_container_add (GTK_CONTAINER (alignment1), hbox1);

  image1 = gtk_image_new_from_stock ("gtk-cancel", GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image1);
  gtk_box_pack_start (GTK_BOX (hbox1), image1, FALSE, FALSE, 0);

  label2 = gtk_label_new_with_mnemonic ("Cancel");
  gtk_widget_show (label2);
  gtk_box_pack_start (GTK_BOX (hbox1), label2, FALSE, FALSE, 0);

  g_signal_connect ((gpointer) CancelButton, "clicked",
                    G_CALLBACK (on_CancelButton_clicked),
                    NULL);

  /* Store pointers to all widgets, for use by lookup_widget(). */
  GLADE_HOOKUP_OBJECT_NO_REF (WaitWindow, WaitWindow, "WaitWindow");
  GLADE_HOOKUP_OBJECT (WaitWindow, fixed1, "fixed1");
  GLADE_HOOKUP_OBJECT (WaitWindow, MessageLabel, "MessageLabel");
  GLADE_HOOKUP_OBJECT (WaitWindow, CancelButton, "CancelButton");
  GLADE_HOOKUP_OBJECT (WaitWindow, alignment1, "alignment1");
  GLADE_HOOKUP_OBJECT (WaitWindow, hbox1, "hbox1");
  GLADE_HOOKUP_OBJECT (WaitWindow, image1, "image1");
  GLADE_HOOKUP_OBJECT (WaitWindow, label2, "label2");

  gtk_timeout_add(50, connect_wait, WaitWindow);
  return WaitWindow;
}

