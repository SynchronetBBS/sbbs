<p class="navigation"><a class="navigation" href="http://^^JS:system.inet_addr^^/">Home</a> : <a class="navigation" href="/members/">Members</a> : <a class="navigation" href="/members/info.ssjs">Information</a> : Your Stats</p>
<table class="userstats" border="0" cellpadding="2" cellspacing="2">
<tr>
  <td class="userstats">First on:</td class="userstats">
  <td class="userstats">%%JS:strftime("%m/%d/%y",user.stats.firston_date)%%</td class="userstats">
  <td class="userstats">Expire:</td class="userstats">
  <td class="userstats">%%JS:user.security.expiration_date?strftime("%m/%d/%y",user.security.expiration_date):"Never"%%</td class="userstats">
  <td class="userstats">Last on:</td class="userstats">
  <td class="userstats">%%JS:strftime("%m/%d/%y",user.stats.laston_date)%%</td class="userstats">
  <td class="userstats" colspan=2>%%JS:strftime("%H:%M",user.stats.laston_date)%%</td class="userstats">
</tr>
<tr>
  <td class="userstats">Time on:</td class="userstats">
  <td class="userstats">%%JS:user.stats.total_timeon%%</td class="userstats">
  <td class="userstats">Today:</td class="userstats">
  <td class="userstats"> 
  <table width="100%" border=0 cellpadding=0 cellspacing=0>
    <tr>
	  <td class="userstats" align="left">%%JS:user.stats.timeon_today%%&nbsp;</td class="userstats">
	  <td class="userstats" align="right">&nbsp;%%JS:user.limits.time_per_day;%%</td class="userstats">
	</tr>
  </table>
  </td class="userstats">
  <td class="userstats">Last Call:</td class="userstats">
  <td class="userstats">
    <table width="100%" border=0 cellpadding=0 cellspacing=0>
    <tr>
	  <td class="userstats" align="left">%%JS:user.stats.timeon_last_logon%%&nbsp;</td class="userstats">
	  <td class="userstats" align="right">&nbsp;%%JS:user.limits.time_per_logon;%%</td class="userstats">
	</tr>
    </table>
  </td class="userstats">
  <td class="userstats">Extra:</td class="userstats">
  <td class="userstats">%%JS:user.security.extra_time%%</td class="userstats">
</tr>
<tr>
  <td class="userstats">Logons:</td class="userstats">
  <td class="userstats">%%JS:user.stats.total_logons%%</td class="userstats">
  <td class="userstats">Today:</td class="userstats">
  <td class="userstats">
    <table width="100%" border=0 cellpadding=0 cellspacing=0>
    <tr>
	  <td class="userstats" align="left">%%JS:user.stats.logons_today%%&nbsp;</td class="userstats">
	  <td class="userstats" align="right">&nbsp;%%JS:user.limits.logons_per_day%%</td class="userstats">
	</tr>
    </table>
  </td class="userstats">
  <td class="userstats">Posts:</td class="userstats">
  <td class="userstats">
    <table width="100%" border=0 cellpadding=0 cellspacing=0>
    <tr>
	  <td class="userstats" align="left">%%JS:user.stats.total_posts%%&nbsp;</td class="userstats">
	  <td class="userstats" align="right">&nbsp;%%pinfo%%</td class="userstats">
	</tr>
    </table>
  </td class="userstats">
  <td class="userstats">Today:</td class="userstats">
  <td class="userstats">%%JS:user.stats.posts_today%%</td class="userstats">
</tr>
<tr>
  <td class="userstats">E-Mails:</td class="userstats">
  <td class="userstats">%%JS:user.stats.total_emails%%</td class="userstats">
  <td class="userstats">To sysop:</td class="userstats">
  <td class="userstats">%%JS:user.stats.total_feedbacks%%</td class="userstats">
  <td class="userstats">Waiting:</td class="userstats">
  <td class="userstats">%%JS:user.stats.mail_waiting%%</td class="userstats">
  <td class="userstats">Today:</td class="userstats">
  <td class="userstats">%%JS:user.stats.email_today%%</td class="userstats">
</tr>
<tr>
  <td class="userstats" colspan="8">&nbsp;</td class="userstats">
</tr>
  <tr>
    <td class="userstats">Uploads:</td class="userstats">
	<td class="userstats" align="right">%%bytes_uploaded%%</td class="userstats">
	<td class="userstats" colspan="6">bytes in %%files_uploaded%% files</td class="userstats">
  </tr>
  <tr>
    <td class="userstats">Downloads:</td class="userstats">
	<td class="userstats" align="right">%%bytes_downloaded%%</td class="userstats">
	<td class="userstats" colspan="6">bytes in %%files_downloaded%% files</td class="userstats">
  </tr>
  <tr>
    <td class="userstats">Credits:</td class="userstats">
	<td class="userstats" align="right">%%credits%%</td class="userstats">
	<td class="userstats" colspan="6">free: %%freecreds%% (%%freeday%% per day)</td class="userstats">
  </tr>
  <tr>
    <td class="userstats">Minutes:</td class="userstats">
	<td class="userstats" align="right">%%tbank%%</td class="userstats">
	<td class="userstats" colspan="6">&nbsp;</td class="userstats">
  </tr>
</table>

