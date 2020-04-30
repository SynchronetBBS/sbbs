#include <stdlib.h>
#include <locale.h>

#include <gtk/gtk.h>

#include "sbbs.h"

#include "chatfuncs.h"
#include "interface.h"
#include "support.h"

int lprintf(int level, char *fmt, ...)
{
	/* ToDo: Log output */
	return(0);
}


int
main (int argc, char *argv[])
{
  int		node;
  char		*ctrl_dir;
  GtkWidget *WaitWindow;

  setlocale(LC_ALL, "");
  gtk_init (&argc, &argv);

/*  add_pixmap_directory (PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps"); */

  ctrl_dir = get_ctrl_dir();

  if(argc<2)
    return(1);

  node=atoi(argv[1]);

  if(node<1)
    return(1);

  /*
   * Request a chat
   */
  if(chat_open(node, ctrl_dir))
  	return(1);

  /*
   * Show "waiting" window
   */
  WaitWindow = create_WaitWindow ();
  gtk_widget_show (WaitWindow);

  gtk_main ();
  return 0;
}
