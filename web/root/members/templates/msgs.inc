<!-- Sub listing

Supports:
%%PATH%%		- URL for this script
%%GRP_*%%		- Property from msg_area.grp_list ie: GRP_description
%%SUB_*%%		- Property from msg_area.grp_list.sub_list ie: SUB_description
%%HDR_*%%		- Property from Header object ie: HDR_from
%%HDRDATE%%		- Formatted date from HDR
-->
<TR><TD><A HREF="%%PATH%%?msg_grp=!!URI%%GRP_number%%!!&msg_sub=!!URI%%SUB_code%%!!&message=!!URI%%HDR_number%%!!">%%HDR_subject%%</A></TD>
<TD>%%HDR_from%%</TD>
<TD>%%HDR_to%%</TD>
<TD nowrap>%%HDRDATE%%</TD></TR>

