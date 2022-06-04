Synchronet Virtual DOS Modem (SVDM) for Windows - Copyright 2022 Rob Swindell

For the current/complete documentation on the latest build/version, see
https://wiki.synchro.net/util:svdm

SVDM is a modem emulator for Windows which utilizes the Synchronet Virtual
UART/FOSSIL Driver (DOSXTRN.EXE/SBBSEXEC.DLL) to enable support for DOS
communications programs on Windows NT-based operating systems. 

SVDM should run on 32-bit and 64-bit editions of Windows XP and later.

For operation on 64-bit editions of Windows, you will need to also install
NTVDMx64: http://www.columbia.edu/~em36/ntvdmx64.html

On 32-bit editions of Windows Vista and later, the SBBSEXEC.DLL must also
be copied to your Windows/System32 directory (by an administrator).

Although SVDM reuses components of the Synchronet BBS Software and shares some
of its source code and libraries, it is is not technically "part of"
Synchronet BBS nor is it required for the normal use or operation of a
Synchronet BBS.
 
===== Uses =====
  * Use 16-bit DOS terminal programs (e.g. Telix, ProComm, Qmodem, Telemate)
    to connect to Internet-connected BBSes
  * Use 16-bit DOS terminal program's "host mode" to accept incoming Telnet 
    or "Raw TCP" connections from Internet clients
  * Use 16-bit DOS BBS program (e.g. Synchronet v1/2, WWIVv4, TriBBS, etc.)
    to accept incoming Telnet or "Raw TCP" connections from Internet users
  * Run 16-bit DOS door programs from Internet-connected BBSes
   (with the SVDM -h command-line option)

===== Features ===== 
  * Open source (Microsoft Visual C)
  * Emulates an NS16550 UART
  * Provides a FOSSIL/PC-BIOS (int14h) software interface
  * IPv4 and IPv6 support (for inbound and outbound connections)
  * Inbound connection filtering based on IP address/range
  * Telnet client and server support
  * Raw TCP client and server support
  * Accurate Hayes Smartmodem / USRobotics Sportster Modem "AT Command Set"
     emulation
     * Complete with virtual NVRAM settings/number storage
     * Optional auto-answer (ATS0=1)
     * RING result upon incoming connections
     * Verbal and numeric result modes
     * Caller ID support (enabled with AT+VCID=1 or AT#CID=1)
     * "Dial" IP addresses or hostnames
       with optionally-specified TCP port and/or protocol

===== Install =====
SVDM currently does not include an "installer", so you will need to copy the
following files to a directory ideally in your system's search path:
  * svdm.exe - the main program (modem emulator)
  * svdm.ini - configuration settings, optional
  * dosxtrn.exe - 16-bit FOSSIL driver and VDD loader
  * sbbsexec.dll - 32-bit Virtual UART/Device Driver

On 32-bit editions of Windows Vista and later, sbbsexec.dll must also be
copied to your Windows/System32 directory (by an administrator).

===== Usage =====
The general usage is to use svdm to execute the 16-bit DOS communications
program of interest. Any command-line options you want to pass to the program
may be specified on the command-line after the program path/filename.

usage: svdm [-opts] <program> [options]
opts:
        -telnet   Use Telnet protocol by default
        -raw      Use Raw TCP by default
        -4        Use IPv4 address family by default
        -6        Use IPv6 address family by default
        -l[addr]  Listen for incoming TCP connections
                  [on optionally-specified network interface]
        -p<port>  Specify default TCP port number (decimal)
        -n<node>  Specify node number
        -d        Enable debug output
        -h<sock>  Specify socket descriptor/handle to use (decimal)
        -r<cps>   Specify maximum receive data rate (chars/second)
        -c<fname> Specify alternate configuration (.ini) path/filename
        -V        Display detailed version information and exit

==== Flow Control ====
When utilizing UART (COM Port) virtualization (e.g. with most DOS terminal
programs and some DOS door programs), be sure to enable RTS/CTS hardware flow
control in the DOS program and do **not** enable DTR/DSR or XON/XOFF
(software) flow control.

==== Ports ====

For DOS programs using UART/COM Port I/O, the COM port number (or I/O port and
IRQ) must match what SVDM is virtualizing. By default, SVDM virtualizes COM 1
(I/O Port 3F8, IRQ 4), but this can be changed globally or per program via the
svdm.ini file.

For DOS programs using the PC-BIOS/FOSSIL int14h interface, all COM ports are
supported and treated identically (i.e. the DX register value is ignored).

==== Baud Rates ====

The DTE baud rate set by the DOS program is not used by SVDM (i.e. the
transmit and receive data rates are only limited by the TCP/IP network). If
you wish to artificially limit the receive data rate for some reason (not
normally necessary), you can use the -r command-line option or the "Rate"
key in the root section of svdm.ini to set the maximum receive rate, in
characters per second.

A DTE rate of 38400 bps is reflected by virtual device driver and if the DOS
program attempts to change this value, there is no effect.

==== Connect Rates ====

For incoming TCP connections, SVDM will report either "CONNECT 9600" or just
"CONNECT" (assumed 300 bps) depending on the modem's extended response
(ATX) setting.

==== Bits o' Parity ====

The communication data (word) bit width, stop bits, and parity settings used
by the DOS program are not used by SVDM. The equivalent of 8 data bits, no
parity, and one stop bit, is assumed.

==== Disconnect ==== 
If the DOS program de-asserts ("drops") DTR, the TCP connection will be
terminated (by default). Alternatively, the DOS program can send the escape
sequence (by default, "+++", surrounded by a one second guard time of
inactivity) to enter command mode and then send "ATH" or "ATH0" to disconnect.

===== Examples =====

Run Telix (popular comm program for MS-DOS from the 1980s):
  svdm telix

Run Renegade BBS Software (in WFC mode), listening on all network interfaces
for incoming TCP connections:
  svdm -l renegade
  
===== Dialing =====

SVDM responds to the ATD dial command to perform an outbound TCP connection.
If the first character of the dial-string (following the 'D') is an uppercase
'T' or 'P', this character is ignored as it was used to specify "tone" or 
"pulse" dialing on the old telephone system and not relevant to TCP/IP
connections. For example, any of the following commands may be used to "dial"
test.synchro.net:
  ATDTtest.synchro.net
  ATDPtest.synchro.net  
  ATDtest.synchro.net
  atdtest.synchro.net

The address to "dial" may be specified as a DNS hostname, an IPv4 address, or
an IPv6 address (in brackets):
  ATDvert.synchro.net
  ATD192.168.1.2
  ATD[::1]

The TCP port to connect to may be specified following the address and a colon:
  ATDvert.synchro.net:23
  ATD192.168.1.2:23
  ATD[::1]:23
  
The TCP protocol to connect with may be specified (followed by a colon) before
the address:
  ATDtelnet:vert.synchro.net
  ATDtelnet:192.168.1.2
  ATDraw:[::1]
  
The last "number dialed" can be dialed again by dialing 'L' or 'l':
  ATDL
  
Up to 20 dial strings (numbers) can be stored using the AT&Z command:
  AT&Z0=vert.synchro.net
  AT&Z1=L
  
Saved dial strings (numbers) can be queried/displayed using the AT&Z command:
  AT&Z0?
  AT&ZL?

Save dial strings (numbers) can be dialed by using the ATDSn command:
  ATDS0

If your DOS terminal program of choice has trouble dialing long dial strings
(e.g. accommodating long DNS hostnames or IPv6 addresses), try using the saved
number storage feature to resolve that limitation. After enter the following
command, dialing "S0" as a "phone number" would actually connect to 
"telnet:vert.synchro.net:23".
  AT&Z0=telnet:vert.synchro.net:23

===== Configure =====

SVDM's behavior can be customized or modified by specifying command-line
parameters, invoking modem "AT commands", or creating/modifying a .ini file.

The default configuration file svdm.ini will be loaded from the same
directory where the executed svdm.exe is located.

Settings changed via modem AT command take precedence over command-line
options which take precedence over .ini file settings.

Settings in the //Root// section may also be specified in a program-specific
section (named after the program itself), to create program-specific settings.

Settings in the UART section may also be over-ridden in a program-specific
section but naming the section [//<program>//.UART].
For example, by default SVDM emulates a UART for COM Port 1 
(I/O address 0x3f8, IRQ 4), but you could change the emulated COM Port for one
particular program by creating a program-specific UART section in the .ini
file:
[telix.UART]
ComPort=2

===== AT Command Set =====

SVDM will recognize and respond to modem commands only when in "command mode".

When there is no active connection, "command mode" is the current mode. When
there is an active connection, the terminal can force the modem into command
mode by sending the Hayes-302 escape sequence (by default, "+++") with the
minimum guard time of inactivity before and after the escape sequence. The
duration of the guard time (default of one second) and the character repeated
3 times for the escape sequence are configurable via modem S-registers.

Modem commands are prefixed by "AT" (case-insensitively), which stands for
"attention", hence the name "AT Command".

After the initial "AT" is sent to the modem, zero or more commands may be
specified, followed by a final carriage-return (CR) character. The ASCII
values of the carriage-return and line-feed characters are configurable via
modem S-registers. The backspace character (configurable via S-register) is
recognized as a destructive edit command to modify a command sequence before
the command is submitted (usually with the ENTER key).

If all of the commands are valid, then the modem will respond with the "OK"
response (or 0 in numeric response mode). There are exceptions (e.g. D
command).

If any of the commands are invalid, then the modem will respond with the 
"ERROR" response (or 4 in numeric response mode).

Any white-space characters within the AT command sequence are ignored.

Some commands (e.g. D, &Zn=), must be the last command of the command
sequence since they accept an arbitrary string argument that may contain
otherwise-valid AT command characters and sequences.
 
For the current/complete documentation on the latest build/version, see
https://wiki.synchro.net/util:svdm
