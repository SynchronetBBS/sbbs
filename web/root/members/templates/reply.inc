<FORM action="%%PATH%%" method="POST">
<input type="hidden" name="msg_grp" value="%%GRP_number%%">
<input type="hidden" name="msg_sub" value="%%SUB_code%%">
Subject: <input type="text" name="subject" value="Re: %%HDR_subject%%"><br>
To: <input type="text" name="to" value="%%HDR_from%%"><BR>
<textarea name=body rows=20 cols=80>On %%HDR_date%% %%HDR_from%% wrote to %%HDR_to%%

%%BODY%%</textarea><br>
<input type="Submit">
</FORM>
</BODY>
</HTML>

