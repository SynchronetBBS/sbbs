// userlist.js

// A sample user listing script for Synchronet v3.1+

// $Id: userlist.ssjs,v 1.16 2006/02/25 21:40:35 runemaster Exp $

http_reply.fast=true;
load("sbbsdefs.js");
load("../web/lib/template.ssjs");

var show_qnet=false;

var sub="";

if(system.lastuser==undefined)	/* v3.10 */
	lastuser=system.stats.total_users;
else							/* v3.11 */
	lastuser=system.lastuser;
var u = new User(1);
template.users = new Array;
template.title = system.name+ " - User List";

for(i=1;i<=lastuser;i++) {
	usr=new Object;
	u.number=i;
	if(u.settings&USER_DELETED)
		continue;
	if(!show_qnet && (u.security.restrictions & UFLAG_Q))
		continue;
	if(u.security.restrictions & UFLAG_G) /* Don't Show Guest Account in User List */
		continue;
	usr.alias=u.alias.toString();
	usr.alias='<a href="/members/viewprofile.ssjs?showuser=' + u.number + '">' + usr.alias + '</a>';
	usr.location=u.location.toString();
	usr.connection=u.connection.toString();
	usr.logon=strftime("%b-%d-%y",u.stats.laston_date);
	usr.laston=0-u.stats.laston_date;
	template.users.push(usr);
}
if(http_request.query["sort"]!=undefined)
	template.users.sort(alphasort);

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load(topnav_html);
if(do_leftnav)
load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
write_template("userlist.inc");
if(do_footer)
	write_template("footer.inc");

function alphasort (a,b)
{
	if(http_request.query["sort"]==undefined)
		return(0);
	var sortby=http_request.query["sort"]
	var au;
	var bu;
	if(a[sortby].toUpperCase!=undefined)
		au=a[sortby].toUpperCase();
	else
		au=a[sortby];
	if(b[sortby].toUpperCase!=undefined)
		bu=b[sortby].toUpperCase();
	else
		bu=b[sortby];
	if(au<bu)
		return -1;
	if(bu>au)
		return 1;
	return 0;
}
