<p class="navigation"><a class="navigation" href="http://^^JS:system.inet_addr^^/">Home</a> : <b>Groups</b></p>
<table class="grouplist" border="0" cellpadding="2" cellspacing="2">
<tr>
<th class="grouplist">Group</th><th class="grouplist">Description</th>
</tr>
<<REPEAT groups>>
<tr>
	<td class="grouplist"><a class="grouplist" href="subs.ssjs?msg_grp=^^groups:name^^">%%groups:name%%</a></td>
	<td class="grouplist">%%groups:description%%</td>
</tr>
<<END REPEAT groups>>
<tr>
        <td class="grouplist"><a class="grouplist" href="msgs.ssjs?msg_sub=mail">E-Mail</a></td>
        <td class="grouplist">Personal E-Mail</td>
</tr>
</table>

