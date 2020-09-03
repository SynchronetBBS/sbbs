// Basic FormMail script, emulates Matt Wright's FormMail.pl script

// $Id: formmail.ssjs,v 1.17 2005/03/23 10:29:46 rswindell Exp $

load("sbbsdefs.js");

var log_results = true;
var log_request = true;
var referers = [];
var recipients = [ "sysop" ];
var disabled = false;

var cfg_file = new File(file_cfgname(system.ctrl_dir, "formmail.ini"));
if(cfg_file.open("r")) {
	disabled = cfg_file.iniGetValue(null,"Disabled",disabled);
	log_results = cfg_file.iniGetValue(null,"LogResults",log_results);
	log_request = cfg_file.iniGetValue(null,"LogRequest",log_request);
	referers = cfg_file.iniGetValue(null,"Referers",referers);
	recipients = cfg_file.iniGetValue(null,"Recipients",recipients);
	cfg_file.close();
}

// List of supported 'config' form fields (not included in body text by default)
var config_fields = {
	// FormMail.pl
	recipient:1,
	subject:1,
	email:1,
	realname:1,
	sort:1,
	required:1,
	missing_fields_redirect:1,
	env_report:1,
	print_config:1,
	print_blank_fields:1,
	redirect:1,
	title:1,
	bgcolor:1,
	background:1,
	text_color:1,
	link_color:1,
	vlink_color:1,
	alink_color:1,
	return_link_url:1,
	return_link_title:1,

	// formmail.ssjs
	value_fmt_string:1,
};

// Parse config fields and set defaults
var value_fmt_string = "%-10s = %s";
if(http_request.query.value_fmt_string
	&& strip_ctrl(http_request.query.value_fmt_string).length)
	value_fmt_string = strip_ctrl(http_request.query.value_fmt_string);

var print_blank_fields = false;
if(http_request.query.print_blank_fields)
	print_blank_fields = Boolean(http_request.query.print_blank_fields[0]);

var print_config = [];
if(http_request.query.print_config)
	print_config = http_request.query.print_config[0].split(/\s*,\s*/);

var return_link_url = http_request.header.referer;
if(http_request.query.return_link_url && http_request.query.return_link_url.length)
	return_link_url = strip_ctrl(http_request.query.return_link_url);

var return_link_title = "Click here to return to " + return_link_url.toString().italics();
if(http_request.query.return_link_title && http_request.query.return_link_title.length)
	return_link_title = strip_ctrl(http_request.query.return_link_title);

var title = "Thank You";
if(http_request.query.title)
	title = http_request.query.title;

// Test if script disabled here
if(disabled)
	results(LOG_WARNING,"This script has been disabled by the system operator");

// Test for required form fields here
var redirect;
if(http_request.query.redirect)
   redirect = strip_ctrl(http_request.query.redirect[0]);

if(http_request.query.required) {
	var required = http_request.query.required[0].split(/\s*,\s*/);
	var i;
	var missing = [];
	for(i in required) {
		if(!http_request.query[required[i]] || !http_request.query[required[i]][0].length)
			missing.push(required[i]);
	}
	if(missing.length)
		results(LOG_ERR,"Missing the following form fields: " + missing.join(", "), true);
}

// Test for allowed 'referer' here
if(referers.length && !find(http_request.header.referer,referers))
	results(LOG_ERR,"Invalid referer value: " + http_request.header.referer);

// Dump an object to file
function dump(obj, name, file)
{
	var i;

	for(i in obj) {
		if(typeof(obj[i])=="object")
			dump(obj[i], name +'.'+ i, file);
		else {
			if(!obj[i].length)
				continue;
			file.write('\t');
			if(obj.length!=undefined)
				file.write(name +'['+ i +'] = ');
			else
				file.write(name +'.'+ i +' = ');
			file.writeln(obj[i]);
		}
	}
}

// Send HTTP/HTML results here
function results(level, text, missing)
{
	if(level<=LOG_ERR)
		text = "!ERROR: ".bold() + text;
	else if(level==LOG_WARNING)
		text = "!WARNING: ".bold() + text;

	var plain_text = text.replace(/<[^>]*>/g,"");	// strip HTML tags

	log(level,"FormMail: " + plain_text);

	if(log_results) {
		var fname = system.logs_dir + "formmail.log";
		var file = new File(fname);
		if(file.open("a")) {
			file.printf("%s  %s\n",strftime("%b-%d-%y %H:%M"),plain_text);
			if(log_request)
				dump(http_request,"http_request",file);
			file.close();
		}
	}

	if(missing && http_request.query.missing_fields_redirect) {
		http_reply.status='307 Temporary Redirect';
		http_reply.header.location=http_request.query.missing_fields_redirect;
		exit();
	}

	if(redirect) {
		http_reply.status='307 Temporary Redirect';
		http_reply.header.location=redirect;
		exit();
	}

	writeln("<html>");
	writeln("<head>");
	writeln("<title>" + title + "</title>");
	writeln("</head>");

	writeln("<body" + body_attributes() + ">");

	writeln("<center>");
	writeln(text);
	writeln("<p>");
	writeln(return_link_title.link(return_link_url));
	writeln("</body>");
	writeln("</html>");
	exit();
}

// Return the optionally-configured HTML body attributes
function body_attributes()
{
	var i;
	var result='';

	for(i in {bgcolor:1, background:1, link_color:1
		,vlink_color:1, alink_color:1, text_color:1} ) {
		if(http_request.query[i])
			result+=format(' %s="%s"', i, http_request.query[i]);
	}
	return result;
}

// Search an array for a specific value or regexp match
function find(val, list)
{
	var n;

	for(n in list) {
		if(list[n].toLowerCase()==val.toLowerCase() 
			|| val.search(new RegExp(list[n] +'$', 'i'))==0)
			return(true);
	}
	return(false);
}

// Open mail database here
var msgbase=new MsgBase("mail");
if(!msgbase.open())
	results(LOG_ERR,format("%s opening mail base", msgbase.error));

//------------------------
// Build the body text
//------------------------
var i;
var body="Form fields follow:\r\n\r\n";
for(i in http_request.query) {
	if(config_fields[i] && !find(i,print_config))
		continue;
	if(!print_blank_fields && !http_request.query[i].toString().length)
		continue;
	body += format(value_fmt_string, i, http_request.query[i]) + "\r\n";
}

body+=format("\r\nvia %s\r\nat %s [%s]\r\n"
			 ,http_request.header['user-agent']
			 ,http_request.remote_host, http_request.remote_ip);

//------------------------
// Build the e-mail header
//------------------------
var hdr = { from: 'FormMail',
			subject: 'WWW Form Submission' };

// Use form-specified message subject
if(http_request.query.subject)		
	hdr.subject			=strip_ctrl(http_request.query.subject[0]);

// Use form-specified email address
if(http_request.query.email && http_request.query.email.toString().length) {
	hdr.from_net_addr	=strip_ctrl(http_request.query.email);
	hdr.from			=strip_ctrl(http_request.query.email);
}

// Use form-specified real name
if(http_request.query.realname && http_request.query.realname.toString().length)
	hdr.from			=strip_ctrl(http_request.query.realname);

//-------------------------
// Build the recipient list
//-------------------------
var recipient = ['Sysop'];

// Use form-specified recipient list
if(http_request.query.recipient)
	recipient=http_request.query.recipient[0].split(/\s*,\s*/);

var rcpt_list = [];
for(i in recipient) {

	if(recipients.length && !find(recipient[i],recipients))
		results(LOG_ERR,"invalid recipient: " + recipient[i]);

	var rcpt  = { to: strip_ctrl(recipient[i]) };

	rcpt.to_net_type=netaddr_type(rcpt.to);
	if(rcpt.to_net_type!=NET_NONE)
		rcpt.to_net_addr=rcpt.to;
	else {
		var usrnum=system.matchuser(rcpt.to);
		if(usrnum==0)
			results(LOG_ERR,"bad local username specified: " + rcpt.to);
		rcpt.to_ext=usrnum;
	}
	rcpt_list.push(rcpt);
}

// Save and send e-mail message(s)
if(!msgbase.save_msg(hdr,client,body,rcpt_list))
	results(LOG_ERR,format("%s saving message", msgbase.error));

results(LOG_INFO,"E-mail sent from " + hdr.from + " to " + recipient.toString().italics().bold() + " successfully.");
