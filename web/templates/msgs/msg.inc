<P><A HREF="http://freebsd.synchro.net/">Home</A> : <A HREF="index.ssjs">Groups</A> : <A HREF="subs.ssjs?msg_grp=^^group:number^^">%%group:description%%</A> :
<A HREF="msgs.ssjs?msg_grp=^^group:number^^&msg_sub=^^sub:code^^">%%sub:description%%</A></P>
<TABLE BORDER=0>
<TR><TD>Subject:</TD><TD WIDTH="100%">%%hdr:subject%%</TD></TR>
<TR><TD>To:</TD><TD>%%hdr:to%%</TD></TR>
<TR><TD>From:</TD><TD>%%hdr:from%%</TD></TR>
<TR><TD>Date:</TD><TD>%%JS:system.timestr(@@hdr:when_written_time@@)%%</TD></TR>
<TR><TD COLSPAN=2><HR></TD></TR>
<TR>
<TD COLSPAN=2>
<PRE><FONT FACE="Courier New">@@body@@</FONT></PRE>
</TD>
</TR>
</TABLE>
@@JS:if(template.attachments==undefined) '<!--'; else '';@@
<b>Attachments:</b><br>
<<REPEAT attachments>>
<a href="attachments.ssjs/^^group:code^^/^^sub:code^^/^^hdr:number^^/^^attachments:name^^">%%attachments:name%%</a><br>
<<END REPEAT attachments>>
@@JS:if(template.attachments==undefined) '-->'; else '';@@
<A HREF="reply.ssjs?msg_grp=^^group:number^^&msg_sub=^^sub:code^^&reply_to=^^hdr:number^^">Reply</A>
