#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "chatfuncs.h"
#include "callbacks.h"
#include "interface.h"
#include "support.h"

gint
get_from_remote(gpointer data)
{
	int		ch;
	gchar	*outstr;
	gchar	instr[2];
	gsize		inbytes;
	gsize		outbytes;

	instr[1]=0;
	switch(chat_check_remote()) {
		case 2:		/* Chat active */
			while((ch=chat_read_byte())) {
				if(ch==-1) {
					chat_close();
					gtk_main_quit();
					return(FALSE);
				}
				if(ch==8 || ch==127) {
					GtkTextIter		start;
					GtkTextIter		end;

					gtk_text_buffer_get_iter_at_mark(
							 gtk_text_view_get_buffer(GTK_TEXT_VIEW(data))
							,&start
							,gtk_text_buffer_get_insert(
									gtk_text_view_get_buffer(GTK_TEXT_VIEW(data))
							)
					);
					end=start;
					gtk_text_iter_backward_cursor_position(&end);
					gtk_text_buffer_delete(
							 gtk_text_view_get_buffer(GTK_TEXT_VIEW(data))
							,&start
							,&end
					);
				}
				else {
					instr[0]=ch;
					outstr=g_convert(instr, 1, "UTF-8", "CP437", &inbytes, &outbytes, NULL);
					gtk_text_buffer_insert_at_cursor(
							 gtk_text_view_get_buffer(GTK_TEXT_VIEW(data))
							,outstr
							,1
					);
					g_free(outstr);
				}
			}
			return(TRUE);
	}
	chat_close();
	return(FALSE);
}


gint
connect_wait(gpointer data)
{
	GtkWidget *MainWindow;

	switch(chat_check_remote()) {
		case -1:	/* Error */
		case 0:		/* Remote has gone away */
			chat_close();
			return(FALSE);
		case 1:		/* Waiting for remote */
			return(TRUE);
		case 2:		/* Chat active */
			MainWindow = create_MainWindow ();
			gtk_widget_hide (GTK_WIDGET(data));
			gtk_widget_show (MainWindow);
			return(FALSE);
	}
	return(TRUE);
}


void
on_MainWindow_destroy                  (GtkObject       *object,
                                        gpointer         user_data)
{
	chat_close();
	gtk_main_quit();
}


gboolean
on_LocalText_key_press_event           (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data)
{
	gchar	*outstr;
	gchar	instr[2];
	gsize	inbytes;
	gsize	outbytes;

	if(event->keyval==GDK_BackSpace || event->keyval==GDK_Delete) {
		GtkTextIter		start;
		GtkTextIter		end;

		chat_write_byte('\b');
		gtk_text_buffer_get_iter_at_mark(
				 gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget))
				,&start
				,gtk_text_buffer_get_insert(
						gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget))
				)
		);
		end=start;
		gtk_text_iter_backward_cursor_position(&end);
		gtk_text_buffer_delete(
				 gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget))
				,&start
				,&end
		);
	}
	if(event->keyval >= 32 && event->keyval < 127) {
		instr[1]=0;
		instr[0]=event->keyval;
		chat_write_byte(event->keyval);
		outstr=g_convert(instr, 1, "UTF-8", "CP437", &inbytes, &outbytes, NULL);
		gtk_text_buffer_insert_at_cursor(
				 gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget))
				,outstr
				,1
		);
		g_free(outstr);
	}
	if(event->keyval == GDK_Return || event->keyval == GDK_KP_Enter) {
		instr[1]=0;
		instr[0]='\n';
		chat_write_byte('\r');
		outstr=g_convert(instr, 1, "UTF-8", "CP437", &inbytes, &outbytes, NULL);
		gtk_text_buffer_insert_at_cursor(
				 gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget))
				,outstr
				,1
		);
		g_free(outstr);
	}
	
	return FALSE;
}


void
on_CancelButton_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
	chat_close();
	gtk_main_quit();
}
