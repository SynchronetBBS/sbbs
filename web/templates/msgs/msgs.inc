<center>
<P><A HREF="http://^^JS:system.inet_addr^^/">Home</A> :  <A href="/members/">Members</A> : <A HREF="index.ssjs">Groups</A> : <A HREF="subs.ssjs?msg_grp=^^group:number^^">%%group:description%%</A> : <B>%%sub:description%%</B>
<A HREF="subinfo.ssjs?msg_grp=^^group:number^^&amp;msg_sub=^^sub:code^^">[?]</A></P>
<a href="post.ssjs?msg_grp=^^group:number^^&amp;msg_sub=^^sub:code^^&amp;post=new">Post New</a>
<FORM action="management.ssjs" METHOD="GET">
<TABLE class="msglist" cellpadding=5 bordercolor="#000000" bordercolorlight="#000000" bordercolordark="#FFFFFF" CELLSPACING=0 BORDER=1 WIDTH="100%">
<TR bgcolor=white><TH class="msglist"><font color=black>X</FONT></TH><TH class="msglist"><font color=black>A</font></TH><TH class="msglist"><font color=black>Subject</font></TH><TH class="msglist"><font color=black>From</font></TH><TH class="msglist"><font color=black>To</font></TH><TH class="msglist"><font color=black>Date</font></TH></TR>
<!-- %%JS:template.color=0%% -->
<<REPEAT messages>>
<TR bgcolor=@@JS:((template.color++)%2==0)?'"#00868B"':'"#007979"';@@>
	<TD class="msglist">
		<INPUT name="offset" value="%%messages:offset%%" type="checkbox">
	</TD>
	<TD class="msglist">
		@@JS:if(@@messages:attachments@@>0) '<img alt="Attachment" src="/graphics/attach.gif">'; else '&nbsp;';@@
	</TD>
	<TD>
		<A HREF="msg.ssjs?msg_grp=^^group:number^^&amp;msg_sub=^^sub:code^^&amp;message=^^messages:number^^">%%messages:subject%%</A>
	</TD>
	<TD>
		%%messages:from%%
	</TD>
	<TD>
		%%messages:to%%
	</TD>
	<TD nowrap>
		%%JS:system.timestr(@@messages:when_written_time@@)%%
	</TD>
</TR>
<<END REPEAT messages>>
</TABLE>
<INPUT TYPE="Submit" NAME="Delete" VALUE="Delete Messages">
</FORM>
@@pagelinks@@
</center>
