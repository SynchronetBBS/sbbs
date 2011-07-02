var ClientVars = {};

// AutoConnect controls how a connection to the remote server is initiated.  
//   0 = Disabled -- User must click a button to connect
//   1 = Enabled -- Connection is made after the page loads
// Default: 0
ClientVars.AutoConnect = 0;

// BitsPerSecond controls how fast data is received from the remote server 
// (except during file transfers, which run at full speed).
//   Here are some suggested values to try:
//     115200, 57600, 38400, 28800, 19200, 14400, 9600, 4800, 2400, 1200, 300
//   (Those were just common bitrates, feel free to use any value you want)
// Default: 115200
ClientVars.BitsPerSecond = 115200;

// Blink controls whether blinking text and high intensity background colours 
// are available.
//   0 = Blinking text is disabled, high intensity background colours are 
//       enabled (this is how modern Windows OS's handle blinking text)
//   1 = Blinking text is enabled, high intensity background colours are 
//        disabled (this is how it was with DOS)
// Default: 1
ClientVars.Blink = 1;

// BorderStyle controls how the border and close button looks.
//   "FlashTerm"  = Single pixel border, like FlashTerm, no close button
//   "None"       = No border, no close button
//   "Ubuntu1004" = Ubuntu 10.04 style
//   "Win2000"    = Windows 2000 style
//   "Win7"       = Windows 7 style
//   "WinXP"      = Windows XP style
// Default: Win7
ClientVars.BorderStyle = "None";

// CodePage controls which font file to use.
// NOTE: Each code page has its own set of supported FondWidth and FontHeight
//       values, so see the online documentation for help selecting OK values
//   Available IBM PC code pages: 
//     437 = The original IBM PC code page
//     737 = Greek
//     850 = Multilingual (Latin-1) (Western European languages)
//     852 = Slavic (Latin-2) (Central and Eastern European languages)
//     855 = Cyrillic
//     857 =  Turkish
//     860 = Portuguese
//     861 = Icelandic
//     862 = Hebrew
//     863 = French (Quebec French)
//     865 = Danish/Norwegian
//     866 = Cyrillic
//     869 = Greek
//   Available Amiga "code pages": 
//     BStrict
//     BStruct
//     MicroKnight
//     MoSoul
//     PotNoodle
//     Topaz
//     TopazPlus
//   Available Atari "code pages" for standard 40 column displays:
//     ATASCII-Arabic
//     ATASCII-Graphics (Default for Atari)
//     ATASCII-International
//   Available Atari "code pages" for non-standard 80 column displays:
//     ATASCII-Arabic-HalfWidth
//     ATASCII-Graphics-HalfWidth
//     ATASCII-International-HalfWidth
// Default: 437
ClientVars.CodePage = "437"; 

// ConnectAnsi controls which ANSI file to display under the initial Connect button
// NOTE: This setting has no effect when ClientVars.RIP is set to 1
// Default: connect.ans
ClientVars.ConnectAnsi = "/ftelnet/connect.ans";

// ConnectButtonX and ConnectButtonY control where the connect button will be displayed
// NOTE: To center the connect button, use 0 for both values
// NOTE: To position relative to the top or left, use positive values.  To position relative to the bottom or right, use negative values
// Default: 0 
ClientVars.ConnectButtonX = -30;
ClientVars.ConnectButtonY = -30;

// ConnectRIP controls which RIP file to display under the initial Connect button
// NOTE: This setting has no effect unless ClientVars.RIP is set to 1
// Default: connect.rip
ClientVars.ConnectRIP = "/ftelnet/connect.rip";

// Enter controls what to send to the server when the Enter key is pressed.  
//   The telnet specification says to send \r\n (ASCII CRLF) when the Enter key
//   is pressed, but not all software will work with that, so you may have more
//   luck with the non-standard \r (ASCII CR without the LF). 
// Default: \r
ClientVars.Enter = "\r";

// FontHeight and FontWidth control the size of each character on screen.  
// NOTE: These values only need to be set if you're using an IBM PC code page.
//       The Amiga and Atari code pages will override these values, as there
//       is only one available font size for those "code pages".
//   Available IBM PC code page sizes: 
//     437 = 8x12, 8x13, 9x16, 10x19, 12x23
//     737 =             9x16,        12x23
//     850 =       8x13, 9x16, 10x19, 12x23
//     852 =             9x16, 10x19, 12x23
//     855 =             9x16                 
//     857 =             9x16              
//     860 =             9x16              
//     861 =             9x16              
//     862 =                   10x19         
//     863 =             9x16                 
//     865 =       8x13, 9x16, 10x19, 12x23
//     866 =             9x16                 
//     869 =             9x16              
// Default: 9x16
ClientVars.FontWidth = 9;
ClientVars.FontHeight = 16;

// RIP controls whether the client will display RIPscrip or ANSI
// Default: 0
ClientVars.RIP = 0;

// ScreenColumns and ScreenRows control the size of the screen.  
//   Anything within reason should be accepted 
//   (no sanity checking is done, so bad values can/will crash the program)
// Default: 80x25 
ClientVars.ScreenColumns = 80;
ClientVars.ScreenRows = 25;