/* $Id$ */

#define CIO_KEY_HOME      0x47 << 8
#define CIO_KEY_UP        72   << 8
#define CIO_KEY_END       0x4f << 8
#define CIO_KEY_DOWN      80   << 8
#define CIO_KEY_F(x)      (0x3a+x) << 8
#define CIO_KEY_IC        0x52 << 8
#define CIO_KEY_DC        0x53 << 8
#define CIO_KEY_LEFT      0x4b << 8
#define CIO_KEY_RIGHT     0x4d << 8
#define CIO_KEY_PPAGE     0x49 << 8
#define CIO_KEY_NPAGE     0x51 << 8

#define CIO_KEY_BACKSPACE    0xff01   /* Windows never differentiates between a
				   * backspace keypress and the backspace
				   * char
				   */
#define CIO_KEY_MOUSE	0xff02

#ifndef KEY_HOME
 #define KEY_HOME      0x47 << 8
#endif
#ifndef KEY_UP
 #define KEY_UP        72   << 8
#endif
#ifndef KEY_END
 #define KEY_END       0x4f << 8
#endif
#ifndef KEY_DOWN
 #define KEY_DOWN      80   << 8
#endif
#ifndef KEY_F
 #define KEY_F(x)      (0x3a+x) << 8
#endif
#ifndef KEY_IC
 #define KEY_IC        0x52 << 8
#endif
#ifndef KEY_DC
 #define KEY_DC        0x53 << 8
#endif
#ifndef KEY_LEFT
 #define KEY_LEFT      0x4b << 8
#endif
#ifndef KEY_RIGHT
 #define KEY_RIGHT     0x4d << 8
#endif
#ifndef KEY_PPAGE
 #define KEY_PPAGE     0x49 << 8
#endif
#ifndef KEY_NPAGE
 #define KEY_NPAGE     0x51 << 8
#endif

#ifndef KEY_BACKSPACE
 #define KEY_BACKSPACE    0xff01   /* Windows never differentiates between a
				   * backspace keypress and the backspace
				   * char
				   */
#endif

#ifndef KEY_MOUSE
#define KEY_MOUSE	0xff02
#endif
