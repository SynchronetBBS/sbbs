#ifndef _X_EVENTS_H_
#define _X_EVENTS_H_

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

enum x11_local_events {
	 X11_LOCAL_SETMODE
	,X11_LOCAL_SETNAME
	,X11_LOCAL_SETTITLE
	,X11_LOCAL_COPY
	,X11_LOCAL_PASTE
	,X11_LOCAL_DRAWRECT
	,X11_LOCAL_FLUSH
	,X11_LOCAL_BEEP
	,X11_LOCAL_SETICON
};

struct x11_local_event {
	enum x11_local_events	type;
	union {
		int		mode;
		char	name[81];
		char	title[81];
		struct	rectlist *rect;
		unsigned long	*icon_data;
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
	int	(*XPutImage)	(Display*, Drawable, GC, XImage *, int,int,int,int,unsigned int,unsigned int);
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
	XVisualInfo *(*XGetVisualInfo)(Display *display, long vinfo_mask, XVisualInfo *vinfo_template, int *nitems_return);
	Window (*XCreateWindow)(Display *display, Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width, int depth, 
                       unsigned int class, Visual *visual, unsigned long valuemask, XSetWindowAttributes *attributes);
	Colormap (*XCreateColormap)(Display *display, Window w, Visual *visual, int alloc);
	XClassHint *(*XAllocClassHint)(void);
	int (*XSetForeground)(Display *display, GC gc, unsigned long foreground);
	char *(*XSetLocaleModifiers)(char *modifier_list);
	XIM (*XOpenIM)(Display *display, XrmDatabase db, char *res_name, char *res_class);
	XIC (*XCreateIC)(XIM im, ...);
	int (*XwcLookupString)(XIC ic, XKeyPressedEvent *event, wchar_t *buffer_return, int wchars_buffer, KeySym *keysym_return, Status *status_return);
	void (*XSetICFocus)(XIC ic);
	Bool (*XFilterEvent)(XEvent *event, Window w);
	Atom utf8;
	Atom targets;
};

extern int local_pipe[2];			/* Used for passing local events */
extern int key_pipe[2];			/* Used for passing keyboard events */

extern struct x11 x11;

extern char 	*copybuf;
extern pthread_mutex_t	copybuf_mutex;
extern char 	*pastebuf;
extern sem_t	pastebuf_set;
extern sem_t	pastebuf_used;
extern Atom	copybuf_format;
extern Atom	pastebuf_format;
extern sem_t	init_complete;
extern sem_t	mode_set;
extern sem_t	event_thread_complete;
extern int terminate;
extern int x11_window_xpos;
extern int x11_window_ypos;
extern int x11_window_width;
extern int x11_window_height;
extern int x11_initialized;
extern struct video_stats x_cvstat;

void x11_event_thread(void *args);

#endif
