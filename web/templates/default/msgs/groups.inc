<p class="navigation"><a class="navigation" href="/index.ssjs">Home</a> : <b>Groups</b></p>
<table class="grouplist" border="0" cellpadding="2" cellspacing="2">
<tr>
<th class="grouplist">Message Group</th><th class="grouplist">Subs</th>
</tr>
<<REPEAT groups>>
<tr>
	<td class="grouplist"><a class="grouplist" href="subs.ssjs?msg_grp=^^groups:name^^">%%groups:description%%</a></td>
    <td class="grouplist" align=right>@@JS:msg_area.grp['^^groups:name^^'].sub_list.length@@</td>
</tr>
<<END REPEAT groups>>
</table>

