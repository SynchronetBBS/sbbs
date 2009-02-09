//Declaration and initialization of CRT.H functions global variables.
//The comments about them refers to expected or default values, not to actual
//values. If actual values are different of default values, the user might
//want to load in these variables the actual values (using some special
//functions or changing their values) in order to CRT.H functions work
//properly

int vmode_x=80;	//number of expected columns in text mode (default=80)*
int vmode_y=25;  	//number of expected rows in text mode (default=25)*
int vmode_mode=3; //expected video mode (default=3) (updated by CRT.H functions)
						//* by CRT.H functions
						//They might be sometimes different from the actual number
						//of rows and columns in current text mode. To load their
						//actual values use crt_detect. crt_init also updates
						//crtwin_dta coordinates to full screen.

int  crt_direct=0;// if == 0 CRT.H functions write/read directly to video RAM,
	//otherwise, they write/read by BIOS calls (INT 10h)
int  crt_page=0;  //text page used when crt_direct!=0

int  crt_EVGA_addr=0x3C0; //EGA/VGA base port address. Alternate at 0x2C0

char*video_addr=(char *)0xb8000000UL;
							//base address for the video map (RAM)

char*crtvar_p;	//pointer used in many functions to access the video RAM

struct crtwin			//coordinates used by crtwindow and printsj
 {
	int left;
	int top;
	int right;
	int bottom;
 }crtwin_dta={(int)-1,(int)-1,(int)80,(int)25}; //default values. This line means:
	//crtwin_dta.left=-1;
	//crtwin_dta.top=-1;
	//crtwin_dta.right=80;
	//crtwin_dta.bottom=25;

int crtwin_just=1; //used by printsj and printsjc. Default=1(CENTER TEXT)
