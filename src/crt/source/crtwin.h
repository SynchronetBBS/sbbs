struct crtwin //structure type of crtwin_dta global variable
 {
	int left;
	int top;
	int right;
	int bottom;
 };

extern struct crtwin crtwin_dta;

#define xi crtwin_dta.left
#define xf crtwin_dta.right
#define yi crtwin_dta.top
#define yf crtwin_dta.bottom
