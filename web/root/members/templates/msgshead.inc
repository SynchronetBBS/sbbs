<!-- Group header.

Supports:
%%PATH%% - Valid URL for this script.
%%NEXT%%  - URL to "next" screen
%%PREV%%  - URL to "previous" screen
%%GRP_*%%               - Property from msg_area.grp_list ie: GRP_description

--NEXT%% ... %%NEXT-- removed if there is a next screen.
++NEXT%% ... %%NEXT++ removed unless there is a next screen


-->
<P><A HREF="http://freebsd.synchro.net/">Home</A> : <A HREF="%%PATH%%">Groups</A> : <A HREF="%%PATH%%?msg_grp=!!URI%%GRP_number%%!!">%%GRP_description%%</A> : 
<B>%%SUB_description%%</B></P>
<a href="%%PATH%%?msg_grp=!!URI%%GRP_number%%!!&msg_sub=!!URI%%SUB_code%%!!&post=new">Post New</a>
<TABLE cellpadding=5 bordercolor="#000000" bordercolorlight="#000000" bordercolordark="#FFFFFF" CELLSPACING=0 BORDER=1 WIDTH="100%"><TR><TH>Subject</TH><TH>From</TH><TH>To</TH><TH>Date</TH></TR>
