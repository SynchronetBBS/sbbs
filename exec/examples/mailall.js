// mailall.js

// Example script to email all users individualized message text
// Based on bulkmail.js

load("sbbsdefs.js");

const REVISION = "$Revision: 1.3 $".split(' ')[1];

print("Synchronet MailAll " + REVISION);

print("Enter ARS matches to send mail to or [ENTER] for everyone");

printf("ARS to match: "); 
if(js.global.console)
	ars=console.getstr(40,K_UPPER);
else
	ars=readln();
if(ars==undefined)
	exit();

printf("\r\n\1y\1hSubject: ");

if(js.global.console)
	subj=console.getstr(70,K_LINE);
else
	subj=readln();
if(subj=="")
	exit();

msgbase = new MsgBase("mail");
if(msgbase.open()==false) {
	log(LOG_ERR,"!ERROR " + msgbase.last_error);
	exit();
}

var u;	// user object

lastuser=system.lastuser;

var sent=0;

for(i=1; i<=lastuser; i++)
{
    u = new User(i);

	if(u.settings&(USER_DELETED|USER_INACTIVE))
		continue;

    if(!u.compare_ars(ars))
		continue;

	hdr = { from: system.operator, from_ext: "1", subject: subj };  

	/*checking for netmail forward */
	if(u.settings&USER_NETMAIL && u.netmail.length) {
		hdr.to_net_addr = u.netmail; 
		hdr.to_net_type = netaddr_type(u.netmail);
	} else
		hdr.to_ext = String(u.number);
	
	hdr.to = u.alias;

	printf("Sending mail to %s #%u\r\n", u.alias, i);
	log(format("Sending mail message to: %s #%u", u.alias, i));
	sent++;

	msgtxt = "Your message text goes here";
	// Note, you can include destination user information from the 'u' object.
	// For example:
	// msgtxt = "Your password is " + u.security.password;

	if(!msgbase.save_msg(hdr, msgtxt))
		log(LOG_ERR,"!ERROR " + msgbase.last_error + "saving message");

}

msgbase.close();

if(sent>1) {
	print("Sent mail to " + sent + " users");
	log("Sent mail message to " + sent + " users");
}
