<CENTER>
<TABLE border=0>
<tr>
  <td>
    First on:
  </td>
  <td>
    %%JS:strftime("%m/%d/%y",user.stats.firston_date)%%
  </td>
  <td>
    Expire:
  </td>
  <td>
    %%JS:user.security.expiration_date?strftime("%m/%d/%y",user.security.expiration_date):"Never"%%
  </td>
  <td>
    Last on:
  </td>
  <td>
    %%JS:strftime("%m/%d/%y",user.stats.laston_date)%%
  </td>
  <td colspan=2>
    %%JS:strftime("%H:%M",user.stats.laston_date)%%
  </td>
</tr>
<tr>
  <td>
    Time on:
  </td>
  <td>
    %%JS:user.stats.total_timeon%%
  </td>
  <td>
    Today:
  </td>
  <td>
    <table width="100%" border=0 cellpadding=0 cellspacing=0><tr>
	  <td align="left">%%JS:user.stats.timeon_today%%</td>
	  <td align="right">%%JS:user.limits.time_per_day;%%</td>
	</tr></table>
  </td>
  <td>
    Last Call:
  </td>
  <td>
    <table width="100%" border=0 cellpadding=0 cellspacing=0><tr>
	  <td align="left">%%JS:user.stats.timeon_last_logon%%</td>
	  <td align="right">%%JS:user.limits.time_per_logon;%%</td>
	</tr></table>
  </td>
  <td>
    Extra:
  </td>
  <td>
    %%JS:user.security.extra_time%%
  </td>
</tr>
<tr>
  <td>
    Logons:
  </td>
  <td>
    %%JS:user.stats.total_logons%%
  </td>
  <td>
    Today:
  </td>
  <td>
    <table width="100%" border=0 cellpadding=0 cellspacing=0><tr>
	  <td align="left">%%JS:user.stats.logons_today%%</td>
	  <td align="right">%%JS:user.limits.logons_per_day%%</td>
	</tr></table>
  </td>
  <td>
    Posts:
  </td>
  <td>
    <table width="100%" border=0 cellpadding=0 cellspacing=0><tr>
	  <td align="left">%%JS:user.stats.total_posts%%</td>
	  <td align="right">%%pinfo%%</td>
	</tr></table>
  </td>
  <td>
    Today:
  </td>
  <td>
    %%JS:user.stats.posts_today%%
  </td>
</tr>
<tr>
  <td>
    E-Mails:
  </td>
  <td>
    %%JS:user.stats.total_emails%%
  </td>
  <td>
    To sysop:
  </td>
  <td>
    %%JS:user.stats.total_feedbacks%%
  </td>
  <td>
    Waiting:
  </td>
  <td>
    Not Imp.
  </td>
  <td>
    Today:
  </td>
  <td>
    %%JS:user.stats.email_today%%
  </td>
</tr>
<tr>
<td colspan="8">
&nbsp;
</td>
</tr>
  <tr>
    <td>
	  Uploads:
	</td>
	<td align="right">
	  %%bytes_uploaded%%
	</td>
	<td colspan="6">
	  bytes in %%files_uploaded%% files
	</td>
  </tr>
  <tr>
    <td>
	  Downloads:
	</td>
	<td align="right">
	  %%bytes_downloaded%%
	</td>
	<td colspan="6">
	  bytes in %%files_downloaded%% files
	</td>
  </tr>
  <tr>
    <td>
	  Credits:
	</td>
	<td align="right">
	  %%credits%%
	</td>
	<td colspan="6">
	  free: %%freecreds%% (%%freeday%% per day)
	</td>
  </tr>
  <tr>
    <td>
	  Minutes:
	</td>
	<td align="right">
      %%tbank%%
	</td>
	<td colspan="6">
	  &nbsp;
	</td>
  </tr>
</table>
</center>
