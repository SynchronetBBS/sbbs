/* $Id: userstats.ssjs,v 1.12 2006/02/25 21:40:35 runemaster Exp $ */

load("../web/lib/template.ssjs");

var sub="";

template.title="User Statistics for " +user.alias; 

template.bytes_uploaded=addcommas(user.stats.bytes_uploaded);
template.files_uploaded=addcommas(user.stats.files_uploaded);
template.bytes_downloaded=addcommas(user.stats.bytes_downloaded);
template.files_downloaded=addcommas(user.stats.files_downloaded);
template.credits=addcommas(user.security.credits);
template.freecreds=addcommas(user.security.free_credits);
template.freeday=addcommas(user.limits.free_credits_per_day);
template.tbank=addcommas(user.security.minutes);
var i;
if(user.stats.total_posts)
	i=user.stats.total_logons/user.stats.total_posts;
else
	i=0;
i=i ? 100/i : user.stats.total_posts > user.stats.total_logons ? 100 : 0;
template.pinfo=parseInt(i);

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load(topnav_html);
if(do_leftnav)
load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
write_template("userstats.inc");
if(do_footer)
	write_template("footer.inc");

function addcommas(num)
{
	num = '' + num;
	if (num.length > 3) {
		var mod = num.length % 3;
		var output = (mod > 0 ? (num.substring(0,mod)) : '');
		for (i=0 ; i < Math.floor(num.length / 3); i++) {
			if ((mod == 0) && (i == 0))
				output += num.substring(mod+ 3 * i, mod + 3 * i + 3);
			else
				output+= ',' + num.substring(mod + 3 * i, mod + 3 * i + 3);
		}
		return (output);
	}
	else
		return num;
}
