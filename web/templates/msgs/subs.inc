<p class="navigation"><a class="navigation" href="http://^^JS:system.inet_addr^^/">Home</a> :  <a class="navigation" href="/members/">Members</a> : <a class="navigation" href="index.ssjs">Groups</a> : <b>%%group:description%%</b> : (@@showall_toggle@@)</p>
<table class="sublist" cellpadding="2" cellspacing="2">
<tr>
    <th class="sublistlft">Sub</th>
    <th class="sublist">Info</th>
    <th class="sublistlft">Description</th>
    <th class="sublist">Total Msgs</th>
    <th class="sublist">Latest Msg</th>
</tr>
<<REPEAT subs>>
<tr>
	<td class="sublist"><a class="msglist" href="msgs.ssjs?msg_sub=^^subs:code^^">%%subs:name%%</a></td>
    <td class="sublistcntr"><a class="msglistnu" href="subinfo.ssjs?msg_sub=^^subs:code^^" title="Click for Sub Info">?</a></td>
	<td class="sublist">%%subs:description%%</td>
    <td class="sublist">%%subs:messages%%</td>
    <td class="sublist">%%subs:lastmsg%%</td>
</tr>
<<END REPEAT subs>>
</table>

