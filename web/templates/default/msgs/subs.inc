<p class="navigation"><a class="navigation" href="http://^^JS:system.inet_addr^^/">Home</a> : <a class="navigation" href="index.ssjs">Groups</a> : <b>%%group:description%%</b></p>
<table class="sublist" cellpadding="2" cellspacing="2">
<tr>
    <th class="sublistlft">Sub</th>
    <th class="sublistlft">Description</th>
    <th class="sublist">Total Msgs</th>
    <th class="sublist">Latest Msg</th>
</tr>
<<REPEAT subs>>
<tr>
	<td class="sublist"><a class="msglist" href="msgs.ssjs?msg_sub=^^subs:code^^">%%subs:name%%</a></td>
    <td class="sublist"><a class="msglistnu" href="subinfo.ssjs?msg_sub=^^subs:code^^" title="Click for Sub Info">%%subs:description%%</a></td>
    <td class="sublistright">%%subs:messages%%</td>
    <td class="sublistright">%%subs:lastmsg%%</td>
</tr>
<<END REPEAT subs>>
</table>
@@JS:template.showall_toggle==undefined ? '' : '<p class="msgcenter">['+template.showall_toggle+']'@@</p>
