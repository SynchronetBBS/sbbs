#line 2 "SCFGNODE.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/****************************************************************************/
/* Synchronet configuration utility 										*/
/****************************************************************************/

#include "scfg.h"
#include "spawno.h"

int com_type()
{
	int i;

i=0;
strcpy(opt[0],"UART");
strcpy(opt[1],"FOSSIL Int 14h");
strcpy(opt[2],"PC BIOS Int 14h");
strcpy(opt[3],"PS/2 BIOS Int 14h");
strcpy(opt[4],"DigiBoard Int 14h");
opt[5][0]=0;
SETHELP(WHERE);
/*
COM Port Type:

Select the type of serial COM port for this node. If you are unsure,
select UART. If you have a FOSSIL driver installed, you do not have to
select FOSSIL unless you specifically want to override Synchronet's
internal COM functions.
*/
i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
	,"COM Port Type",opt);
if(i==-1)
	return(0);
changes=1;
if(i) {
	com_irq=com_port-1;
	if(i==1) {
		savnum=0;
		umsg("WARNING: This is not a recommended setting for most systems");
		umsg("The default DSZ command lines will not work with this setting");
		com_base=0xf; }
	else if(i==2)
		com_base=0xb;
	else if(i==3)
		com_base=0xe;
	else if(i==4)
		com_base=0xd; }

else {	/* UART */
	if(com_port && com_port<5) {
		i=0;
		strcpy(opt[0],"Yes");
		strcpy(opt[1],"No");
		opt[2][0]=0;
		SETHELP(WHERE);
/*
Use Default UART IRQ and I/O Address:

If your COM Port's UART is using the normal IRQ and Base I/O Address
for the configured COM Port number, select Yes. If your COM Port
is using a non-standard IRQ or I/O Address, select No and be sure
to set the UART IRQ and UART I/O Address options. If you are not
sure what IRQ and I/O Address your COM Port is using, select Yes.
*/
		i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
			,"Use Default UART IRQ and I/O Address"
			,opt);
		if(i==0) {
			switch(com_port) {
				case 1:
					com_irq=4;
					com_base=0x3f8;
					break;
				case 2:
					com_irq=3;
					com_base=0x2f8;
					break;
				case 3:
					com_irq=4;
					com_base=0x3e8;
					break;
				case 4:
					com_irq=3;
					com_base=0x2e8;
					break; }
			changes=1;
			return(1); } }
	else {
		if(com_base<0x100)
			com_base=0x100;
		if(com_irq<2 || com_irq>15)
			com_irq=5; } }
return(0);
}

void node_menu()
{
	char	done,str[81],savnode=0;
	int 	i,j;
	static int node_menu_dflt,node_bar;

while(1) {
	for(i=0;i<sys_nodes;i++)
		sprintf(opt[i],"Node %d",i+1);
	opt[i][0]=0;
	j=WIN_ORG|WIN_ACT|WIN_INSACT|WIN_DELACT;
	if(sys_nodes>1)
		j|=WIN_DEL|WIN_GET;
	if(sys_nodes<MAX_NODES && sys_nodes<MAX_OPTS)
		j|=WIN_INS;
	if(savnode)
		j|=WIN_PUT;
SETHELP(WHERE);
/*
Node List:

This is the list of configured nodes in your system.

To add a node, hit  INS .

To delete a node, hit  DEL .

To configure a node, select it using the arrow keys and hit  ENTER .

To copy a node's configuration to another node, first select the source
node with the arrow keys and hit  F5 . Then select the destination
node and hit  F6 .
*/

	i=ulist(j,0,0,13,&node_menu_dflt,&node_bar,"Nodes",opt);
	if(i==-1) {
		if(savnode) {
			free_node_cfg();
			savnode=0; }
		return; }

	if((i&MSK_ON)==MSK_DEL) {
		strcpy(opt[0],"Yes");
		strcpy(opt[1],"No");
		opt[2][0]=0;
		sprintf(str,"Delete Node %d",sys_nodes);
		i=1;
SETHELP(WHERE);
/*
Delete Node:

If you are positive you want to delete this node, select Yes. Otherwise,
select No or hit  ESC .
*/
		i=ulist(WIN_MID,0,0,0,&i,0,str,opt);
		if(!i) {
			--sys_nodes;
			FREE(node_path[sys_nodes]);
			write_main_cfg(); }
		continue; }
	if((i&MSK_ON)==MSK_INS) {
		strcpy(node_dir,node_path[sys_nodes-1]);
		i=sys_nodes+1;
		upop("Reading NODE.CNF...");
		read_node_cfg(txt);
		upop(0);
		sprintf(str,"..\\NODE%d\\",i);
		sprintf(tmp,"Node %d Path",i);
SETHELP(WHERE);
/*
Node Path:

This is the path to this node's private directory where its separate
configuration and data files are stored.

The drive and directory of this path can be set to any valid DOS
directory that can be accessed by ALL nodes and MUST NOT be on a RAM disk
or other volatile media.

If you want to abort the creation of this new node, hit  ESC .
*/
		j=uinput(WIN_MID,0,0,tmp,str,50,K_EDIT|K_UPPER);
		changes=0;
		if(j<2)
			continue;
		truncsp(str);
		if((node_path=(char **)REALLOC(node_path,sizeof(char *)*(sys_nodes+1)))
			==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sys_nodes+1);
			continue; }
		if((node_path[i-1]=MALLOC(strlen(str)+2))==NULL) {
			errormsg(WHERE,ERR_ALLOC,str,strlen(str)+2);
			continue; }
		strcpy(node_path[i-1],str);
		if(str[strlen(str)-1]=='\\')
			str[strlen(str)-1]=0;
		mkdir(str);
		node_num=++sys_nodes;
		sprintf(node_name,"Node %u",node_num);
		write_node_cfg();
		write_main_cfg();
		free_node_cfg();
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		if(savnode)
			free_node_cfg();
		i&=MSK_OFF;
		strcpy(node_dir,node_path[i]);
		upop("Reading NODE.CNF...");
		read_node_cfg(txt);
		upop(0);
		savnode=1;
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		strcpy(node_dir,node_path[i]);
		node_num=i+1;
		write_node_cfg();
		changes=1;
		continue; }

	if(savnode) {
		free_node_cfg();
		savnode=0; }
	strcpy(node_dir,node_path[i]);
	upop("Reading NODE.CNF...");
	read_node_cfg(txt);
	upop(0);
	if(node_num!=i+1) { 	/* Node number isn't right? */
		node_num=i+1;		/* so fix it */
		write_node_cfg(); } /* and write it back */
	node_cfg();

	free_node_cfg(); }
}

void node_cfg()
{
	static	int node_dflt;
	char	done,str[81];
	static	int mdm_dflt,adv_dflt,tog_dflt,tog_bar;
	int 	i,j,dflt=0,bar=0;

while(1) {
	i=0;
	sprintf(opt[i++],"%-27.27s%s","Name",node_name);
	sprintf(opt[i++],"%-27.27s%s","Phone Number",node_phone);
	sprintf(opt[i++],"%-27.27s%ubps","Minimum Connect Rate",node_minbps);
	sprintf(opt[i++],"%-27.27s%.40s","Logon Requirements",node_ar);
	sprintf(opt[i++],"%-27.27s%.40s","Local Text Editor",node_editor);
	sprintf(opt[i++],"%-27.27s%.40s","Local Text Viewer",node_viewer);
	sprintf(opt[i++],"%-27.27s%.40s","Configuration Command",scfg_cmd);
	sprintf(opt[i++],"%-27.27s%.40s","DOS Command Interpreter"
		,node_comspec[0] ? node_comspec : "%COMSPEC%");
	strcpy(opt[i++],"Toggle Options...");
	strcpy(opt[i++],"Advanced Options...");
	strcpy(opt[i++],"Modem Configuration...");
	strcpy(opt[i++],"Wait for Call Number Keys...");
	strcpy(opt[i++],"Wait for Call Function Keys...");
	opt[i][0]=0;
	sprintf(str,"Node %d Configuration",node_num);
SETHELP(WHERE);
/*
Node Configuration Menu:

This is the node configuration menu. The options available from this
menu will only affect the selected node's configuration.

Options with a trailing ... will produce a sub-menu of more options.
*/
	switch(ulist(WIN_ACT|WIN_CHE|WIN_BOT|WIN_RHT,0,0,60,&node_dflt,0
		,str,opt)) {
		case -1:
			i=save_changes(WIN_MID);
			if(!i)
				write_node_cfg();
			if(i!=-1)
				return;
			break;
		case 0:
SETHELP(WHERE);
/*
Node Name:

This is the name of the selected node. It is used for documentary
purposes only.
*/
			uinput(WIN_MID|WIN_SAV,0,10,"Name",node_name,40,K_EDIT);
			break;
		case 1:
SETHELP(WHERE);
/*
Node Phone Number:

This is the phone number to access the selected node. It is used for
documentary purposes only.
*/
			uinput(WIN_MID|WIN_SAV,0,10,"Phone Number",node_phone,12,K_EDIT);
			break;
		case 2:
SETHELP(WHERE);
/*
Minimum Connect Rate:

This is the lowest modem connection speed allowed for the selected node.
The speed is a decimal number representing bits per second (bps) or more
inaccurately referred to as baud.

If a user attempts to logon this node at a speed lower than the minimum
connect rate, a message is displayed explaining the minimum connect
rate and the user is disconnected.

If the file TEXT\TOOSLOW.MSG exists, it will be displayed to the user
before the user is disconnected.

Users with the M exemption can log onto any node at any speed.
*/
			sprintf(str,"%u",node_minbps);
			uinput(WIN_MID|WIN_SAV,0,10,"Minimum Connect Rate",str,5
				,K_NUMBER|K_EDIT);
			node_minbps=atoi(str);
			break;
		case 3:
			sprintf(str,"Node %u Logon",node_num);
			getar(str,node_ar);
			break;
		case 4:
SETHELP(WHERE);
/*
Local Text Editor:

This is the command line to execute to edit text files locally in
Synchronet. If this command line is blank, the default editor of the
sysop (user #1) will be used.

This command line can be used to create posts and e-mail locally, if the
toggle option Use Editor for Messages is set to Yes.

The %f command line specifier is used for the filename to edit.
*/
			uinput(WIN_MID|WIN_SAV,0,13,"Editor",node_editor
				,50,K_EDIT);
			break;
		case 5:
SETHELP(WHERE);
/*
Local Text Viewer:

This is the command line used to view log files from the wait for call
screen.

The %f command line specifier is used for the filename to view.
*/
			uinput(WIN_MID|WIN_SAV,0,14,"Viewer",node_viewer
				,50,K_EDIT);
			break;
		case 6:
SETHELP(WHERE);
/*
Configuration Command Line:

This is the command line used to launch the Synchronet configuration
program (SCFG) from this node's Waiting for Call (WFC) screen.
*/
			uinput(WIN_MID|WIN_SAV,0,14,"Configuration Command",scfg_cmd
				,40,K_EDIT);
            break;
		case 7:
SETHELP(WHERE);
/*
DOS Command Interpreter:

This is the complete path to your DOS command interpreter (normally
COMMAND.COM). Leaving this option blank (designated by %COMSPEC%)
specifies that the path contained in the COMSPEC environment variable
should be used.

When running Synchronet for OS/2, this option should be set to:
C:\OS2\MDOS\COMMAND.COM.
*/
			uinput(WIN_MID|WIN_SAV,0,14,"DOS Command Interpreter"
				,node_comspec,40,K_EDIT);
            break;
		case 8:
			done=0;
			while(!done) {
				i=0;
				sprintf(opt[i++],"%-27.27s%s","Alarm When Answering"
					,node_misc&NM_ANSALARM ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Status Screen While WFC"
					,node_misc&NM_WFCSCRN ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Total Msgs/Files on WFC"
					,node_misc&NM_WFCMSGS ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Use Editor for Messages"
					,node_misc&NM_LCL_EDIT ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Use EMS for Overlays"
					,node_misc&NM_EMSOVL ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Allow Swapping"
                    ,node_swap&SWAP_NONE ? "No":"Yes");
                sprintf(opt[i++],"%-27.27s%s","Swap to EMS Memory"
					,node_swap&SWAP_EMS ? "Yes":"No");
                sprintf(opt[i++],"%-27.27s%s","Swap to XMS Memory"
					,node_swap&SWAP_XMS ? "Yes":"No");
                sprintf(opt[i++],"%-27.27s%s","Swap to Extended Memory"
					,node_swap&SWAP_EXT ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Windows/OS2 Time Slice API"
					,node_misc&NM_WINOS2 ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","DESQview Time Slice API"
					,node_misc&NM_NODV ? "No":"Yes");
				sprintf(opt[i++],"%-27.27s%s","DOS Idle Interrupts"
					,node_misc&NM_INT28 ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Low Priority String Input"
					,node_misc&NM_LOWPRIO ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Allow Logon by Number"
					,node_misc&NM_NO_NUM ? "No":"Yes");
				sprintf(opt[i++],"%-27.27s%s","Allow Logon by Real Name"
					,node_misc&NM_LOGON_R ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Always Prompt for Password"
					,node_misc&NM_LOGON_P ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Disable Local Inactivity"
					,node_misc&NM_NO_INACT ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Disable Local Keyboard"
                    ,node_misc&NM_NO_LKBRD ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Local System Protection"
					,node_misc&NM_SYSPW ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Beep Locally"
					,node_misc&NM_NOBEEP ? "No":"Yes");
				sprintf(opt[i++],"%-27.27s%s","Allow 8-bit Remote Logons"
					,node_misc&NM_7BITONLY ? "No":"Yes");
				sprintf(opt[i++],"%-27.27s%s","Reset Video Between Calls"
					,node_misc&NM_RESETVID ? "Yes":"No");

				opt[i][0]=0;
				savnum=0;
SETHELP(WHERE);
/*
Node Toggle Options:

This is the toggle options menu for the selected node's configuration.

The available options from this menu can all be toggled between two or
more states, such as Yes and No.
*/
				switch(ulist(WIN_BOT|WIN_RHT|WIN_ACT|WIN_SAV,3,2,35,&tog_dflt
					,&tog_bar,"Toggle Options",opt)) {
					case -1:
						done=1;
						break;
					case 0:
						i=1;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
SETHELP(WHERE);
/*
Alarm When Answering:

Set this option to Yes if want this node to make an alarm sound each
time it answers an incoming call. Usually, the modem's speaker is a
sufficient alarm if one is desired.
*/
						i=ulist(WIN_MID|WIN_SAV,0,15,0,&i,0
							,"Sound Alarm When Answering",opt);
						if(i==0 && !(node_misc&NM_ANSALARM)) {
							node_misc|=NM_ANSALARM;
							changes=1; }
						else if(i==1 && (node_misc&NM_ANSALARM)) {
							node_misc&=~NM_ANSALARM;
							changes=1; }
						break;
					case 1:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
SETHELP(WHERE);
/*
Status Screen While WFC:

If you want the current system statistics and current status of all
active nodes to be displayed while this node is waiting for a caller,
set this option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Display Status Screen While Waiting for Call",opt);
						if(i==0 && !(node_misc&NM_WFCSCRN)) {
							node_misc|=NM_WFCSCRN;
							changes=1; }
						else if(i==1 && (node_misc&NM_WFCSCRN)) {
							node_misc&=~NM_WFCSCRN;
							changes=1; }
						break;
					case 2:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
SETHELP(WHERE);
/*
Total Messages and Files on WFC Status Screen:

If you want the total number of messages and files to be retrieved
and displayed on the WFC status screen, set this option to Yes.

Setting this option to No will significantly reduce the amount of
time required to retrieve the system statistics on systems with large
numbers of message and file areas.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Display Total Messages and Files while WFC",opt);
						if(i==0 && !(node_misc&NM_WFCMSGS)) {
							node_misc|=(NM_WFCMSGS|NM_WFCSCRN);
							changes=1; }
						else if(i==1 && (node_misc&NM_WFCMSGS)) {
							node_misc&=~NM_WFCMSGS;
							changes=1; }
                        break;
					case 3:
						i=1;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
SETHELP(WHERE);
/*
Use Local Text Editor for Messages:

If a local text editor command has been specified, it can be used to
create messages (posts and e-mail) locally by setting this option to
Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Use Local Text Editor to Create Messages",opt);
						if(i==0 && !(node_misc&NM_LCL_EDIT)) {
							node_misc|=NM_LCL_EDIT;
							changes=1; }
						else if(i==1 && (node_misc&NM_LCL_EDIT)) {
							node_misc&=~NM_LCL_EDIT;
							changes=1; }
						break;
					case 4:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
SETHELP(WHERE);
/*
Use Available Expanded Memory (EMS) for Swapping Overlays:

If you wish to use any available expanded memory (EMS) for swapping
overlays, set this option to Yes. Swapping overlays to memory is much
faster than swapping to disk.

If this option is set to Yes, Synchronet will attempt to allocation 360K
of EMS for swapping overlays into.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Use Available EMS for Overlays",opt);
						if(i==0 && !(node_misc&NM_EMSOVL)) {
							node_misc|=NM_EMSOVL;
							changes=1; }
						else if(i==1 && (node_misc&NM_EMSOVL)) {
							node_misc&=~NM_EMSOVL;
							changes=1; }
						break;
					case 5:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
                        SETHELP(WHERE);
/*
Allow Swapping:

If this option is set to Yes, Synchronet will use one of the available
Swap to options when swapping out of memory.	If you have no Extended,
EMS, or XMS memory available, and this option is set to Yes, Synchronet
will swap to disk.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Allow Swapping",opt);
						if(i==0 && node_swap&SWAP_NONE) {
							node_swap&=~SWAP_NONE;
							changes=1; }
						else if(i==1 && !(node_swap&SWAP_NONE)) {
							node_swap|=SWAP_NONE;
							changes=1; }
                        break;
					case 6:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
                        SETHELP(WHERE);
/*
Swap to EMS Memory:

If this option is set to Yes, and the Allow Swapping option is set to
Yes Synchronet will use EMS memory (when available) for swapping out
of memory.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Swap to EMS Memory",opt);
						if(i==1 && node_swap&SWAP_EMS) {
							node_swap&=~SWAP_EMS;
							changes=1; }
						else if(i==0 && !(node_swap&SWAP_EMS)) {
							node_swap|=SWAP_EMS;
							changes=1; }
                        break;
					case 7:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
                        SETHELP(WHERE);
/*
Swap to XMS Memory:

If this option is set to Yes, and the Allow Swapping option is set to
Yes Synchronet will use XMS memory (when available) for swapping out
of memory.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Swap to XMS Memory",opt);
						if(i==1 && node_swap&SWAP_XMS) {
							node_swap&=~SWAP_XMS;
							changes=1; }
						else if(i==0 && !(node_swap&SWAP_XMS)) {
							node_swap|=SWAP_XMS;
							changes=1; }
                        break;
					case 8:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
                        SETHELP(WHERE);
/*
Swap to Extended Memory:

If this option is set to Yes, and the Allow Swapping option is set to
Yes Synchronet will use non-XMS Extended memory (when available) for
swapping out of memory.

If you are running under a DOS multitasker (e.g. Windows, DESQview, or
OS/2) set this option to No.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Swap to Extended Memory",opt);
						if(i==1 && node_swap&SWAP_EXT) {
							node_swap&=~SWAP_EXT;
							changes=1; }
						else if(i==0 && !(node_swap&SWAP_EXT)) {
							node_swap|=SWAP_EXT;
							changes=1; }
                        break;
					case 9:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
						SETHELP(WHERE);
/*
Windows/OS2 Time Slice API:

If you wish to have Synchronet specifically use the Windows and OS/2
API call to surrender time slices when in a low priority task, set this
option to Yes. If setting this option to Yes causes erratic pauses
or performance problems, leave the option on No. This type of behavior
has been witnessed under Windows 3.1 when using this API call.

If running in an OS/2 DOS session, you will most likely want this
option set to Yes for the best aggregate performance for your system.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Use Windows/OS2 Time Slice API",opt);
						if(i==0 && !(node_misc&NM_WINOS2)) {
							node_misc|=NM_WINOS2;
							changes=1; }
						else if(i==1 && (node_misc&NM_WINOS2)) {
							node_misc&=~NM_WINOS2;
							changes=1; }
                        break;
					case 10:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
						SETHELP(WHERE);
/*
DESQview Time Slice API:

If this option is set to Yes, and this node is run under DESQview,
Synchronet will use the DESQview Time Slice API calls for intelligent
variable time slicing.

Since Synchronet automatically detects DESQview, there is no harm in
having this option set to Yes if you are not running DESQview. Only set
this option to No, if you wish to disable the DESQview API for some
reason.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"DESQview Time Slice API",opt);
						if(i==0 && node_misc&NM_NODV) {
							node_misc&=~NM_NODV;
							changes=1; }
						else if(i==1 && !(node_misc&NM_NODV)) {
							node_misc|=NM_NODV;
							changes=1; }
                        break;
					case 11:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
						SETHELP(WHERE);
/*
DOS Idle Interrupts:

If you want Synchronet to make DOS Idle Interrupts when waiting for user
input or in other low priority loops, set this option to Yes.

For best aggregate multinode performance under multitaskers, you should
set this option to Yes.

For best single node performance under multitaskers, you should set this
option to No.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"DOS Idle Interrupts",opt);
						if(i==0 && !(node_misc&NM_INT28)) {
							node_misc|=NM_INT28;
							changes=1; }
						else if(i==1 && (node_misc&NM_INT28)) {
							node_misc&=~NM_INT28;
							changes=1; }
                        break;
					case 12:
						i=1;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
						SETHELP(WHERE);
/*
Low Priority String Input:

Normally, Synchronet will not give up time slices (under a multitasker)
when users are prompted for a string of characters. This is considered
a high priority task.

Setting this option to Yes will force Synchronet to give up time slices
during string input, possibly causing jerky keyboard input from the
user, but improving aggregate system performance under multitaskers.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Low Priority String Input",opt);
						if(i==0 && !(node_misc&NM_LOWPRIO)) {
							node_misc|=NM_LOWPRIO;
							changes=1; }
						else if(i==1 && (node_misc&NM_LOWPRIO)) {
							node_misc&=~NM_LOWPRIO;
							changes=1; }
                        break;
					case 13:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
						SETHELP(WHERE);
/*
Allow Logon by Number:

If you want users to be able logon using their user number at the NN:
set this option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Allow Logon by Number",opt);
						if(i==0 && node_misc&NM_NO_NUM) {
							node_misc&=~NM_NO_NUM;
							changes=1; }
						else if(i==1 && !(node_misc&NM_NO_NUM)) {
							node_misc|=NM_NO_NUM;
							changes=1; }
                        break;
					case 14:
						i=1;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
						SETHELP(WHERE);
/*
Allow Logon by Real Name:

If you want users to be able logon using their real name as well as
their alias, set this option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Allow Logon by Real Name",opt);
						if(i==0 && !(node_misc&NM_LOGON_R)) {
							node_misc|=NM_LOGON_R;
							changes=1; }
						else if(i==1 && (node_misc&NM_LOGON_R)) {
							node_misc&=~NM_LOGON_R;
							changes=1; }
                        break;
					case 15:
						i=1;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
						SETHELP(WHERE);
/*
Always Prompt for Password:

If you want to have attempted logons using an unknown user name still
prompt for a password, set this option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Always Prompt for Password",opt);
						if(i==0 && !(node_misc&NM_LOGON_P)) {
							node_misc|=NM_LOGON_P;
							changes=1; }
						else if(i==1 && (node_misc&NM_LOGON_P)) {
							node_misc&=~NM_LOGON_P;
							changes=1; }
                        break;
					case 16:
						i=1;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
						SETHELP(WHERE);
/*
Disable Local Inactivity Warning/Logoff:

Setting this option to Yes will disable the user inactivity warning
and automatic logoff for local logons.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Disable Local Inactivity",opt);
						if(i==0 && !(node_misc&NM_NO_INACT)) {
							node_misc|=NM_NO_INACT;
							changes=1; }
						else if(i==1 && (node_misc&NM_NO_INACT)) {
							node_misc&=~NM_NO_INACT;
							changes=1; }
                        break;
					case 17:
						i=1;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
						SETHELP(WHERE);
/*
Disable Local Keyboard (Entirely):

Setting this option to Yes will disable the local keyboard when the
BBS is running. Use this option only for the absolute highest degree
of local system security. If this option is set to Yes, the BBS cannot
be exited until the node is downed from another process or the machine
is rebooted.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Disable Local Keyboard (Entirely)",opt);
						if(i==0 && !(node_misc&NM_NO_LKBRD)) {
							node_misc|=NM_NO_LKBRD;
							changes=1; }
						else if(i==1 && (node_misc&NM_NO_LKBRD)) {
							node_misc&=~NM_NO_LKBRD;
							changes=1; }
                        break;
					case 18:
						i=1;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
						SETHELP(WHERE);
/*
Local System Protection:

When this option is set to Yes, all local system functions from the
Waiting for Call screen and Alt-Key combinations will require the user
to enter the system password.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Local System Protection (Password)",opt);
						if(i==0 && !(node_misc&NM_SYSPW)) {
							node_misc|=NM_SYSPW;
							changes=1; }
						else if(i==1 && (node_misc&NM_SYSPW)) {
							node_misc&=~NM_SYSPW;
							changes=1; }
                        break;
					case 19:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
						SETHELP(WHERE);
/*
Beep Locally:

If you want the local speaker to be disabled for online beeps, set this
option to No.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Beep Locally",opt);
						if(i==1 && !(node_misc&NM_NOBEEP)) {
							node_misc|=NM_NOBEEP;
							changes=1; }
						else if(i==0 && (node_misc&NM_NOBEEP)) {
							node_misc&=~NM_NOBEEP;
							changes=1; }
                        break;
					case 20:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
						SETHELP(WHERE);
/*
Allow 8-bit Remote Input During Logon:

If you wish to allow E-7-1 terminals to use this node, you must set this
option to No. This will also eliminate the ability of 8-bit remote users
to send IBM extended ASCII characters during the logon sequence.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Allow 8-bit Remote Input During Logon",opt);
						if(i==1 && !(node_misc&NM_7BITONLY)) {
							node_misc|=NM_7BITONLY;
							changes=1; }
						else if(i==0 && (node_misc&NM_7BITONLY)) {
							node_misc&=~NM_7BITONLY;
							changes=1; }
                        break;
					case 21:
						i=0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						savnum=1;
						SETHELP(WHERE);
/*
Reset Video Mode Between Calls:

If you wish to have SBBS reset the video mode between calls, set this
option to Yes. This is to reverse the effects of some external programs
that change the video mode without permission.
*/
						i=ulist(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Reset Video Mode Beteween Calls",opt);
						if(i==0 && !(node_misc&NM_RESETVID)) {
							node_misc|=NM_RESETVID;
							changes=1; }
						else if(i==1 && (node_misc&NM_RESETVID)) {
							node_misc&=~NM_RESETVID;
							changes=1; }
                        break;
						} }
			break;
		case 9:
			done=0;
			while(!done) {
				i=0;
				sprintf(opt[i++],"%-27.27s%s","Validation User"
					,node_valuser ? itoa(node_valuser,tmp,10) : "Nobody");
				sprintf(opt[i++],"%-27.27s%s","Screen Length"
					,node_scrnlen ? itoa(node_scrnlen,tmp,10)
					: "Auto-Detect");
				sprintf(opt[i++],"%-27.27s%s","Screen Blanker"
					,node_scrnblank ? itoa(node_scrnblank,tmp,10)
					: "Disabled");
				sprintf(opt[i++],"%-27.27s%u seconds","Semaphore Frequency"
					,node_sem_check);
				sprintf(opt[i++],"%-27.27s%u seconds","Statistics Frequency"
					,node_stat_check);
				sprintf(opt[i++],"%-27.27s%u seconds","Inactivity Warning"
					,sec_warn);
				sprintf(opt[i++],"%-27.27s%u seconds","Inactivity Disconnection"
					,sec_hangup);
				sprintf(tmp,"$%d.00",node_dollars_per_call);
				sprintf(opt[i++],"%-27.27s%s","Cost Per Call",tmp);
				sprintf(opt[i++],"%-27.27s%.40s","Daily Event",node_daily);
				sprintf(opt[i++],"%-27.27s%.40s","Control Directory",ctrl_dir);
				sprintf(opt[i++],"%-27.27s%.40s","Text Directory",text_dir);
				sprintf(opt[i++],"%-27.27s%.40s","Temporary Directory"
					,temp_dir);
				sprintf(opt[i++],"%-27.27s%.40s","Swap Directory"
					,node_swapdir);
				opt[i][0]=0;
				savnum=0;
SETHELP(WHERE);
/*
Node Advanced Options:

This is the advanced options menu for the selected node. The available
options are of an advanced nature and should not be modified unless you
are sure of the consequences and necessary preparation.
*/
				switch(ulist(WIN_T2B|WIN_RHT|WIN_ACT|WIN_SAV,2,0,60,&adv_dflt,0
					,"Advanced Options",opt)) {
                    case -1:
						done=1;
                        break;
					case 0:
						itoa(node_valuser,str,10);
SETHELP(WHERE);
/*
Validation User Number:

When a caller logs onto the system as New, he or she must send
validation feedback to the sysop. This feature can be disabled by
setting this value to 0, allowing new users to logon without sending
validation feedback. If you want new users on this node to be forced to
send validation feedback, set this value to the number of the user to
whom the feedback is sent. The normal value of this option is 1 for
user number one.
*/
						uinput(WIN_MID,0,13,"Validation User Number (0=Nobody)"
							,str,4,K_NUMBER|K_EDIT);
						node_valuser=atoi(str);
						break;
					case 1:
						itoa(node_scrnlen,str,10);
SETHELP(WHERE);
/*
Screen Length:

Synchronet automatically senses the number of lines on your display and
uses all of them. If for some reason you want to override the detected
screen length, set this value to the number of lines (rows) you want
Synchronet to use.
*/
						uinput(WIN_MID,0,14,"Screen Length (0=Auto-Detect)"
							,str,2,K_NUMBER|K_EDIT);
						node_scrnlen=atoi(str);
						break;
					case 2:
						itoa(node_scrnblank,str,10);
SETHELP(WHERE);
/*
Screen Blanker:

This option allows you to have this node blank its Waiting for Call
screen after this number of minutes of inactivity. Setting this option
to 0 disables the screen blanker.
*/
						uinput(WIN_MID|WIN_SAV,0,14
							,"Minutes Inactive Before Blanking Screen "
							"(0=Disabled)"
							,str,2,K_NUMBER|K_EDIT);
						node_scrnblank=atoi(str);
                        break;
					case 3:
						itoa(node_sem_check,str,10);
SETHELP(WHERE);
/*
Semaphore Check Frequency While Waiting for Call (in seconds):

This is the number of seconds between semaphore checks while this node
is waiting for a caller. Default is 60 seconds.
*/
						uinput(WIN_MID|WIN_SAV,0,14
							,"Seconds Between Semaphore Checks"
							,str,3,K_NUMBER|K_EDIT);
						node_sem_check=atoi(str);
                        break;
					case 4:
						itoa(node_stat_check,str,10);
SETHELP(WHERE);
/*
Statistics Check Frequency While Waiting for Call (in seconds):

This is the number of seconds between static checks while this node
is waiting for a caller. Default is 10 seconds.
*/
						uinput(WIN_MID|WIN_SAV,0,14
							,"Seconds Between Statistic Checks"
							,str,3,K_NUMBER|K_EDIT);
						node_stat_check=atoi(str);
                        break;
					case 5:
						itoa(sec_warn,str,10);
SETHELP(WHERE);
/*
Seconds Before Inactivity Warning:

This is the number of seconds the user must be inactive before a
warning will be given. Default is 180 seconds.
*/
						uinput(WIN_MID|WIN_SAV,0,14
							,"Seconds Before Inactivity Warning"
							,str,4,K_NUMBER|K_EDIT);
						sec_warn=atoi(str);
                        break;
					case 6:
						itoa(sec_hangup,str,10);
SETHELP(WHERE);
/*
Seconds Before Inactivity Disconnection:

This is the number of seconds the user must be inactive before they
will be automatically disconnected. Default is 300 seconds.
*/
						uinput(WIN_MID|WIN_SAV,0,14
							,"Seconds Before Inactivity Disconnection"
							,str,4,K_NUMBER|K_EDIT);
						sec_hangup=atoi(str);
                        break;
					case 7:
						itoa(node_dollars_per_call,str,10);
SETHELP(WHERE);
/*
Billing Node Cost Per Call:

If you have an automated billing phone number (usually area code 900
or prefix 976) and you wish to use this node as an automated billing
node, set this value to the dollar amount that is billed for the first
minute. Subsequent minutes should be charged the minimum amount as the
maximum connect time for a billing node is 90 seconds.

If this feature is used, when a caller connects to the node, he or she
will be notified of the eminent charge amount if they do not drop
carrier within the allotted free period. If the caller is still
connected after 30 seconds, he or she will be prompted to enter a valid
user name or number. After a valid user name or number has been entered,
the caller is disconnected and the entered user is immediately given the
purchased credits based on this dollar amount multiplied by the
Credits per Dollar system option.
*/
						uinput(WIN_MID,0,0,"Billing Node Cost Per Call "
							"(in dollars)",str,2,K_NUMBER|K_EDIT);
						node_dollars_per_call=atoi(str);
						break;
					case 8:
SETHELP(WHERE);
/*
Daily Event:

If you have an event that this node should run every day, enter the
command line for that event here.

An event can be any valid DOS command line. If multiple programs or
commands are required, use a batch file.

Remember: The %! command line specifier is an abreviation for your
		  configured EXEC directory path.
*/
						uinput(WIN_MID|WIN_SAV,0,10,"Daily Event"
							,node_daily,50,K_EDIT);
						break;
					case 9:
						strcpy(str,ctrl_dir);
						if(strstr(str,"\\CTRL\\")!=NULL)
							*strstr(str,"\\CTRL\\")=0;
SETHELP(WHERE);
/*
Control Directory Parent:

Your control directory contains important configuration and data files
that ALL nodes share. This directory MUST NOT be located on a RAM disk
or other volatile media.

This option allows you to change the parent of your control directory.
The \CTRL\ suffix (sub-directory) cannot be changed or removed.
*/
						if(uinput(WIN_MID|WIN_SAV,0,9,"Control Dir Parent"
							,str,50,K_EDIT|K_UPPER)>0) {
							if(str[strlen(str)-1]!='\\')
								strcat(str,"\\");
							strcat(str,"CTRL\\");
							strcpy(ctrl_dir,str); }
						break;
					case 10:
						strcpy(str,text_dir);
						if(strstr(str,"\\TEXT\\")!=NULL)
							*strstr(str,"\\TEXT\\")=0;
SETHELP(WHERE);
/*
Text Directory Parent:

Your text directory contains read-only text files. Synchronet never
writes to any files in this directory so it CAN be placed on a RAM
disk or other volatile media. This directory contains the system's menus
and other important text files, so be sure the files and directories are
moved to this directory if you decide to change it.

This option allows you to change the parent of your control directory.
The \TEXT\ suffix (sub-directory) cannot be changed or removed.
*/
						if(uinput(WIN_MID|WIN_SAV,0,10,"Text Dir Parent"
							,str,50,K_EDIT|K_UPPER)>0) {
							if(str[strlen(str)-1]!='\\')
								strcat(str,"\\");
							strcat(str,"TEXT\\");
							strcpy(text_dir,str); }
						break;
					case 11:
						strcpy(str,temp_dir);
SETHELP(WHERE);
/*
Temp Directory:

Your temp directory is where Synchronet stores files of a temporary
nature for this node. Each node MUST have its own unique temp directory.
This directory can exist on a RAM disk or other volatile media. For
the best batch upload performance, it should be located on the same
drive as the majority of your upload directories.
*/
						uinput(WIN_MID|WIN_SAV,0,10,"Temp Directory"
							,str,50,K_EDIT|K_UPPER);
						if(!strlen(str))
							umsg("Temp directory cannot be blank");
						else
							strcpy(temp_dir,str);
						break;
					case 12:
						strcpy(str,node_swapdir);
SETHELP(WHERE);
/*
Swap Directory:

Your swap directory is where Synchronet will swap out to if you have
swapping enabled, and it is necessary to swap to disk. The default is
the node directory. If you do specify a swap directory, it must not
be a relative path (i.e. "..\etc"). Specify the entire path and include
the drive letter specification (i.e. "D:\SBBS\SWAP").
*/
						uinput(WIN_MID|WIN_SAV,0,10,"Swap Directory"
							,str,50,K_EDIT|K_UPPER);
						if(str[0] && strcmp(str,".") && strcmp(str,".\\")
							&& (strstr(str,"..") || str[1]!=':'
							|| str[2]!='\\'))
							umsg("Must specify full path");
						else
							strcpy(node_swapdir,str);
						break; } }
			break;
		case 11:
SETHELP(WHERE);
/*
Wait for Call Number Key Commands:

Each of the number keys (0 through 9) can be assigned a command to
execute when entered at the wait for call screen. This is a list of
those keys and the assigned commands. Since Synchronet is still in
memory when these command are executed, the number keys should be used
for small program execution or command lines with small memory
requirements.

If you have a program or command line with large memory requirements,
use a Wait for Call Function Key Command.
*/
			j=0;
			while(1) {
				for(i=0;i<10;i++)
					sprintf(opt[i],"%d  %s",i,wfc_cmd[i]);
				opt[i][0]=0;
				savnum=0;

				i=ulist(WIN_T2B|WIN_RHT|WIN_ACT|WIN_SAV,2,0,50,&j,0
					,"Number Key Commands",opt);
				if(i==-1)
					break;
				sprintf(str,"%d Key Command Line",i);
				uinput(WIN_MID|WIN_SAV,0,0,str,wfc_cmd[i],50,K_EDIT); }
			break;
		case 12:
SETHELP(WHERE);
/*
Wait for Call Function Key Commands:

Each of the function keys (F1 through F12) can be assigned a command to
execute when entered at the wait for call screen. This is a list of
those keys and the currently assigned commands. Synchronet will shrink
to 16K of RAM before executing one of these command lines.

If you have a command line with small memory requirements, you should
probably use a Wait for Call Number Key Command for faster execution.
*/
			j=0;
			while(1) {
				for(i=0;i<12;i++)
					sprintf(opt[i],"F%-2d  %s",i+1,wfc_scmd[i]);
				opt[i][0]=0;
				savnum=0;
				i=ulist(WIN_T2B|WIN_RHT|WIN_ACT|WIN_SAV,2,0,50,&j,0
					,"Function Key (Shrinking) Commands",opt);
				if(i==-1)
					break;
				sprintf(str,"F%d Key Command Line",i+1);
				uinput(WIN_MID|WIN_SAV,0,0,str,wfc_scmd[i],50,K_EDIT); }
            break;
		case 10:
			done=0;
			while(!done) {
				i=0;
				sprintf(opt[i++],"%-27.27s%s","COM Port"
					,com_port ? itoa(com_port,tmp,10) : "Disabled");
				sprintf(opt[i++],"%-27.27s%u"
					,com_base && com_base<0x100 ? "COM Channel"
						:"UART IRQ Line"
					,com_irq);
				if(com_base==0xd)
					strcpy(str,"DigiBoard");
				else if(com_base==0xf)
					strcpy(str,"FOSSIL");
				else if(com_base==0xb)
					strcpy(str,"PC BIOS");
				else if(com_base==0xe)
					strcpy(str,"PS/2 BIOS");
				else
					sprintf(str,"%Xh",com_base);
				sprintf(opt[i++],"%-27.27s%s"
					,com_base<0x100 ? "COM Type":"UART I/O Address",str);
				sprintf(opt[i++],"%-27.27s%lu","DTE Rate",com_rate);
				sprintf(opt[i++],"%-27.27s%s","Fixed DTE Rate"
					,mdm_misc&MDM_STAYHIGH ? "Yes" : "No");
				if(!(mdm_misc&(MDM_RTS|MDM_CTS)))
					strcpy(str,"None");
				else {
					if(mdm_misc&MDM_CTS) {
						strcpy(str,"CTS");
						if(mdm_misc&MDM_RTS)
							strcat(str,"/"); }
					else str[0]=0;
					if(mdm_misc&MDM_RTS)
						strcat(str,"RTS"); }
				sprintf(opt[i++],"%-27.27s%s","Hardware Flow Control",str);
				sprintf(opt[i++],"%-27.27s%u ring%c","Answer After"
					,mdm_rings,mdm_rings>1 ? 's':' ');
				sprintf(opt[i++],"%-27.27s%u second%c","Answer Delay"
					,mdm_ansdelay,mdm_ansdelay>1 ? 's':' ');
				sprintf(opt[i++],"%-27.27s%s","Reinitialization Timer"
					,mdm_reinit ? itoa(mdm_reinit,tmp,10) : "Disabled");
				strcpy(opt[i++],"Result Codes...");
				strcpy(opt[i++],"Toggle Options...");
				strcpy(opt[i++],"Control Strings...");
				strcpy(opt[i++],"Auto-Configuration...");
				strcpy(opt[i++],"Import Configuration...");
				strcpy(opt[i++],"Export Configuration...");
				opt[i][0]=0;
				savnum=0;
SETHELP(WHERE);
/*
Modem Configuration Menu:

This menu contains the configuration options for this node's modem.
If you do not have a modem attached to this node or do not want to use
an attached modem, you can disable the Synchronet modem communications
by setting the COM Port to 0.

If your modem is listed in the Auto-Configuration, you should probably
just set the COM Port and run the auto-configuration for your modem
type.
*/
				switch(ulist(WIN_T2B|WIN_RHT|WIN_ACT|WIN_SAV,2,0,44,&mdm_dflt,0
					,"Modem Configuration",opt)) {
                    case -1:
						done=1;
						break;
					case 0:
						itoa(com_port,str,10);
SETHELP(WHERE);
/*
COM Port:

This is the serial communications port that the modem for this node is
connected to. If you do not have a modem connected to this node, or
do not wish to use a connected modem, you can disable the Synchronet
modem communications by setting this value to 0.
*/
						if(uinput(WIN_MID|WIN_SAV,0,10,"COM Port (0=Disabled)"
							,str,2,K_EDIT|K_NUMBER)==-1)
							break;
						com_port=atoi(str);
						if(!com_port)
							break;
						savnum=1;
						com_type();
						break;
					case 1:
						itoa(com_irq,str,10);
SETHELP(WHERE);
/*
UART IRQ Line or Channel Number:

If you are using a standard UART serial interface for this COM port,
this is the IRQ line that your COM Port's UART is using. If you have
configured your COM Port and selected the default IRQ and I/O address,
you should not need to change the value of this option.

If this COM port is accessed via Int 14h, this is the channel number
for this COM port (normally, the COM port number minus one).
*/
						uinput(WIN_MID|WIN_SAV,0,10
							,com_base && com_base<0x100 ? "Channel"
							: "UART IRQ Line"
							,str,2,K_EDIT|K_NUMBER);
						com_irq=atoi(str);
						break;
					case 2:
						savnum=1;
						if(com_type() || com_base<0x100)
							break;
SETHELP(WHERE);
/*
UART I/O Address in Hex:

This is the base I/O address of your COM Port's UART. If you have
configured your COM Port and selected the default IRQ and I/O address,
you should not need to change the value of this option. If this node's
COM Port's UART is using a non-standard I/O address, enter that address
(in hexadecimal) using this option.
*/
						itoa(com_base,str,16);
						strupr(str);
						uinput(WIN_MID|WIN_SAV,0,10
							,"UART I/O Address in Hex"
							,str,4,K_EDIT|K_UPPER);
						com_base=ahtoul(str);
                        break;
					case 3:
						savnum=1;
SETHELP(WHERE);
/*
UART (DTE) Rate:

This is the data transfer rate between your COM Port's UART (Data
Terminal Equipment) and your modem. This is NOT the connect rate of
your modem (Data Communications Equipment). Most high-speed (9600bps+)
modems use a fixed DTE rate that is higher than the highest DCE rate to
allow for data compression and error correction. This value should be
set to the highest DTE rate your modem supports. If you have a 1200 or
2400bps modem without data compression capabilities, this value should
be set to 1200 or 2400 respectively. If you have a high-speed modem,
refer to the modem's manual to find the highest supported DTE rate.
*/
						i=0;
						strcpy(opt[i++],"300");
						strcpy(opt[i++],"1200");
						strcpy(opt[i++],"2400");
						strcpy(opt[i++],"4800");
						strcpy(opt[i++],"9600");
						strcpy(opt[i++],"19200");
						strcpy(opt[i++],"38400");
						strcpy(opt[i++],"57600");
						strcpy(opt[i++],"115200");
						opt[i][0]=0;
						switch(com_rate) {
							default:
								i=0;
								break;
							case 1200:
								i=1;
								break;
							case 2400:
								i=2;
								break;
							case 4800:
								i=3;
								break;
							case 9600:
								i=4;
								break;
							case 19200:
								i=5;
								break;
							case 38400:
								i=6;
								break;
							case 57600:
								i=7;
								break;
							case 115200L:
								i=8;
								break; }
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0,"UART (DTE) Rate"
							,opt);
						if(i==-1)
							break;
						changes=1;
						switch(i) {
							default:
								com_rate=300;
								break;
							case 1:
								com_rate=1200;
								break;
							case 2:
								com_rate=2400;
								break;
							case 3:
								com_rate=4800;
								break;
							case 4:
								com_rate=9600;
								break;
							case 5:
								com_rate=19200;
								break;
							case 6:
								com_rate=38400;
								break;
							case 7:
								com_rate=57600;
								break;
							case 8:
								com_rate=115200L;
								break; }
						break;
					case 4:
						i=1;
						savnum=1;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
SETHELP(WHERE);
/*
Use Fixed DTE Rate:

If this node is using a modem with error correction or data compression
capabilities, set this option to Yes. If you are using a 2400bps or
slower modem, it is most likely this value should be set to No.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0,"Use Fixed "
							"DTE Rate",opt);
						if(i==0 && !(mdm_misc&MDM_STAYHIGH)) {
							mdm_misc|=MDM_STAYHIGH;
							changes=1; }
						else if(i==1 && mdm_misc&MDM_STAYHIGH) {
							mdm_misc&=~MDM_STAYHIGH;
							changes=1; }
						break;
					case 5:
						i=1;
						savnum=1;
						strcpy(opt[0],"Both");
						strcpy(opt[1],"CTS Only");
						strcpy(opt[2],"RTS Only");
						strcpy(opt[3],"None");
						opt[4][0]=0;
						i=0;
SETHELP(WHERE);
/*
Hardware Flow Control (CTS/RTS):

If your modem supports the use of hardware flow control via CTS/RTS
(Clear to Send/Request to Send), set this option to Both. If are using
a high-speed modem or a modem with data compression or error correction
capabilities, it is most likely this option should be set to Both. If
you are using a 2400bps or slower modem without data compression or
error correction capabilities, set this option to None.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0,"Hardware "
							"Flow Control (CTS/RTS)",opt);
						if(i==0
							&& (mdm_misc&(MDM_CTS|MDM_RTS))
									   !=(MDM_CTS|MDM_RTS)) {
							mdm_misc|=(MDM_CTS|MDM_RTS);
							changes=1; }
						else if(i==1
							&& (mdm_misc&(MDM_CTS|MDM_RTS))!=MDM_CTS) {
							mdm_misc|=MDM_CTS;
							mdm_misc&=~MDM_RTS;
							changes=1; }
						else if(i==2
							&& (mdm_misc&(MDM_CTS|MDM_RTS))!=MDM_RTS) {
							mdm_misc|=MDM_RTS;
							mdm_misc&=~MDM_CTS;
                            changes=1; }
						else if(i==3
							&& mdm_misc&(MDM_CTS|MDM_RTS)) {
							mdm_misc&=~(MDM_RTS|MDM_CTS);
                            changes=1; }
						break;

					case 6:
SETHELP(WHERE);
/*
Number of Rings to Answer After:

This is the number of rings to let pass before answering the phone.
*/
                        itoa(mdm_rings,str,10);
                        uinput(WIN_MID|WIN_SAV,0,0
                            ,"Number of Rings to Answer After"
                            ,str,2,K_EDIT|K_NUMBER);
                        mdm_rings=atoi(str);
                        if(!mdm_rings)
                            mdm_rings=1;
                        break;

					case 7:
SETHELP(WHERE);
/*
Seconds to Delay after Answer:

This is the length of time (in seconds) to delay after connection and
before the terminal detection sequence is trasmitted to the remote user
and the logon prompt is displayed.
*/
						itoa(mdm_ansdelay,str,10);
						uinput(WIN_MID|WIN_SAV,0,0
							,"Answer Delay (in seconds)"
							,str,4,K_EDIT|K_NUMBER);
						mdm_ansdelay=atoi(str);
						break;

					case 8:
SETHELP(WHERE);
/*
Minutes Between Reinitialization:

If you want your modem to be periodically reinitialized while waiting
for a caller, set this option to the maximum number of minutes between
initializations. Setting this value to 0 disables this feature.
*/
						itoa(mdm_reinit,str,10);
						uinput(WIN_MID|WIN_SAV,0,0
							,"Minutes Between Reinitialization (0=Disabled)"
							,str,4,K_EDIT|K_NUMBER);
						mdm_reinit=atoi(str);
						break;
					case 9:
						dflt=bar=0;
						while(1) {
							for(i=0;i<mdm_results;i++) {
								sprintf(opt[i],"%3u %5ubps %4ucps %s"
									,mdm_result[i].code,mdm_result[i].rate
									,mdm_result[i].cps,mdm_result[i].str); }
							opt[i][0]=0;
							i=WIN_ACT|WIN_SAV|WIN_RHT;
							if(mdm_results<MAX_OPTS)
								i|=WIN_INS;
							if(mdm_results>1)
								i|=WIN_DEL;
							savnum=1;
SETHELP(WHERE);
/*
Modem Result Codes:

This is the list of configured numeric connection result codes that this
node's modem supports. If this node is getting Invalid Result Code
errors when answer incoming calls, those result codes should be added
to this list. Refer to your modem's manual for a list of supported
result codes. Using the Auto-Configuration option automatically
configures this list for you.
To add a result code, hit  INS .

To delete a result code, select it using the arrow keys and hit  DEL .
*/
							i=ulist(i,0,0,34,&dflt,&bar
								,"Modem Result Codes",opt);
							if(i==-1)
								break;
							if((i&MSK_ON)==MSK_DEL) {
								i&=MSK_OFF;
								mdm_results--;
								while(i<mdm_results) {
									mdm_result[i]=mdm_result[i+1];
									i++; }
								changes=1;
								continue; }
							if((i&MSK_ON)==MSK_INS) {
								i&=MSK_OFF;
								if((mdm_result=(mdm_result_t *)REALLOC(
									mdm_result,sizeof(mdm_result_t)
									*(mdm_results+1)))==NULL) {
									errormsg(WHERE,ERR_ALLOC,nulstr
										,mdm_results+1);
									mdm_results=0;
									bail(1);
									continue; }
								for(j=mdm_results;j>i;j--)
									mdm_result[j]=mdm_result[j-1];
								mdm_result[i]=mdm_result[i+1];
								mdm_results++;
								changes=1;
								continue; }
							results(dflt); }
						break;
					case 10:	/* Toggle Options */
						dflt=0;
						while(1) {
							savnum=1;
							i=0;
							sprintf(opt[i++],"%-27.27s%s"
								,"Caller Identification"
								,mdm_misc&MDM_CALLERID ? "Yes" : "No");
							sprintf(opt[i++],"%-27.27s%s"
								,"Dumb Modem Connection"
								,mdm_misc&MDM_DUMB ? "Yes" : "No");
							sprintf(opt[i++],"%-27.27s%s"
								,"Drop DTR to Hang Up"
								,mdm_misc&MDM_NODTR ? "No" : "Yes");
							sprintf(opt[i++],"%-27.27s%s"
								,"Use Verbal Result Codes"
								,mdm_misc&MDM_VERBAL ? "Yes" : "No");
							sprintf(opt[i++],"%-27.27s%s"
								,"Allow Unknown Result Codes"
                                ,mdm_misc&MDM_KNOWNRES ? "No" : "Yes");
							opt[i][0]=0;
							i=ulist(WIN_SAV|WIN_ACT,0,0,0,&dflt,0
								,"Modem Toggle Options",opt);
							savnum=2;
							if(i==-1)
								break;
							if(i==0) {
								i=1;
								strcpy(opt[0],"Yes");
								strcpy(opt[1],"No");
								opt[2][0]=0;
SETHELP(WHERE);
/*
Caller Identification:

If your modem supports Caller ID and is configured to return caller
number delivery messages, set this option to Yes.

If this option is set to Yes, Synchronet will log caller number delivery
messages. If a caller attempts to connect with a number listed in the
file TEXT\CID.CAN, they will be displayed TEXT\BADCID.MSG (if it exists)
and disconnected. Each user will have their most recent caller ID number
stored in their note field.
*/
								i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
									,"Caller ID",opt);
								if(i==0 && !(mdm_misc&MDM_CALLERID)) {
									mdm_misc|=MDM_CALLERID;
									changes=1; }
								else if(i==1 && mdm_misc&MDM_CALLERID) {
									mdm_misc&=~MDM_CALLERID;
									changes=1; } }
							else if(i==1) {
								i=1;
								strcpy(opt[0],"Yes");
								strcpy(opt[1],"No");
								opt[2][0]=0;
SETHELP(WHERE);
/*
Dumb Modem Connection:

If this node is connected to a serial line through a dumb modem, set
this option to Yes to disable all modem commands and answer calls when
DCD is raised.
*/
								i=ulist(WIN_SAV|WIN_MID,0,0,0,&i,0
									,"Dumb Modem",opt);
								if(i==0 && !(mdm_misc&MDM_DUMB)) {
									mdm_misc|=MDM_DUMB;
									changes=1; }
								else if(i==1 && mdm_misc&MDM_DUMB) {
									mdm_misc&=~MDM_DUMB;
									changes=1; } }
							else if(i==2) {
								i=0;
								strcpy(opt[0],"Yes");
								strcpy(opt[1],"No");
								opt[2][0]=0;
SETHELP(WHERE);
/*
Drop DTR to Hang Up:

If you wish to have the BBS drop DTR to hang up the modem, set this
option to Yes. If this option is set to No, the Hang Up String is sent
to modem instead.
*/
								i=ulist(WIN_SAV|WIN_MID,0,0,0,&i,0
									,"Drop DTR to Hang Up",opt);
								if(i==0 && mdm_misc&MDM_NODTR) {
									mdm_misc&=~MDM_NODTR;
									changes=1; }
								else if(i==1 && !(mdm_misc&MDM_NODTR)) {
									mdm_misc|=MDM_NODTR;
                                    changes=1; } }
							else if(i==3) {
								i=1;
								strcpy(opt[0],"Yes");
								strcpy(opt[1],"No");
								opt[2][0]=0;
SETHELP(WHERE);
/*
Use Verbal Result Codes:

If you wish to have the BBS expect verbal (or verbose) result codes
from the modem instead of numeric (non-verbose) result codes, set this
option to Yes.

This option can be useful for modems that insist on returning multiple
result codes upon connection (CARRIER, PROTOCOL, etc) or modems that
do not report numeric result codes correctly. While this option is more
flexible than numeric result codes, it may not be as accurate in
describing the connection or estimating the CPS rate of the connection.

If this option is set to Yes, the configured result code list will not
be used (you may remove all entries from the list if you wish). The
connection rate, description, and estimated CPS will be automatically
generated by the BBS.
*/
								i=ulist(WIN_SAV|WIN_MID,0,0,0,&i,0
									,"Use Verbal Result Codes",opt);
								if(i==0 && !(mdm_misc&MDM_VERBAL)) {
									mdm_misc|=MDM_VERBAL;
									changes=1; }
								else if(i==1 && mdm_misc&MDM_VERBAL) {
									mdm_misc&=~MDM_VERBAL;
                                    changes=1; } }
							else if(i==4) {
								i=0;
								strcpy(opt[0],"Yes");
								strcpy(opt[1],"No");
								opt[2][0]=0;
SETHELP(WHERE);
/*
Allow Unknown Result Codes:

If you wish to have the BBS allow modem connections with an unknown
(un-configured) numeric result code by using the last known (configured)
result code information by default, set this option to Yes.

This option has no effect if Verbal Result Codes are used.
*/
								i=ulist(WIN_SAV|WIN_MID,0,0,0,&i,0
									,"Allow Unknown Result Codes",opt);
								if(i==0 && mdm_misc&MDM_KNOWNRES) {
									mdm_misc&=~MDM_KNOWNRES;
									changes=1; }
								else if(i==1 && !(mdm_misc&MDM_KNOWNRES)) {
									mdm_misc|=MDM_KNOWNRES;
                                    changes=1; } }
									}
						break;
					case 11:
SETHELP(WHERE);
/*
Modem Control Strings:

This menu contains a list of available modem control strings. It is usually
not necessary to modify these except under special circumstances.
*/
                        j=0;
                        while(1) {
                            i=0;
                            savnum=1;
							sprintf(opt[i++],"%-27.27s%.40s"
                                ,"Initialization String",mdm_init);
							sprintf(opt[i++],"%-27.27s%.40s"
                                ,"Special Init String",mdm_spec);
							sprintf(opt[i++],"%-27.27s%.40s"
                                ,"Terminal Init String",mdm_term);
							sprintf(opt[i++],"%-27.27s%.40s"
                                ,"Dial String",mdm_dial);
							sprintf(opt[i++],"%-27.27s%.40s"
                                ,"Off Hook String",mdm_offh);
							sprintf(opt[i++],"%-27.27s%.40s"
                                ,"Answer String",mdm_answ);
							sprintf(opt[i++],"%-27.27s%.40s"
								,"Hang Up String",mdm_hang);
							opt[i][0]=0;
                            j=ulist(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&j,0
                                ,"Modem Control Strings",opt);
                            if(j==-1)
                                break;
                            switch(j) {
                                case 0:
SETHELP(WHERE);
/*
Initialization String:

This is one of the strings of characters sent to your modem upon
modem initialization. If you find it necessary to send additional
commands to the modem during initialization, use the Special Init String
for that purpose.
*/
                                    uinput(WIN_MID|WIN_SAV,0,0
                                        ,"Initialization"
                                        ,mdm_init,50,K_EDIT|K_UPPER);
                                    break;
                                case 1:
SETHELP(WHERE);
/*
Special Init String:

This is an additional optional string of characters to be sent to your
modem during initialization. Many of the Auto-Configuration options
automatically set this string. It is used for sending commands that are
particular to this modem type. If you find it necessary to send
additional commands to your modem during initialization, use this option
for that purpose.
*/
                                    uinput(WIN_MID|WIN_SAV,0,0
                                        ,"Special Init"
                                        ,mdm_spec,50,K_EDIT|K_UPPER);
                                    break;
                                case 2:
SETHELP(WHERE);
/*
Terminal Init String:

This is the string of characters sent to your modem when terminal mode
is entered from the Synchronet wait for call screen.
*/
                                    uinput(WIN_MID|WIN_SAV,0,0
                                        ,"Terminal Init"
                                        ,mdm_term,50,K_EDIT|K_UPPER);
                                    break;
                                case 3:
SETHELP(WHERE);
/*
Dial String:

This is the string used to dial the modem in Synchronet Callback
(an optional callback verification module).
*/
                                    uinput(WIN_MID|WIN_SAV,0,0
										,"Dial String",mdm_dial,40
                                        ,K_EDIT|K_UPPER);
                                    break;
                                case 4:
SETHELP(WHERE);
/*
Off Hook String:

This is the string of characters sent to your modem to take it off hook
(make busy).
*/
                                    uinput(WIN_MID|WIN_SAV,0,0
                                        ,"Off Hook String",mdm_offh
                                        ,40,K_EDIT|K_UPPER);
                                    break;
                                case 5:
SETHELP(WHERE);
/*
Answer String:

This is the string of characters sent to your modem to answer an
incoming call.
*/
                                    uinput(WIN_MID|WIN_SAV,0,0
                                        ,"Answer String",mdm_answ,40
                                        ,K_EDIT|K_UPPER);
									break;
								case 6:
SETHELP(WHERE);
/*
Hang Up String:

This is the string of characters sent to your modem to hang up the
phone line (go on-hook).
*/
                                    uinput(WIN_MID|WIN_SAV,0,0
										,"Hang Up String",mdm_hang,40
                                        ,K_EDIT|K_UPPER);
                                    break;
									} }
						break;
					case 12:
						dflt=bar=0;
						while(1) {
							for(i=0;i<mdm_types;i++)
								strcpy(opt[i],mdm_type[i]);
							opt[i][0]=0;
							savnum=1;
SETHELP(WHERE);
/*
Auto-Configuration Modem Type:

This is the list of modem types currently supported by Synchronet's
automatic modem configuration feature. If your modem is listed, select
it using the arrow keys and hit  ENTER . If your modem is not listed,
you may want to try using the auto-configuration of a compatible modem
or one from the same manufacturer. If you have a 2400bps modem, select
Hayes SmartModem 2400. If you have a high-speed modem that is not
listed, you may want to try Generic 9600 or Generic 14400 and add
the additional high-speed result codes by hand. Refer to your modem's
manual for a list of result codes supported by your modem.
*/
							i=ulist(WIN_ACT|WIN_SAV|WIN_RHT,0,0,0
								,&dflt,&bar,"Modem Type",opt);
							if(i==-1)
								break;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							j=1;
							savnum=2;

							j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
								,"Continue with Modem Auto-Configuration",opt);
							if(!j) {
								mdm_cfg(i);
								changes=1;
								sprintf(str
									,"Modem configured as %s",mdm_type[i]);
								savnum=1;
								umsg(str); } }
						break;
					case 13:
SETHELP(WHERE);
/*
Import Modem Configuration File

Using this option, you can import modem settings from an MDM file in
your CTRL directory (i.e. To import from CTRL\MYMODEM.MDM, type MYMODEM
now).
*/
						i=changes;
						str[0]=0;
						uinput(WIN_MID|WIN_SAV,0,0,"MDM Filename",str
							,8,K_UPPER);
						if(str[0]) {
							FREE(mdm_result);
							mdm_result=NULL;

							mdm_answ[0]=0;
							mdm_hang[0]=0;
							mdm_dial[0]=0;
							mdm_offh[0]=0;
							mdm_term[0]=0;
							mdm_init[0]=0;
							mdm_spec[0]=0;
							mdm_results=0;

							if(exec_mdm(str))
								umsg("Imported");
							else {
								umsg("File Not Found");
								changes=i; } }
						else
							changes=i;
                        break;

					case 14:
SETHELP(WHERE);
/*
Export Current Modem Configuration to a File

Using this option, you can save the current modem settings to a file.
The file will be placed into the CTRL directory and will be given an
extension of MDM (i.e. To create CTRL\MYMODEM.MDM, type MYMODEM now).
*/
						i=changes;
						str[0]=0;
						uinput(WIN_MID|WIN_SAV,0,0,"MDM Filename",str
							,8,K_UPPER);
						if(str[0]) {
							if(export_mdm(str))
								umsg("Exported");
							else
								umsg("Error Exporting"); }
						changes=i;
						break;
						} } } }
}

void results(int i)
{
	char str[81];
	int n,dflt=0;

while(1) {
	n=0;
	sprintf(opt[n++],"%-22.22s%u","Numeric Code",mdm_result[i].code);
	sprintf(opt[n++],"%-22.22s%ubps","Modem Connect Rate",mdm_result[i].rate);
	sprintf(opt[n++],"%-22.22s%ucps","Modem Average CPS",mdm_result[i].cps);
	sprintf(opt[n++],"%-22.22s%s","Description",mdm_result[i].str);
	opt[n][0]=0;
	savnum=2;
SETHELP(WHERE);
/*
Result Code:

This is one of the configured connect result codes for your modem. The
available options are:

    Numeric Code       :  Numeric result code
    Modem Connect Rate :  Connect speed (in bps) for this result code
	Modem Average CPS  :  Average file transfer throughput in cps
    Description        :  Description of this connection type
*/
	switch(ulist(WIN_SAV|WIN_MID|WIN_ACT,0,0,35,&dflt,0,"Result Code",opt)) {
		case -1:
			return;
		case 0:
			sprintf(str,"%u",mdm_result[i].code);
SETHELP(WHERE);
/*
Numeric Result Code:

This is the numeric (as opposed to verbal) result code your modem will
send to your computer when it establishes a connection with another
modem with this connect type. Your modem must have the V0 setting
enabled to return numeric result codes.
*/
			uinput(WIN_L2R|WIN_SAV,0,15,"Numeric Result Code",str,3
				,K_EDIT|K_NUMBER);
			mdm_result[i].code=atoi(str);
			break;
		case 1:
SETHELP(WHERE);
/*
Connection (DCE) Rate:

This is the modem (DCE) rate established with this connect type. This is
NOT the DTE rate of your modem. As an example, a 14,400bps modem may
have a DTE rate of 38,400bps but a maximum DCE rate of 14,400bps.
*/
			sprintf(str,"%u",mdm_result[i].rate);
			uinput(WIN_L2R|WIN_SAV,0,15,"Connection (DCE) Rate",str,5
				,K_EDIT|K_NUMBER);
			mdm_result[i].rate=atoi(str);
			break;
		case 2:
SETHELP(WHERE);
/*
Average File Transfer CPS:

This is the average file transfer through-put (characters per second)
of connections with this result code. If you don't know the average
through-put, just divide the DCE Rate by ten for an approximation.
As an example, a 9600bps DCE Rate would have an approximate file
transfer through-put of 960cps.
*/
			sprintf(str,"%u",mdm_result[i].cps);
			uinput(WIN_L2R|WIN_SAV,0,15,"Average File Transfer CPS",str,5
				,K_EDIT|K_NUMBER);
			mdm_result[i].cps=atoi(str);
			break;
		case 3:
SETHELP(WHERE);
/*
Modem Description:

This is a description of the modem type used for a connection with
this result code. It can be up to eight characters long. This
description usually just contains the DCE rate and possibly the
protocol type. Such as 2400 for a 2400bps connection, 9600/V32
for a 9600bps V.32 connection or 9600/HST for a 9600bps HST connection.
*/
			uinput(WIN_L2R|WIN_SAV,0,15,"Modem Description"
				,mdm_result[i].str,LEN_MODEM,K_EDIT);
			break; } }
}
