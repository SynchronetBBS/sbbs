<center>
<P><A HREF="http://^^JS:system.inet_addr^^/">Home</A> : <A HREF="index.ssjs">Groups</A> : <A HREF="subs.ssjs?msg_grp=^^group:number^^">%%group:description%%</A> : <B>%%sub:description%%</B></P>
<a href="post.ssjs?msg_grp=^^group:number^^&msg_sub=^^sub:code^^&post=new">Post New</a>
<TABLE cellpadding=5 bordercolor="#000000" bordercolorlight="#000000" bordercolordark="#FFFFFF" CELLSPACING=0 BORDER=1 WIDTH="100%"><TR><TH>Subject</TH><TH>From</TH><TH>To</TH><TH>Date</TH></TR>
<!-- %%JS:template.color=0%% -->
<<REPEAT messages>>
<TR bgcolor=@@JS:((template.color++)%2==0)?'"Turquoise"':'"lightblue"';@@>
	<TD>
		<A HREF="msg.ssjs?msg_grp=^^group:number^^&msg_sub=^^sub:code^^&message=^^messages:number^^">%%messages:subject%%</A>
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
@@pagelinks@@
</center>
