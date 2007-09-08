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
	if(xtrn_area.sec_list.length > 1) {
		if(file_exists(system.text_dir + "menu/xtrn_sec.asc")) {
			bbs.menu("xtrn_sec");
			xsec=console.getnum(xtrn_area.sec_list.length);
			if(xsec<=0)
				break;
			xsec--;
		}
		else {
			for(i in xtrn_area.sec_list)
				console.uselect(Number(i),"External Program Section"
					,xtrn_area.sec_list[i].name);
			xsec=console.uselect(); 
		}
	}
	if(xsec<0)
		break;

	while(bbs.online) {

		if(!xtrn_area.sec_list[xsec].prog_list.length) {
			write(bbs.text(NoXtrnPrograms));
			console.pause();
			break; 
		}

		if(file_exists(system.text_dir + "menu/xtrn" + (xtrn_area.sec_list[xsec].number+1) + ".asc")) {
			bbs.menu("xtrn" + (xtrn_area.sec_list[xsec].number+1)); 
		}
		else {
			printf(bbs.text(XtrnProgLstHdr),xtrn_area.sec_list[xsec].name);
			write(bbs.text(XtrnProgLstTitles));
			if(multicolumn && xtrn_area.sec_list[xsec].prog_list.length >= 10) {
				write("     ");
				write(bbs.text(XtrnProgLstTitles)); 
			}
			console.crlf();
			write(bbs.text(XtrnProgLstUnderline));
			if(multicolumn && xtrn_area.sec_list[xsec].prog_list.length >= 10) {
				write("     ");
				write(bbs.text(XtrnProgLstUnderline)); 
			}
			console.crlf();
			var n;
			if(multicolumn && xtrn_area.sec_list[xsec].prog_list.length >= 10)
				n=Math.floor(xtrn_area.sec_list[xsec].prog_list.length/2)+(xtrn_area.sec_list[xsec].prog_list.length&1);
			else
				n=xtrn_area.sec_list[xsec].prog_list.length;

			var i,j;
			for(i=0;i<n;i++) {
				printf(bbs.text(XtrnProgLstFmt),i+1
					,xtrn_area.sec_list[xsec].prog_list[i].name
					,xtrn_area.sec_list[xsec].prog_list[i].cost);

				if(multicolumn
					&& xtrn_area.sec_list[xsec].prog_list.length>=10) {
					j=Math.floor(xtrn_area.sec_list[xsec].prog_list.length/2)+i+(xtrn_area.sec_list[xsec].prog_list.length&1);
					if(j<xtrn_area.sec_list[xsec].prog_list.length) {
						write("     ");
						printf(bbs.text(XtrnProgLstFmt),j+1
							,xtrn_area.sec_list[xsec].prog_list[j].name
							,xtrn_area.sec_list[xsec].prog_list[j].cost); 
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
		if((i=console.getnum(xtrn_area.sec_list[xsec].prog_list.length))<1)
			break;
		i--;
		if(file_exists(system.text_dir + "menu/xtrn/" + xtrn_area.sec_list[xsec].prog_list[i].code)) {
			menu("xtrn/" + xtrn_area.sec_list[xsec].prog_list[i].code);
			console.line_counter=0;
		}
		bbs.exec_xtrn(xtrn_area.sec_list[xsec].prog_list[i].code); 

		if(xtrn_area.sec_list[xsec].prog_list[i].settings&XTRN_PAUSE)
			bbs.line_counter=2;	/* force a pause before CLS */
	}
	if(xtrn_area.sec_list.length<2)
		break; 
}
