<CENTER>
<A HREF="http://^^JS:system.inet_addr^^">Home</A> : <A HREF="index.ssjs">Groups</A> : <A HREF="subs.ssjs?msg_grp=^^group:number^^">%%group:description%%</A> :
<A HREF="msgs.ssjs?msg_grp=^^group:number^^&msg_sub=^^sub:code^^">%%sub:description%%</A><BR>
<TABLE BORDER=0 width="95%">
<TR><TD nowrap>Subject:</TD><TD nowrap>%%hdr:subject%%</TD>
<TD nowrap>To:</TD><TD nowrap>%%hdr:to%%</TD></TR>
<TR><TD nowrap>From:</TD><TD nowrap>%%hdr:from%%</TD>
<TD nowrap>Date:</TD><TD nowrap>%%JS:system.timestr(@@hdr:when_written_time@@)%%</TD></TR>
<TR><TD COLSPAN=4><HR></TD></TR>
<TR>
<TD COLSPAN=4>
<OBJECT TYPE="text/html" BORDER=2 DATA="msgframe.ssjs/^^group:number^^/^^sub:code^^/^^hdr:number^^" STANDBY="Loading Message..." WIDTH="100%" HEIGHT="450">
@@body@@
</OBJECT>
</TD>
</TR>
</TABLE>
@@JS:if(template.attachments==undefined) '<!--'; else '';@@
<b>Attachments:</b><br>
<<REPEAT attachments>>
<a href="attachments.ssjs/^^group:number^^/^^sub:code^^/^^hdr:number^^/^^attachments:name^^">%%attachments:name%%</a><br>
<<END REPEAT attachments>>
@@JS:if(template.attachments==undefined) '-->'; else '';@@
<A HREF="reply.ssjs?msg_grp=^^group:number^^&msg_sub=^^sub:code^^&reply_to=^^hdr:number^^">Reply</A>
</CENTER>
