load("../web/lib/template.ssjs");

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

write_template("header.inc");
write_template("userstats.inc");
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
