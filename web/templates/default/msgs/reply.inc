<!-- $Id: reply.inc,v 1.7 2006/01/31 23:14:21 runemaster Exp $ -->

<!-- Main Content -->  
    
    <td class="main" valign="top"><br />

<form action="savemsg.ssjs" method="post">
<table class="replymsg" border="0" cellspacing="2" cellpadding="0">
<input type="hidden" name="msg_sub" value="%%sub:code%%" />
<input type="hidden" name="reply_to" value="%%number%%" />
  <tbody>
    <tr>
      <td class="replymsg">Subject: <input class="replymsg" type="text" name="subject" value="%%subject%%" /></td>
      <td class="replymsg">To: <input class="replymsg" type="text" name="to" value="%%from%%" /></td>
    </tr>
    <tr>
      <td class="postmsg" colspan="2">@@anonnote@@</td>
    </tr>
    <tr>
      <td class="postmsg" colspan="2">@@privnote@@</td>
    </tr>
    <tr>
      <td class="replymsg" colspan="2"><textarea class="replymsg" name=body rows="20" cols="79" wrap="hard">%%body%%</textarea></td>
      </tr>
    <tr>
      <td align="center" class="replymsg" colspan="2"><input type="image" src="@@image_dir@@/@@reply_button@@" value="Post Reply" /></td>
    </tr>
  </tbody>
</table>
</form>

<!-- end Main Content -->

