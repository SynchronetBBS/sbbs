<div align="center"><p><a href="http://^^JS:system.inet_addr^^/">Home</a> :  <a href="/members/">Members</a> : <a href="index.ssjs">Groups</a> : <b>%%group:description%%</b></p></div>
<table class="centeralign">
<tr>
<th class="sublist">Sub</th><th class="sublist">Info</th><th class="grouplist">Description</th>
</tr>
<<REPEAT subs>>
<tr>
	<td>
		<a href="msgs.ssjs?msg_grp=^^group:number^^&amp;msg_sub=^^subs:code^^">%%subs:name%%</a>
		
	</td>
    <td>
    <a href="subinfo.ssjs?msg_grp=^^group:number^^&amp;msg_sub=^^subs:code^^">[?]</a>
    </td>
	<td>
		%%subs:description%%
	</td>
</tr>
<<END REPEAT subs>>
</table>

