// xtrn_sec.js

// Synchronet External Program Section
// Menus displayed to users via Telnet/RLogin

// $Id$

load("sbbsdefs.js");

/* text.dat entries */
const R_ExternalPrograms	=123
const NoXtrnPrograms		=379
const XtrnProgLstHdr		=380
const XtrnProgLstTitles		=381
const XtrnProgLstUnderline	=382
const XtrnProgLstFmt		=383
const WhichXtrnProg			=384

var multicolumn = true;
var sort = false;

function sort_by_name(a, b)
{ 
	if(a.name.toLowerCase()>b.name.toLowerCase()) return 1; 
	if(a.name.toLowerCase()<b.name.toLowerCase()) return -1;
	return 0;
} 

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
	var sec_list=xtrn_area.sec_list.slice();
	if(sec_list.length > 1) {

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
	
			if(sort)
				sec_list.sort(sort_by_name);
			for(i in sec_list)
				console.uselect(Number(i),"External Program Section"
					,sec_list[i].name);
			xsec=console.uselect(); 
		}
	}
	if(xsec<0)
		break;

	xsec=sec_list[xsec].index;
	while(bbs.online) {
		var prog_list=xtrn_area.sec_list[xsec].prog_list.slice();

		if(!prog_list.length) {
			write(bbs.text(NoXtrnPrograms));
			console.pause();
			break; 
		}

		if(file_exists(system.text_dir + "menu/xtrn" + (xtrn_area.sec_list[xsec].number+1) + ".asc")) {
			bbs.menu("xtrn" + (xtrn_area.sec_list[xsec].number+1)); 
		}
		else {
			if(sort)
				prog_list.sort(sort_by_name);
			printf(bbs.text(XtrnProgLstHdr),xtrn_area.sec_list[xsec].name);
			write(bbs.text(XtrnProgLstTitles));
			if(multicolumn && prog_list.length >= 10) {
				write("     ");
				write(bbs.text(XtrnProgLstTitles)); 
			}
			console.crlf();
			write(bbs.text(XtrnProgLstUnderline));
			if(multicolumn && prog_list.length >= 10) {
				write("     ");
				write(bbs.text(XtrnProgLstUnderline)); 
			}
			console.crlf();
			var n;
			if(multicolumn && prog_list.length >= 10)
				n=Math.floor(prog_list.length/2)+(prog_list.length&1);
			else
				n=prog_list.length;

			var i,j;
			for(i=0;i<n;i++) {
				printf(bbs.text(XtrnProgLstFmt),i+1
					,prog_list[i].name
					,prog_list[i].cost);

				if(multicolumn
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
		if(file_exists(system.text_dir + "menu/xtrn/" + prog_list[i].code)) {
			menu("xtrn/" + prog_list[i].code);
			console.line_counter=0;
		}
		bbs.exec_xtrn(prog_list[i].code); 

		if(prog_list[i].settings&XTRN_PAUSE)
			bbs.line_counter=2;	/* force a pause before CLS */
	}
	if(xtrn_area.sec_list.length<2)
		break; 
}
