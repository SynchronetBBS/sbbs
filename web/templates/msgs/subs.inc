<div align="center"><P><A HREF="http://^^JS:system.inet_addr^^/">Home</A> :  <A href="/members/">Members</A> : <A HREF="index.ssjs">Groups</A> : <B>%%group:description%%</B></P></div>
<TABLE class="centeralign">
<tr>
<th class="sublist">Sub</th><th class="sublist">Info</th><th class="grouplist">Description</th>
</tr>
<<REPEAT subs>>
<TR>
	<TD>
		<A HREF="msgs.ssjs?msg_grp=^^group:number^^&amp;msg_sub=^^subs:code^^">%%subs:name%%</A>
		
	</TD>
    <td>
    <A HREF="subinfo.ssjs?msg_grp=^^group:number^^&amp;msg_sub=^^subs:code^^">[?]</A>
    </td>
	<TD>
		%%subs:description%%
	</TD>
</TR>
<<END REPEAT subs>>
</TABLE>

