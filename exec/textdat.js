load("sbbsdefs.js");	// SS_ABORT

for(i=1;!(bbs.sys_status&SS_ABORT);i++) {
	str = bbs.text(i);
	if(str==null)
		break;
	// console.line_counter=0; /* uncomment to display continuously */
	printf("\1n\1h%d: \1n%s\1n\r\n",i,str);
}