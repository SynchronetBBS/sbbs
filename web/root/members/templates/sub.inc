<!-- Sub listing

Supports:
%%PATH%%		- URL for this script
%%GRP_*%%		- Property from msg_area.grp_list ie: GRP_description
%%SUB_*%%		- Property from msg_area.grp_list.sub_list ie: SUB_description
-->
<TR>
	<TD>
		<A HREF="%%PATH%%?msg_grp=!!URI%%GRP_number%%!!&msg_sub=!!URI%%SUB_code%%!!">%%SUB_name%%</A>
	</TD>
	<TD>
		%%SUB_description%%
	</TD>
</TR>
