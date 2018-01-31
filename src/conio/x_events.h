#ifndef _X_EVENTS_H_
#define _X_EVENTS_H_

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

struct update_rect {
	int	x;
	int	y;
	int	width;
	int	height;
	uint32_t	*data;
};

enum x11_local_events {
	 X11_LOCAL_SETMODE
	,X11_LOCAL_SETNAME
	,X11_LOCAL_SETTITLE
	,X11_LOCAL_COPY
	,X11_LOCAL_PASTE
	,X11_LOCAL_DRAWRECT
	,X11_LOCAL_FLUSH
	,X11_LOCAL_BEEP
	,X11_LOCAL_SETPALETTE
};

struct x11_palette_entry {
	uint32_t	index;
	uint16_t	r;
	uint16_t	g;
	uint16_t	b;
};

struct x11_local_event {
	enum x11_local_events	type;
	union {
		int		mode;
		char	name[81];
		char	title[81];
		struct	update_rect rect;
		struct	x11_palette_entry palette;
	} data;
};

/* X functions */
struct x11 {
	int		(*XChangeGC)	(Display*, GC, unsigned long, XGCValues*);
	int		(*XCopyPlane)	(Display*, Drawable, Drawable, GC, int, int, unsigned int, unsigned int, int, int, unsigned long);
	int		(*XFillRectangle)	(Display*, Drawable, GC, int, int, unsigned int, unsigned int);
	int		(*XDrawPoint)	(Display*, Drawable, GC, int, int);
	int		(*XFlush)		(Display*);
	int		(*XSync)		(Display*, Bool);
	int		(*XBell)		(Display*, int);
	int		(*XLookupString)(XKeyEvent*, char*, int, KeySym*, XComposeStatus*);
	int		(*XNextEvent)	(Display*, XEvent *);
	XSizeHints*	(*XAllocSizeHints)(void);
	void		(*XSetWMNormalHints)	(Display*, Window, XSizeHints*);
	int		(*XResizeWindow)(Display*, Window, unsigned int, unsigned int);
	int		(*XMapWindow)	(Display*, Window);
	int		(*XFree)		(void *data);
	int		(*XFreePixmap)	(Display*, Pixmap);
	Pixmap	(*XCreatePixmap)(Display*, Drawable, unsigned int, unsigned int, unsigned int);
	void	(*XCopyArea)	(Display*, Drawable, Drawable, GC, int, int, unsigned int, unsigned int, int, int);
	Pixmap	(*XCreateBitmapFromData)	(Display*, Drawable, _Xconst char*, unsigned int, unsigned int);
	Status	(*XAllocColor)	(Display*, Colormap, XColor*);
	Display*(*XOpenDisplay)	(_Xconst char*);
	int	(*XCloseDisplay)	(Display*);
	Window	(*XCreateSimpleWindow)	(Display*, Window, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long);
	GC		(*XCreateGC)	(Display*, Drawable, unsigned long, XGCValues*);
	int		(*XSelectInput)	(Display*, Window, long);
	int		(*XStoreName)	(Display*, Window, _Xconst char*);
	Window	(*XGetSelectionOwner)	(Display*, Atom);
	int		(*XConvertSelection)	(Display*, Atom, Atom, Atom, Window, Time);
	int		(*XGetWindowProperty)	(Display*, Window, Atom, long, long, Bool, Atom, Atom*, int*, unsigned long *, unsigned long *, unsigned char **);
	int		(*XChangeProperty)		(Display*, Window, Atom, Atom, int, int, _Xconst unsigned char*, int);
	Status	(*XSendEvent)	(Display*, Window, Bool, long, XEvent*);
	XImage*	(*XCreateImage)	(Display *, Visual *, unsigned int, int, int, char *,unsigned int, unsigned int, int, int);
#ifndef XPutPixel
	void	(*XPutPixel)	(XImage*,int,int,unsigned long);
#endif
	void	(*XPutImage)	(Display*, Drawable, GC, XImage *, int,int,int,int,unsigned int,unsigned int);
#ifndef XDestroyImage
	void	(*XDestroyImage)(XImage*);
#endif
	int		(*XSetSelectionOwner)	(Display*, Atom, Window, Time);	
	int		(*XSetIconName)	(Display*, Window, _Xconst char *);
	int		(*XSynchronize)	(Display*, Bool);
	Status	(*XGetWindowAttributes)	(Display*,Window,XWindowAttributes*);
	XWMHints* (*XAllocWMHints) (void);
	void	(*XSetWMProperties) (Display*, Window, XTextProperty*, XTextProperty*, char**, int, XSizeHints*, XWMHints*, XClassHint*);
	Status	(*XSetWMProtocols) (Display*, Window, Atom *, int);
	Atom	(*XInternAtom) (Display *, char *, Bool);
	int		(*XFreeColors) (Display*, Colormap, unsigned long *, int, unsigned long);
};



extern int local_pipe[2];			/* Used for passing local events */
extern int key_pipe[2];			/* Used for passing keyboard events */

extern struct x11 x11;

extern char 	*copybuf;
extern pthread_mutex_t	copybuf_mutex;
extern char 	*pastebuf;
extern sem_t	pastebuf_set;
extern sem_t	pastebuf_used;
extern sem_t	init_complete;
extern sem_t	mode_set;
extern sem_t	event_thread_complete;
extern int terminate;
extern int x11_window_xpos;
extern int x11_window_ypos;
extern int x11_window_width;
extern int x11_window_height;
extern int x11_initialized;

void x11_event_thread(void *args);

#endif
