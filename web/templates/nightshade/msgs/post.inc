<!-- $Id: post.inc,v 1.1 2006/02/22 02:16:41 runemaster Exp $ -->

<!-- Main Content -->  
    
<div id="contentcontainer3">
	<div id="content2">
	 <div id="post">
    
<form action="savemsg.ssjs" method="post">
<table class="postmsg" border="0" cellspacing="1" cellpadding="2">
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
      <td align="center" class="postmsg" colspan="2"><input type="submit" value=@@JS:template.sub.code=='mail'? '"Send E-Mail"' : '"Post Message"' @@/></td>
    </tr>
  </tbody>
</table>
</form>
		</div>
		</div>
	</div>
	<div class="cleardiv2"></div>
	</div>
</div>

<!-- end Main Content -->

