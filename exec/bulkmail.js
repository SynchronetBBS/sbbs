// bulkmail.js

// Bulk email all users that match AR String input

// written by the hanged man, Solace BBS, solace.synchro.net

load("sbbsdefs.js");

const REVISION = "$Revision: 1.6 $".split(' ')[1];

print("Synchronet BulkMailer " + REVISION);

print("Enter ARS matches to send mail to or [ENTER] for everyone");

printf("ARS to match: "); 
if((ars=console.getstr(40,K_UPPER))==undefined)
	exit();

printf("\r\n\1y\1hSubject: ");

if((subj=console.getstr(70,K_LINE))=="")
	exit();

fname = system.temp_dir + "bulkmsg.txt";

file_remove(fname)

console.editfile(fname);

if(!file_exists(fname))	// Edit aborted
	exit();

file = new File(fname);
if(!file.open("rt")) {
    log(LOG_ERR,"!ERROR " + errno_str + " opening " + fname);
    exit();
}
msgtxt = lfexpand(file.read(file_size(fname)));
file.close();
delete file;

if(msgtxt == "")
    exit();

msgbase = new MsgBase("mail");
if(msgbase.open()==false) {
	log(LOG_ERR,"!ERROR " + msgbase.last_error);
	exit();
}

var u;	// user object

lastuser=system.lastuser;

var sent=0;
var rcpt_list=new Array();

for(i=1; i<=lastuser; i++)
{
    u = new User(i);

	if(u.settings&(USER_DELETED|USER_INACTIVE))
		continue;

    if(!u.compare_ars(ars))
		continue;

	/*checking for netmail forward */
	if(u.settings&USER_NETMAIL && u.netmail.length)
		hdr = { to_net_addr: u.netmail, to_net_type: netaddr_type(u.netmail) };
	else
		hdr = { to_ext: String(u.number) };
	
	hdr.to = u.alias;
	rcpt_list.push(hdr);

	printf("Sending mail to %s #%u\r\n", u.alias, i);
	log(format("Sending bulk mail message to: %s #%u", u.alias, i));
	sent++;
}

hdr = { from: system.operator, from_ext: "1", subject: subj };  

if(!msgbase.save_msg(hdr, msgtxt, rcpt_list))
	log(LOG_ERR,"!ERROR " + msgbase.last_error + "saving bulkmail message");

msgbase.close();

if(sent>1) {
	print("Sent bulk mail to " + sent + " users");
	log("Sent bulk mail message to " + sent + " users");
}
