/*
 * New user sign-up form for Synchronet
 */

/* $Id$ */

load("sbbsdefs.js");
load("html_inc/template.ssjs");

var fields=new Array("alias","name","handle","netmail","address","location","zipcode","phone","birthdate","gender");
var required=new Array("name","netmail","address","location","zipcode","phone","birthdate","gender");
var maxlengths={alias:25,name:25,handle:8,netmail:60,address:30,location:30,zipcode:10,phone:12,birthdate:8,gender:1};
var err=0;
template.err_message='';
var gender='';
var pwchars='ACDEFHJKLMNPQRTUVWXY3479!?-=+&%*()';
var newpw='';
template.title=system.name+" new user signup";

template.posted=http_request.query;
template.errs=new Object;

/* Plain GET with no query, just display the sign-up page */
if(http_request.method=='get') {
	template.gender_list='<select name="gender">\n<option value="M">Male</option>\n<option value="F">Female</option>\n</select>';
	showform();
}
else {
	/* Create gender list drop-down */
	if(http_request.query["gender"] != undefined)
		gender=http_request.query["gender"].toUpperCase();
	template.gender_list='<select name="gender">\n';
	template.gender_list+='<option value="M"'+(gender=='M'?' selected':'')+'>Male</option>\n';
	template.gender_list+='<option value="F"'+(gender=='F'?' selected':'')+'>Female</option>\n</select>';

	/* POST request... should be a valid application */
	for(field in fields) {
		if(http_request.query[fields[field]]==undefined) {
			template.gender_list='<select name="gender">\n<option value="M">Male</option>\n<option value="F">Female</option>\n</select>';
 		err=1;
			template.errs[fields[field]]="MISSING";
			template.err_message+="Some fields missing from POST data... possible browser issue.\r\n";
		}
		if(err)
			showform()
	}

	for(req in required) {
		if(http_request.query[required[req]]==undefined || http_request.query[required[req]]=='') {
			err=1;
			template.errs[required[req]]="REQUIRED";
			template.err_message="Please fill in the required fields\r\n";
		}
	}
	for(len in maxlengths) {
		if(http_request.query[len].length>maxlengths[len]) {
			err=1;
			template.err_message+=len+" data too long (Length: "+http_request.query[len].length+" Max: "+maxlengths[len]+")\r\n";
			template.errs[titles[len]]='Max length: '+maxlengths[len];
		}
	}
	if(gender != 'M' && gender != 'F') {
		err=1;
		template.err_message+="Please specify gender (M or F)\r\n";
		template.errs["gender"]="Male or Female";
	}
	/* Validate date */
	if(http_request.query["birthdate"].length<8) {
		err=1;
		template.err_message+="Bad date format (ie: 12/19/75)\r\n";
	}
	else {
		brokendate=http_request.query["birthdate"].split('/');
		if(brokendate.length!=3) {
			err=1;
			template.err_message="Bad date format\r\n";
		}
		else {
			if((brokendate[0]<1 || brokendate[0]>12)
					|| (brokendate[1]<1 || brokendate[1]>31)
					|| (brokendate[2]<0 || brokendate[2]>99)) {
				err=1;
				template.err_message="Invalid Date\r\n";
			}
		}
	}
	if(system.new_user_questions & UQ_DUPHAND & system.matchuserdata(50,http_request.query["handle"])) {
		err=1;
		template.err_message+="Please choose a different chat handle\r\n";
		template.errs["handle"]="Duplicate handle";
	}
	if(system.matchuser(http_request.query["alias"])) {
		err=1;
		template.err_message+="Please choose a different alias.\r\n";
		template.errs["alias"]="Duplicate alias";
	}
	if(system.matchuser(http_request.query["name"])) {
		err=1;
		template.err_message+="A user with that name already exists.\r\n";
		template.errs["name"]="Duplicate name";
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
		template.err_message+="Cannot mail password to new email address!\r\n";
		showform();
	}
	hdrs.from=system.name;
	hdrs.from_net_addr='sysop@'+system.inet_addr;
	hdrs.from_net_type=NET_INTERNET;
	hdrs.subject="New user signup";
	var msgbase = new MsgBase("mail");
	if(msgbase.open!=undefined && msgbase.open()==false) {
		err=1;
		template.err_message+=msgbase.last_error+"\r\n";
		showform();
	}
	var msg="Your account on "+system.name+" has been created!\n\n";
	msg += "User name: "+http_request.query.name+"\n";
	msg += "Password: "+newpw+"\n";

	if(!msgbase.save_msg(hdrs,msg))  {
		err=1;
		template.err_message+=msgbase.last_error+"\r\n";
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

	template.title="New user created";
	write_template("header.inc");
	write("Your account has been created and the password has been mailed to: "+http_request.query.netmail);
	write_template("footer.inc");
}

function showform() {
	write_template("header.inc");
	write_template("newuser.inc");
	write_template("footer.inc");
	exit(0);
}

function genpass() {
	var pw='';

	for(i=0;i<8;i++) {
		pw+=pwchars.substr(random(pwchars.length),1);
	}
	return(pw);
}
