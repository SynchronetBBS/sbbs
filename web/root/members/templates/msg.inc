<!-- Message Display

Supports:
%%PATH%%		- URL for this script
%%GRP_*%%		- Property from msg_area.grp_list ie: GRP_description
%%SUB_*%%		- Property from msg_area.grp_list.sub_list ie: SUB_description
%%HDR_*%%		- Property from Header object ie: HDR_from
%%HDRDATE%%		- Formatted date from HDR
%%BODY%%		- Message body
-->
<P><A HREF="http://freebsd.synchro.net/">Home</A> : <A HREF="%%PATH%%">Groups</A> : <A HREF="%%PATH%%?msg_grp=!!URI%%GRP_number%%!!">%%GRP_description%%</A> :
<A HREF="%%PATH%%?msg_grp=!!URI%%GRP_number%%!!&msg_sub=!!URI%%SUB_code%%!!">%%SUB_description%%</A></P>
<TABLE BORDER=0>
<TR><TD>Subject:</TD><TD WIDTH="100%">%%HDR_subject%%</TD></TR>
<TR><TD>To:</TD><TD>%%HDR_to%%</TD></TR>
<TR><TD>From:</TD><TD>%%HDR_from%%</TD></TR>
<TR><TD>Date:</TD><TD>%%HDR_date%%</TD></TR>
<TR><TD COLSPAN=2><HR></TD></TR>
<TR>
<TD COLSPAN=2>
<PRE><FONT FACE="Courier New">%%BODY%%</FONT></PRE>
</TD>
</TR>
</TABLE>
<A HREF="%%PATH%%?msg_grp=!!URI%%GRP_number%%!!&msg_sub=!!URI%%SUB_code%%!!&reply_to=!!URI%%HDR_number%%!!">Reply</A>
