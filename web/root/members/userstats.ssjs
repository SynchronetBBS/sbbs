/* $Id$ */

load("../web/lib/template.ssjs");

var sub="";

template.title="User Statistics for " +user.alias; 

template.first = new Array();

template.first.push ({ html: '<td class="userstats">First on:</td>' });
template.first.push ({ html: '<td class="userstats">' + strftime("%b-%d-%y",user.stats.firston_date) +'</td>' });
template.first.push ({ html: '<td class="userstats">Expire:</td>' });

var expire=user.security.expiration_date?strftime("%b-%d-%y",user.security.expiration_date):"Never";

template.first.push ({ html: '<td class="userstats">' + expire + '</td>' });
template.first.push ({ html: '<td class="userstats">Last on:</td>' });
template.first.push ({ html: '<td class="userstats" colspan="3">' + strftime("%b-%d-%y %H:%M",user.stats.laston_date) +'</td>' });

template.total_timeon=addcommas(user.stats.total_timeon);

template.second = new Array();

template.second.push ({ html: '<td class="userstats" align="left">' + user.stats.timeon_today + '&nbsp;</td>' });
template.second.push ({ html: '<td class="userstats" align="right">&nbsp;' + user.limits.time_per_day + '</td>' });

template.third = new Array();

template.third.push ({ html: '<td class="userstats" align="left">' + user.stats.timeon_last_logon + '&nbsp;</td>' });
template.third.push ({ html: '<td class="userstats" align="right">&nbsp;' + user.limits.time_per_logon + '</td>' });
template.extra_time=user.security.extra_time;
template.total_logons=addcommas(user.stats.total_logons);

template.fourth = new Array();

template.fourth.push ({ html: '<td class="userstats" align="left">' + user.stats.logons_today + '&nbsp;</td>' });
template.fourth.push ({ html: '<td class="userstats" align="right">&nbsp;' + user.limits.logons_per_day + '</td>' });

template.fifth = new Array();

template.fifth.push ({ html: '<td class="userstats" align="left">' + user.stats.total_posts + '&nbsp;</td>' });

var i;
if(user.stats.total_posts)
    i=user.stats.total_logons/user.stats.total_posts;
else
    i=0;
i=i ? 100/i : user.stats.total_posts > user.stats.total_logons ? 100 : 0;
var pinfo=parseInt(i);

template.fifth.push ({ html: '<td class="userstats" align="right">&nbsp;' + pinfo + '</td>' });

template.posts_today=user.stats.posts_today;

template.sixth = new Array();

template.sixth.push ({ html: '<td class="userstats">E-Mails:</td>' });
template.sixth.push ({ html: '<td class="userstats">' + user.stats.total_emails + '</td>' });
template.sixth.push ({ html: '<td class="userstats">To sysop:</td>' });
template.sixth.push ({ html: '<td class="userstats">' + user.stats.total_feedbacks + '</td>' });
template.sixth.push ({ html: '<td class="userstats">Waiting:</td>' });
template.sixth.push ({ html: '<td class="userstats">' + user.stats.mail_waiting + '</td>' });
template.sixth.push ({ html: '<td class="userstats">Today:</td>' });
template.sixth.push ({ html: '<td class="userstats">' + user.stats.email_today + '</td>' });

template.bytes_uploaded=addcommas(user.stats.bytes_uploaded);
template.files_uploaded=addcommas(user.stats.files_uploaded);
template.bytes_downloaded=addcommas(user.stats.bytes_downloaded);
template.files_downloaded=addcommas(user.stats.files_downloaded);
template.credits=addcommas(user.security.credits);
template.freecreds=addcommas(user.security.free_credits);
template.freeday=addcommas(user.limits.free_credits_per_day);
template.tbank=addcommas(user.security.minutes);


write_template("header.inc");
load("../web/lib/topnav_html.ssjs");
load("../web/lib/leftnav_html.ssjs");
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
