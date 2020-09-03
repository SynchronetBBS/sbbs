/*
 * New user sign-up form for Synchronet
 */

/* $Id: newuser.ssjs,v 1.44 2011/12/04 01:10:03 rswindell Exp $ */

/* ToDo: Deal with UQ_NODEF */

var send_user_info_to_sysop=false;
var send_password_via_email=true;

/* These two strings *must* be different! */
var required_str="*";
var optional_str="";

var sub="";

load("sbbsdefs.js");
load("user_info_to_sysop.js");
load("../web/lib/template.ssjs");

template.required=required_str;
template.date_format="MM/DD/YY";
if(system.settings & SYS_EURODATE)
    template.date_format="DD/MM/YY";
else
    template.date_format="MM/DD/YY";

var fields=new Array("alias","name","handle","netmail","address","location","zipcode","phone","birthdate","gender", "editor");
var required=new Array;
var maxlengths={alias:25,name:25,handle:8,netmail:60,address:30,location:30,zipcode:10,phone:12,birthdate:8,gender:1};
var err=0;
template.err_message='';
var gender='';
var pwchars='ACDEFHJKLMNPQRTUVWXY3479!?-=+&*()';	// Removed '%', rswindell
var newpw='';
template.title=system.name+" new user signup";

template.posted=http_request.query;
template.errs=new Object;

/* Get the current NUP... 3.12a had it in the wrong place */
template.nup=((this.login==undefined)?system.newuser_magic_word:system.newuser_password);

/* System is closed to new users */
if(system.settings & SYS_CLOSED) {
	if(do_header)
	    write_template("header.inc");
    write("This system is closed to new users, sorry.");
	if(do_footer)
	    write_template("footer.inc");
    exit(0);
}

/* Set up fields and required array */
if(template.nup != '') {
    template.nupstart='';
    required.push("nupass");
    template.nupass=required_str;
    template.nupend='';
}
else {
    template.nupstart='<!--';
    template.nupass=optional_str;
    template.nupend='-->';
}

template.RealNameIs="Real Name";
if(system.newuser_questions & UQ_ALIASES) {
    required.push("alias");
    template.alias_required=required_str;
}
else
    template.alias_required=optional_str;

if(system.newuser_questions & UQ_LOCATION) {
    required.push("location");
    template.loc_required=required_str;
}
else
    template.loc_required=optional_str;

if(system.newuser_questions & UQ_ADDRESS) {
    required.push("zipcode");
    template.zip_required=required_str;
    required.push("address");
    template.addr_required=required_str;
}
else {
    template.zip_required=optional_str;
    template.addr_required=optional_str;
}

if(system.newuser_questions & UQ_PHONE) {
    required.push("phone");
    template.ph_required=required_str;
}
else
    template.ph_required=optional_str;

if(system.newuser_questions & UQ_HANDLE) {
    required.push("handle");
    template.handle_required=required_str;
}
else
    template.handle_required=optional_str;

if(system.newuser_questions & UQ_SEX) {
    required.push("gender");
    template.sex_required=required_str;
}
else
    template.sex_required=optional_str;

if(system.newuser_questions & UQ_BIRTH) {
    required.push("birthdate");
    template.bd_required=required_str;
}
else
    template.bd_required=optional_str;

if(system.newuser_questions & UQ_REALNAME) {
    required.push("name");
    template.name_required=required_str;
}
else
    template.name_required=optional_str;

if(!(system.newuser_questions & UQ_ALIASES && system.newuser_questions & UQ_REALNAME)) {
    if(system.newuser_questions & UQ_COMPANY) {
        template.RealNameIs="Company Name";
        required.push("name");
        template.name_required=required_str;
    }
}

if(system.newuser_questions & UQ_NONETMAIL)
    template.email_required=optional_str;
else {
    required.push("netmail");
    template.email_required=required_str;
}

/* Plain GET with no query, just display the sign-up page */
if(http_request.method=='GET') {
    if(system.newuser_questions & UQ_SEX)
        template.gender_list='<select name="gender">\n<option value="M">Male</option>\n<option value="F">Female</option>\n</select>';
    else
        template.gender_list='<select name="gender">\n<option value="">Unspecified</option>\n<option value="M">Male</option>\n<option value="F">Female</option>\n</select>';
    template.editor_list=gen_editor_list(system.newuser_editor.toLowerCase());
    showform();
}
else {
    /* Create gender list drop-down */
    if(http_request.query["gender"] != undefined)
        gender=http_request.query["gender"].toString().toUpperCase();
    template.gender_list='<select name="gender">\n';
    if(!(system.newuser_questions & UQ_SEX))
        template.gender_list+='<option value=""'+(gender==''?' selected':'')+'>Unspecified</option>\n';
    template.gender_list+='<option value="M"'+(gender=='M'?' selected':'')+'>Male</option>\n';
    template.gender_list+='<option value="F"'+(gender=='F'?' selected':'')+'>Female</option>\n</select>';

    if(http_request.query["editor"] != undefined)
        template.editor_list=gen_editor_list(http_request.query.editor[0].toLowerCase());
    else
        template.editor_list=gen_editor_list(system.newuser_editor.toLowerCase());

    /* POST request... should be a valid application */
    for(field in fields) {
        if(http_request.query[fields[field]]==undefined) {
            err=1;
            template.errs[fields[field]]="MISSING";
            template.err_message+="Some fields missing from POST data... possible browser issue.\r\n";
        }
        else {
            if((system.newuser_questions & UQ_NOEXASC) && http_request.query[fields[field]][0].search(/[^\x20-\x7f]/)!=-1) {
                err=1;
                template.errs[fields[field]]="No Extended ASCII characters are allowed";
                template.err_message+="Extended ASCII characters used.\r\n";
            }
            if((!(system.newuser_questions & UQ_NOUPRLWR)) && fields[field] != "netmail") {
                http_request.query[fields[field]][0]=http_request.query[fields[field]][0].replace(/^(\s*)(.)/g,
                    function (matched, ws, ch, offset, s) {
                        return(ws+ch.toUpperCase());
                    });
                http_request.query[fields[field]][0]=http_request.query[fields[field]][0].replace(/(\s)(.)/g,
                    function (matched, ws, ch, offset, s) {
                        return(ws+ch.toUpperCase());
                    });
            }
        }
        if(err)
            showform()
    }

    if(template.nup != '') {
        if(http_request.query['nupass'][0]!=template.nup) {
            err=1;
            template.errs['nupass']='Incorrect';
            template.err_message+="Incorrect new user password, attempt has been logged!";
            log(LOG_WARNING, "!ERROR New user signup for "+http_request.query['alias'][0]+" from "+http_request.remote_host+" ("+http_request.remote_ip+") with incorrect new user password ("+http_request.query['nupass'][0]+")");
        }
    }

    if(system.newuser_questions & UQ_LOCATION && !(system.newuser_questions & UQ_NOCOMMAS)) {
        if(http_request.query["location"][0].search(/,./)==-1) {
            err=1;
            template.errs['location']='Format should be "City, State"';
            template.err_message+="Bad location format.\r\n";
        }
    }

    for(req in required) {
        if(http_request.query[required[req]]==undefined || http_request.query[required[req]]=='') {
            err=1;
            template.errs[required[req]]="REQUIRED";
            template.err_message="Please fill in the required fields.\r\n";
        }
    }
    for(len in maxlengths) {
        if(http_request.query[len]!=undefined && http_request.query[len].length>maxlengths[len]) {
            err=1;
            template.err_message+=len+" data too long (Length: "+http_request.query[len].length+" Max: "+maxlengths[len]+")\r\n";
            template.errs[titles[len]]='Max length: '+maxlengths[len];
        }
    }
    if(gender != 'M' && gender != 'F') {
		if(template.sex_required==required_str || template.sex_required != '') {
	        err=1;
    	    template.err_message+="Please specify gender (M or F).\r\n";
        	template.errs["gender"]="Male or Female";
		}
    }
    /* Validate date */
    if(http_request.query["birthdate"].toString().length<8) {
		if(template.bd_required==required_str || http_request.query["birthdate"].toString().length > 0) {
        	err=1;
        	if(system.settings & SYS_EURODATE)
        	    template.err_message+="Bad date format (example: 19/12/75)\r\n";
        	else
        	    template.err_message+="Bad date format (example: 12/19/75)\r\n";
		}
    }
    else {
        brokendate=http_request.query["birthdate"].toString().split('/');
        if(brokendate.length!=3) {
            err=1;
            template.err_message="Bad date format\r\n";
        }
        else {
            if(system.settings & SYS_EURODATE) {
                if((brokendate[1]<1 || brokendate[1]>12)
                        || (brokendate[0]<1 || brokendate[0]>31)
                        || (brokendate[2]<0 || brokendate[2]>99)) {
                    err=1;
                    template.err_message="Invalid Date\r\n";
                }
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
    }
	if(!system.check_name(http_request.query.alias)) {
		err=1;
        template.err_message+="Please choose a different alias.\r\n";
        template.errs["alias"]="Bad format";
    }
    else if((system.newuser_questions & UQ_DUPHAND) && system.matchuserdata(U_HANDLE,http_request.query["handle"])) {
        err=1;
        template.err_message+="Please choose a different chat handle.\r\n";
        template.errs["handle"]="Duplicate handle";
    }
    else if(system.matchuser(http_request.query["alias"])) {
        err=1;
        template.err_message+="Please choose a different alias.\r\n";
        template.errs["alias"]="Duplicate alias";
    }
    else if((system.newuser_questions & UQ_ALIASES) && (system.newuser_questions & UQ_REALNAME) && !system.check_name(http_request.query["name"])) {
		err=1;
		template.err_message+="Please enter your full real name.\r\n";
		template.errs["name"]="Bad format";
	}
	else if((system.newuser_questions & UQ_ALIASES) && (system.newuser_questions & UQ_REALNAME) && http_request.query["name"].toString().indexOf(' ') < 1) {
		err=1;
		template.err_message+="Please enter your first and last name.\r\n";
		template.errs["name"]="Invalid real name";
	}
    else if((system.newuser_questions & UQ_DUPREAL) && system.matchuserdata(U_NAME,http_request.query["name"])) {
        err=1;
        template.err_message+="A user "+(system.newuser_questions & UQ_COMPANY?"for that company":"with that name")+" already exists.\r\n";
        template.errs["name"]="Duplicate "+(system.newuser_questions & UQ_COMPANY?"company":"name");
    }

	/* TODO: reject prepended white-space in more fields (e.g. location, handle, email address) and trim trailing white-space */

    if(err)
        showform();

    newpw=genpass();

    /* Generate and send email */
    if(send_password_via_email && http_request.query.netmail != undefined && http_request.query.netmail != '') {
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
        msg += "User name: "+http_request.query.alias+"\n";
        msg += "Password: "+newpw+"\n";

        if(!msgbase.save_msg(hdrs,msg))  {
            err=1;
            template.err_message+=msgbase.last_error+"\r\n";
            showform();
        }
        msgbase.close();
    }

	try { 
		nuser=system.new_user(http_request.query.alias, client);
	} catch(e) {
		template.err_message = e;
		log(LOG_ERR,"JavaScript exception: " + e);
		showform();
	}
    nuser.name=http_request.query.name;
    nuser.handle=http_request.query.handle;
    nuser.netmail=http_request.query.netmail;
    nuser.address=http_request.query.address;
    nuser.location=http_request.query.location;
    nuser.zipcode=http_request.query.zipcode;
    nuser.birthdate=http_request.query.birthdate;
    nuser.gender=http_request.query.gender;
    nuser.security.password=newpw;
    nuser.phone=http_request.query.phone;
    nuser.editor=http_request.query.editor;

    if(send_user_info_to_sysop)
	sendUserInfoToSysop(
		nuser,
		system.name + " New User Information (web)",
		"" /* TODO: Create a comment textarea field, and put the result here */
	);

    template.title="New user created";
	if(do_header)
	    write_template("header.inc");
	if(do_topnav)
	    load(topnav_html);
	if(do_leftnav)
	    load(leftnav_html);
	if(do_rightnav)
		write_template("rightnav.inc");
    if(send_password_via_email && http_request.query.netmail != undefined && http_request.query.netmail != '')
        template.response=format("Your account has been created and the password has been mailed to: %s." ,http_request.query.netmail);
    else
        template.response=format("Your account has been created and the password is: %s " ,newpw);
    write_template("newuser_created.inc");
    if(do_footer)
	write_template("footer.inc");

prefs_dir=system.data_dir + 'user/';

prefsfile=new File(prefs_dir + '/'+format("%04d.html_prefs",nuser.number));
	if(prefsfile.open("w+",false)) {
		prefsfile.iniSetValue('Messaging', 'SortDate', 'descending');
		prefsfile.iniSetValue('Theme', 'CurrTheme', CurrTheme);
		prefsfile.close();
	}

}

function showform() {
	if(do_header)
	    write_template("header.inc");
	if(do_topnav)
	    load(topnav_html);
	if(do_leftnav)
    	load(leftnav_html);
	if(do_rightnav)
		write_template("rightnav.inc");
    write_template("newuser.inc");
	if(do_footer)
		write_template("footer.inc");
    exit(0);
}

function genpass() {
    var pw='';

    for(i=0;i<8;i++) {
        pw+=pwchars.substr(parseInt(Math.random() * pwchars.length),1);

    }
    return(pw);
}

/* This needs to filter based on access requirements (ARS)! */
function gen_editor_list(current) {
    var retval="";
    retval='<select name="editor">\n';
    retval+='<option value=""'+(current==''?' selected':'')+'>Internal Line Editor</option>\n';
	user.settings|=USER_ANSI;
	user.cached=true;
    for(ed in xtrn_area.editor) {
		if(!user.compare_ars(xtrn_area.editor[ed].ars))
			continue;
        retval+='<option value="'+ed+'"'+(current==ed?' selected':'')+'>'+xtrn_area.editor[ed].name+'</option>\n';
    }
    retval+='</select>';
    return(retval);
}
