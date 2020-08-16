<!-- $Id: msg.inc,v 1.18 2006/01/31 23:14:21 runemaster Exp $ -->

<!-- Main Content -->  
    
    <td class="main" valign="top"><br />

<table class="msg" border="0" width="100%">
<tr class="msg">
    <td width="1%" class="msgheader" nowrap="nowrap" onDblClick="window.open().document.write('%%hfields%%')">Subject:</td>
    <td nowrap="nowrap">%%hdr:subject%%</td>
    <td width="1%" class="msgheader" nowrap="nowrap">Date:</td>
    <td nowrap="nowrap">%%JS:system.timestr(template.hdr.when_written_time)%%</td>
</tr>
<tr class="msg">
    <td width="1%" class="msgheader" nowrap="nowrap">From:</td>
    <td nowrap="nowrap">%%hdr:from%%</td>
    <td width="1%" class="msgheader" nowrap="nowrap">To:</td>
    <td nowrap="nowrap">%%hdr:to%%</td>
</tr>
<tr><td colspan="4"><hr width="90%" /></td></tr>
<tr>
    @@JS:template.txtbodybgwht?'<td class="txtbodywht" colspan="4"><pre class="leftalignwht">':'<td class="txtbody" colspan="4"><pre class="leftalign">'@@@@body@@</pre>
    <!-- <hr /> -->
    </td>
</tr>
<tr><td colspan="4"><hr width="90%" /></td></tr>
<tr>
<td colspan="4">
<table class="msg2" width="100%">
<tr>
    <td width="1%" class="msgnavleft" nowrap="nowrap">@@prevlink@@</td>
    <td width="1%" class="msgnavmid">@@JS:template.can_post?'<a href="reply.ssjs?msg_sub='+template.sub.code+'&amp;reply_to='+template.hdr.number+'"><img src="' + template.image_dir + '/reply.gif" alt="Reply" width="150" height="20" border="0" title="Reply" /></a>':'&nbsp;'@@</td>
    <td width="1%" class="msgnavmid">@@JS:template.can_post?'<a href="post.ssjs?msg_sub='+template.sub.code+'&amp;post=new"><img src="' + template.image_dir + '/post_new.gif" alt="Post New" width="150" height="20" border="0" title="Post New" /></a>':'&nbsp;'@@</td>
    <td width="1%" class="msgnavmid">@@JS:template.can_delete?'<a href="management.ssjs?Action=Delete&amp;msg_sub='+template.sub.code+'&amp;number='+template.hdr.number+'"><img src="' + template.image_dir + '/delete.gif" alt="Delete" width="150" height="20" border="0" title="Delete" /></a>':'&nbsp;'@@</td>
    <td width="1%" class="msgnavright" nowrap="nowrap">@@nextlink@@</td>
</tr>
@@JS:if(template.replyto!=undefined && sub!='mail') '<tr class="msg"><td class="msg" colspan="5"><b>In Reply To: </b><a href="msg.ssjs?msg_sub='+sub+'&amp;message='+template.replyto.number+'">'+template.replyto.subject+'</a> ('+template.replyto.from+')</td></tr>'; else '';@@
@@JS:if(template.replies==undefined || template.replies.length<=0) '<!--'; else '';@@
<tr class="msg"><td class="msg" colspan="5"><b>Replies: </b><<REPEAT replies>><a href="msg.ssjs?msg_sub=^^sub:code^^&amp;message=^^replies:number^^">%%replies:subject%%</a> (%%replies:from%%)<<END REPEAT replies>></td></tr>
@@JS:if(template.replies==undefined || template.replies.length<=0) '-->'; else '';@@
@@JS:if(template.attachments==undefined) '<!--'; else '';@@
<tr class="msg"><td colspan="5"><b>Attachments:</b><br />
<<REPEAT attachments>>
<a href="attachments.ssjs/^^sub:code^^/^^hdr:number^^/^^attachments:name^^">%%attachments:name%%</a><br />
<<END REPEAT attachments>>
</td>
</tr>
@@JS:if(template.attachments==undefined) '-->'; else '';@@
</table>
</td>
</tr>
</table>

<br />

<!-- end Main Content -->

