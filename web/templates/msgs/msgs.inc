<center>
<p><a href="http://^^JS:system.inet_addr^^/">Home</a> :  <a href="/members/">Members</a> : <a href="index.ssjs">Groups</a> : <a href="subs.ssjs?msg_grp=^^group:number^^">%%group:description%%</a> : <b>%%sub:description%%</b>
<a href="subinfo.ssjs?msg_grp=^^group:number^^&amp;msg_sub=^^sub:code^^">[?]</a> -> <a href="post.ssjs?msg_grp=^^group:number^^&amp;msg_sub=^^sub:code^^&amp;post=new">Post New Message</a></p>
<form action="management.ssjs" method="get">
<table class="msglist" cellpadding="5" cellspacing="0" border="1" width="100%">
<tr class="msglist"><th class="msglist">X</th><th class="msglist">A</th><th class="msglist">Subject</th><th class="msglist">From</th><th class="msglist">To</th><th class="msglist">Date</th></tr>
<!-- %%JS:template.color=0%% -->
<<REPEAT messages>>
<tr bgcolor=@@JS:((template.color++)%2==0)?'"#00868B"':'"#007979"';@@>
	<td class="msglist">
		<input name="offset" value="%%messages:offset%%" type="checkbox" />
	</td>
	<td class="msglist">
		@@JS:if(@@messages:attachments@@>0) '<img alt="Attachment" src="/graphics/attach.gif">'; else '&nbsp;';@@
	</td>
	<td>
		<a href="msg.ssjs?msg_grp=^^group:number^^&amp;msg_sub=^^sub:code^^&amp;message=^^messages:number^^">%%messages:subject%%</a>
	</td>
	<td>
		%%messages:from%%
	</td>
	<td>
		%%messages:to%%
	</td>
	<td nowrap>
		%%JS:system.timestr(@@messages:when_written_time@@)%%
	</td>
</tr>
<<END REPEAT messages>>
</table>
<br />
<input type="submit" name="Delete" value="Delete Message(s)">
</form>
@@pagelinks@@
</center>
