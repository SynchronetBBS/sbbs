/* notransit.js */

/* Removes MSG_INTRANSIT attribute from messages in mail database */

load("sbbsdefs.js");

mail = new MsgBase("mail");
if(!mail.open()) {
	alert(mail.last_error);
	exit();
}
for(i=0;i<mail.total_msgs;i++) {
	hdr = mail.get_msg_header(	/* by_offset:		*/	true, 
								/* offset:			*/	i, 
								/* expand_fields:	*/	false);
	printf("#%lu from: %-30s %08lx\r\n",hdr.number,hdr.from,hdr.netattr);
	if(hdr && hdr.netattr&MSG_INTRANSIT) {
		hdr.netattr&=~MSG_INTRANSIT;
		printf("Removing in-transit attribute from message #%lu\r\n",hdr.number);
		if(!mail.put_msg_header(true,i,hdr))
			alert(mail.last_error);
	}
}

mail.close();
