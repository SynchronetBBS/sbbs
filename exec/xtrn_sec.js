// xtrn_sec.js

// Synchronet External Program Section
// Menus displayed to users via Terminal Server (Telnet/RLogin/SSH)

// To jump straight to a specific xtrn section, pass the section code as an argument

// $Id$

load("sbbsdefs.js");

/* text.dat entries */
load("text.js");

/* See if an xtrn section code was passed as an argument */
/* must parse argv before calling load() */
var xsec=-1;
{
    var i,j;
    for(i in argv) {
        for(j in xtrn_area.sec_list) {
            if(argv[i].toLowerCase()==xtrn_area.sec_list[j].code)
                xsec=j;
        }
    }
}

var options;
if((options=load({}, "modopts.js","xtrn_sec")) == null)
	options = {multicolumn: true, sort: false};	// default values

if(options.multicolumn == undefined)
	options.multicolumn = true;

function sort_by_name(a, b)
{ 
	if(a.name.toLowerCase()>b.name.toLowerCase()) return 1; 
	if(a.name.toLowerCase()<b.name.toLowerCase()) return -1;
	return 0;
} 

function external_program_menu(xsec)
{
    var i,j;

	while(bbs.online) {

	    if(user.security.restrictions&UFLAG_X) {
		    write(bbs.text(R_ExternalPrograms));
		    break;
	    }

		var prog_list=xtrn_area.sec_list[xsec].prog_list.slice();   /* prog_list is a possibly-sorted copy of xtrn_area.sec_list[x].prog_list */

		if(!prog_list.length) {
			write(bbs.text(NoXtrnPrograms));
			console.pause();
			break; 
		}

		if(file_exists(system.text_dir + "menu/xtrn" + (xtrn_area.sec_list[xsec].number+1) + ".asc")) {
			bbs.menu("xtrn" + (xtrn_area.sec_list[xsec].number+1)); 
		}
		else {
			if(options.sort)
				prog_list.sort(sort_by_name);
			printf(bbs.text(XtrnProgLstHdr),xtrn_area.sec_list[xsec].name);
			write(bbs.text(XtrnProgLstTitles));
			if(options.multicolumn && prog_list.length >= 10) {
				write("     ");
				write(bbs.text(XtrnProgLstTitles)); 
			}
			console.crlf();
			write(bbs.text(XtrnProgLstUnderline));
			if(options.multicolumn && prog_list.length >= 10) {
				write("     ");
				write(bbs.text(XtrnProgLstUnderline)); 
			}
			console.crlf();
			var n;
			if(options.multicolumn && prog_list.length >= 10)
				n=Math.floor(prog_list.length/2)+(prog_list.length&1);
			else
				n=prog_list.length;

			for(i=0;i<n && !console.aborted;i++) {
				printf(bbs.text(XtrnProgLstFmt),i+1
					,prog_list[i].name
					,prog_list[i].cost);

				if(options.multicolumn
					&& prog_list.length>=10) {
					j=Math.floor(prog_list.length/2)+i+(prog_list.length&1);
					if(j<prog_list.length) {
						write("     ");
						printf(bbs.text(XtrnProgLstFmt),j+1
							,prog_list[j].name
							,prog_list[j].cost); 
					}
				}

				console.crlf(); 
			}
			bbs.node_sync();
			console.mnemonics(bbs.text(WhichXtrnProg)); 
		}
		system.node_list[bbs.node_num-1].aux=0; /* aux is 0, only if at menu */
		bbs.node_action=NODE_XTRN;
		bbs.node_sync();
		if((i=console.getnum(prog_list.length))<1)
			break;
		i--;
		if(file_exists(system.text_dir + "menu/xtrn/" + prog_list[i].code + ".*")) {
			bbs.menu("xtrn/" + prog_list[i].code);
			console.line_counter=0;
		}
		if(options.clear_screen_on_exec)
			console.clear();
		bbs.exec_xtrn(prog_list[i].code); 

		if(prog_list[i].settings&XTRN_PAUSE)
			bbs.line_counter=2;	/* force a pause before CLS */
	}
}

function external_section_menu()
{
    var i,j;

    while(bbs.online) {

	    if(user.security.restrictions&UFLAG_X) {
		    write(bbs.text(R_ExternalPrograms));
		    break;
	    }

	    if(!xtrn_area.sec_list.length) {
		    write(bbs.text(NoXtrnPrograms));
		    break; 
	    }

	    var xsec=0;
	    var sec_list=xtrn_area.sec_list.slice();    /* sec_list is a possibly-sorted copy of xtrn_area.sect_list */

		system.node_list[bbs.node_num-1].aux=0; /* aux is 0, only if at menu */
		bbs.node_action=NODE_XTRN;
		bbs.node_sync();

		if(file_exists(system.text_dir + "menu/xtrn_sec.asc")) {
			bbs.menu("xtrn_sec");
			xsec=console.getnum(sec_list.length);
			if(xsec<=0)
				break;
			xsec--;
		}
		else {
	
			if(options.sort)
				sec_list.sort(sort_by_name);
			for(i in sec_list)
				console.uselect(Number(i),"External Program Section"
					,sec_list[i].name);
			xsec=console.uselect(); 
		}
	    if(xsec<0)
		    break;

        external_program_menu(sec_list[xsec].index);
    }
}

/* main: */
if(xsec >= 0)
    external_program_menu(xsec);
else {
    if(xtrn_area.sec_list.length == 1)
        external_program_menu(0);
    else
        external_section_menu();
}
