<!-- $Id: userstats.inc,v 1.8 2006/01/31 23:13:43 runemaster Exp $ -->

<!-- Main Content -->  
  <td class="main" valign="top"><br />
   
<table class="userstats" border="0" cellpadding="2" cellspacing="2">
<tr>
  <td class="userstats">First on:</td>
  <td class="userstats">%%JS:strftime("%b-%d-%y",user.stats.firston_date)%%</td>
  <td class="userstats">Expire:</td>
  <td class="userstats">%%JS:user.security.expiration_date?strftime("%b-%d-%y",user.security.expiration_date):"Never"%%</td>
  <td class="userstats">Last on:</td>
  <td class="userstats">%%JS:strftime("%b-%d-%y",user.stats.laston_date)%%</td>
  <td class="userstats" colspan="2">%%JS:strftime("%H:%M",user.stats.laston_date)%%</td>
</tr>
<tr>
  <td class="userstats">Time on:</td>
  <td class="userstats">%%JS:user.stats.total_timeon%%</td>
  <td class="userstats">Today:</td>
  <td class="userstats"> 
  <table width="100%" border="0" cellpadding="0" cellspacing="0">
    <tr>
      <td class="userstats" align="left">%%JS:user.stats.timeon_today%%&nbsp;</td>
      <td class="userstats" align="right">&nbsp;%%JS:user.limits.time_per_day;%%</td>
    </tr>
  </table>
  </td>
  <td class="userstats">Last Call:</td>
  <td class="userstats">
    <table width="100%" border="0" cellpadding="0" cellspacing="0">
    <tr>
      <td class="userstats" align="left">%%JS:user.stats.timeon_last_logon%%&nbsp;</td>
      <td class="userstats" align="right">&nbsp;%%JS:user.limits.time_per_logon;%%</td>
    </tr>
    </table>
  </td>
  <td class="userstats">Extra:</td>
  <td class="userstats">%%JS:user.security.extra_time%%</td>
</tr>
<tr>
  <td class="userstats">Logons:</td>
  <td class="userstats">%%JS:user.stats.total_logons%%</td>
  <td class="userstats">Today:</td>
  <td class="userstats">
    <table width="100%" border="0" cellpadding="0" cellspacing="0">
    <tr>
      <td class="userstats" align="left">%%JS:user.stats.logons_today%%&nbsp;</td>
      <td class="userstats" align="right">&nbsp;%%JS:user.limits.logons_per_day%%</td>
    </tr>
    </table>
  </td>
  <td class="userstats">Posts:</td>
  <td class="userstats">
    <table width="100%" border="0" cellpadding="0" cellspacing="0">
    <tr>
      <td class="userstats" align="left">%%JS:user.stats.total_posts%%&nbsp;</td>
      <td class="userstats" align="right">&nbsp;%%pinfo%%</td>
    </tr>
    </table>
  </td>
  <td class="userstats">Today:</td>
  <td class="userstats">%%JS:user.stats.posts_today%%</td>
</tr>
<tr>
  <td class="userstats">E-Mails:</td>
  <td class="userstats">%%JS:user.stats.total_emails%%</td>
  <td class="userstats">To sysop:</td>
  <td class="userstats">%%JS:user.stats.total_feedbacks%%</td>
  <td class="userstats">Waiting:</td>
  <td class="userstats">%%JS:user.stats.mail_waiting%%</td>
  <td class="userstats">Today:</td>
  <td class="userstats">%%JS:user.stats.email_today%%</td>
</tr>
<tr>
  <td class="userstats" colspan="8">&nbsp;</td>
</tr>
  <tr>
    <td class="userstats">Uploads:</td>
    <td class="userstats" align="right">%%bytes_uploaded%%</td>
    <td class="userstats" colspan="6">bytes in %%files_uploaded%% files</td>
  </tr>
  <tr>
    <td class="userstats">Downloads:</td>
    <td class="userstats" align="right">%%bytes_downloaded%%</td>
    <td class="userstats" colspan="6">bytes in %%files_downloaded%% files</td>
  </tr>
  <tr>
    <td class="userstats">Credits:</td>
    <td class="userstats" align="right">%%credits%%</td>
    <td class="userstats" colspan="6">free: %%freecreds%% (%%freeday%% per day)</td>
  </tr>
  <tr>
    <td class="userstats">Minutes:</td>
    <td class="userstats" align="right">%%tbank%%</td>
    <td class="userstats" colspan="6">&nbsp;</td>
  </tr>
</table>
<br />
<br />

<!-- end Main Content -->

