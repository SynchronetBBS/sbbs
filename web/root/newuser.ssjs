/*
 * New user sign-up form for Synchronet
 */

load("sbbsdefs.js");

var fields=new Array("alias","name","handle","netmail","address","location","zipcode","phone","birthdate","gender");
var required=new Array("name","netmail","address","location","zipcode","phone","birthdate","gender");
var titles=new Array();
var maxlengths={alias:25,name:25,handle:8,netmail:60,address:30,location:30,zipcode:10,phone:12,birthdate:8,gender:1};
var types={alias:"text",name:"text",handle:"text",netmail:"text",address:"text",location:"text",zipcode:"text",phone:"text",birthdate:"text",gender:"text"};
var err=0;
var err_message='';
var gender='';
var pwchars='ACDEFHJKLMNPQRTUVWXY3479!?-=+&%*()';
var newpw='';

/* Modify this to add/remove required fields */
titles["alias"]="Alias";
titles["name"]="Real Name";
titles["handle"]="Chat Handle";
titles["netmail"]="E-Mail Address";
titles["address"]="Mailing Address";
titles["location"]="City and Province/State";
titles["zipcode"]="Postal/Zip Code";
titles["phone"]="Phone Number (###-###-####)";
titles["birthdate"]="Birthdate (MM/DD/YY)";
titles["gender"]="Gender (M/F)";

/* Plain GET with no query, just display the sign-up page */
if(http_request.method=='GET') {
	showform();
}
else {
	/* POST request... should be a valid application */
	for(req in required) {
		if(http_request.query[required[req]]==undefined || http_request.query[required[req]]=='') {
			err=1;
			titles[required[req]]='<font color="RED">REQUIRED</font>: '+titles[required[req]];
			err_message+="Please fill in thre required fields<br>";
		}
	}
	for(field in fields) {
		if(http_request.query[fields[field]]==undefined) {
			err=1;
			err_message+="Some fields missing from POST data... possible browser issue.<br>";
		}
	}
	for(len in maxlengths) {
		if(http_request.query[len].length>maxlengths[len]) {
			err=1;
			err_message+=len+" data too long (Length: "+http_request.query[len].length+" Max: "+maxlengths[len]+")<br>";
			titles[len]+=' (Max length: '+maxlengths[len]+') ';
		}
	}
	gender=http_request.query["gender"].toUpperCase();
	if(gender != 'M' && gender != 'F') {
		err=1;
		err_message+="Please enter gender (M or F)<br>";
	}
	/* Validate date */
	if(http_request.query["birthdate"].length<8) {
		err=1;
		err_message="Bad date format (ie: 01/01/01)<br>";
	}
	else {
		brokendate=http_request.query["birthdate"].split('/');
		if(brokendate.length!=3) {
			err=1;
			err_message="Bad date format<br>";
		}
		else {
			if((brokendate[0]<1 || brokendate[0]>12)
					|| (brokendate[1]<1 || brokendate[1]>31)
					|| (brokendate[2]<0 || brokendate[2]>99)) {
				err=1;
				err_message="Invalid Date<br>";
			}
		}
	}
	if(system.new_user_questions & UQ_DUPHAND & system.matchuserdata(50,http_request.query["handle"])) {
		err=1;
		err_message+="Please choose a different chat handle<br>";
	}
	if(system.matchuser(http_request.query["alias"])) {
		err=1;
		err_message+="Please choose a different alias.<br>";
	}
	if(system.matchuser(http_request.query["name"])) {
		err=1;
		err_message+="A user with that name already exists.<br>";
	}
	newpw=genpass();
	if(err) {
		showform();
	}

	/* Generate and send email */
	var hdrs = new Object;
	hdrs.to=http_request.query.name;
	hdrs.to_net_type=netaddr_type(http_request.query.netmail);
	if(hdrs.to_net_type!=NET_NONE) {
		hdrs.to_net_addr=http_request.query.netmail;
	}
	else {
		err=1;
		err_message+="Cannot mail password to new email address!<br>";
		showform();
	}
	hdrs.from=system.name;
	hdrs.from_net_addr='sysop@'+system.inet_addr;
	hdrs.subject="New user signup";
	var msgbase = new MsgBase("mail");
	if(msgbase.open!=undefined && msgbase.open()==false) {
		err=1;
		err_message+=msgbase.last_error+"<br>";
		showform();
	}
	var msg="Your account on "+system.name+" has been created!\n\n";
	msg += "User name: "+http_request.query.name+"\n";
	msg += "Password: "+newpw+"\n";

	if(!msgbase.save_msg(hdrs,msg))  {
		err=1;
		err_message+=msgbase.last_error+"<br>";
		showform();
	}
	msgbase.close();

	nuser=system.new_user(http_request.query.name);
	nuser.name=http_request.query.name;
	nuser.alias=http_request.query.alias;
	nuser.handle=http_request.query.handle;
	nuser.netmail=http_request.query.netmail;
	nuser.address=http_request.query.address;
	nuser.location=http_request.query.location;
	nuser.zipcode=http_request.query.zipcode;
	nuser.birthdate=http_request.query.birthdate;
	nuser.gender=http_request.query.gender;
	nuser.security.password=newpw;
	nuser.phone=http_request.query.phone;
	
	write('<HTML><HEAD><TITLE>User Created!</TITLE></HEAD>');
	write("<BODY>");
	write("Your accound has been created and the password has been mailed to: "+http_request.query.netmail);
	write('<br><br><a href="http://www.synchro.net/"><img src="graphics/sync_pb.gif" border=0></a>');
	write("</BODY></HTML>");
}

function showform() {
	write('<HTML>');
	write('<HEAD>');
	write(' <TITLE>New User Signup</TITLE>');
	write('</HEAD>');
	write('<HTML>');
	write('<FORM ACTION="newuser.ssjs" METHOD="POST">');
	write('<CENTER>');
	if(err) {
		write(err_message);
	}
	write('<TABLE>');
	for(field in fields) {
		write('<TR><TD align="right">'+titles[fields[field]]+':</TD><TD align="left"><INPUT TYPE="'+types[fields[field]]+'" NAME="'+fields[field]+'" MAXLENGTH="'+maxlengths[fields[field]]+'" VALUE="'+(http_request.query[fields[field]]==undefined?'':http_request.query[fields[field]])+'"></TD></TR>');
	}
	write('</TABLE>');
	write('<INPUT TYPE="SUBMIT">');
	write('</FORM>');
	write('</CENTER>');
	write('<br><br><a href="http://www.synchro.net/"><img src="graphics/sync_pb.gif" border=0></a>');
	write('</BODY>');
	write('</HTML>');
	exit(0);
}

function genpass() {
	var pw='';

	for(i=0;i<8;i++) {
		pw+=pwchars.substr(random(pwchars.length),1);
	}
	return(pw);
}
