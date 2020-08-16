<!-- $Id: post.inc,v 1.9 2006/01/31 23:14:21 runemaster Exp $ -->

<!-- Main Content -->  
    
    <td class="main" valign="top"><br />
    
<form action="savemsg.ssjs" method="post">
<table class="postmsg" border="0" cellspacing="2" cellpadding="0">
<input type="hidden" name="msg_sub" value="%%sub:code%%" />
  <tbody>
    <tr>
      <td class="postmsg">Subject: <input class="postmsg" type="text" name="subject" value="" /></td>
      <td class="postmsg">To: <input class="postmsg" type="text" name="to" 
        @@JS:template.sub.code=='mail' ? "" : "value='All'"@@ /></td>
    </tr>
    <tr>
      <td class="postmsg" colspan="2">@@anonnote@@</td>
    </tr>
    <tr>
      <td class="postmsg" colspan="2">@@privnote@@</td>
    </tr>
    <tr>
      <td class="postmsg" colspan="2"><textarea class="postmsg" name=body rows="20" cols="79" wrap="hard"></textarea></td>
      </tr>
    <tr>
      <td align="center" class="postmsg" colspan="2"><input type="image" src="@@image_dir@@/@@post_button@@" value=@@JS:template.sub.code=='mail'? '"Send E-Mail"' : '"Post Message"' @@/></td>
    </tr>
  </tbody>
</table>
</form>

<!-- end Main Content -->

