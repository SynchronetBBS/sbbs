<FORM action="savemsg.ssjs" method="POST">
<input type="hidden" name="msg_grp" value="%%group:number%%">
<input type="hidden" name="msg_sub" value="%%sub:code%%">
Subject: <input type="text" name="subject" value="%%subject%%"><br>
To: <input type="text" name="to" value="%%from%%"><BR>
<textarea name=body rows=20 cols=80>%%body%%</textarea><br>
<input type="Submit">
</FORM>
