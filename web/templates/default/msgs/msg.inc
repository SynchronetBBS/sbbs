<p class="navigation"><a class="navigation" href="/index.ssjs">Home</a>@@JS:template.sub.code!='mail'? ' : <a class="navigation" href="index.ssjs">Groups</a> :':''@@ <a class="navigation" href="subs.ssjs?msg_grp=^^group:name^^">%%group:description%%</a> :
<a class="navigation" href="msgs.ssjs?msg_sub=^^sub:code^^">%%sub:description%%</a><br /></p>

<table class="msg" border="0" width="95%">
<tr class="msg">
    <td width="1%" class="msgheader" nowrap="nowrap" onDblClick="window.open().document.write('%%hfields%%')">Subject:</td>
    <td nowrap="nowrap">%%hdr:subject%%</td>
    <td width="1%" class="msgheader" nowrap="nowrap">Date:</td>
    <td nowrap="nowrap">%%JS:system.timestr(@@hdr:when_written_time@@)%%</td>
</tr>
<tr class="msg">
    <td width="1%" class="msgheader" nowrap="nowrap">From:</td>
    <td nowrap="nowrap">%%hdr:from%%</td>
    <td width="1%" class="msgheader" nowrap="nowrap">To:</td>
    <td nowrap="nowrap">%%hdr:to%%</td>
</tr>
<tr>
    <td colspan="4">
        <img src="/graphics/msg_td_teal.gif" alt="Table Graphic." width="100%" height="11" border="0" />
    </td>
</tr>
<tr>
    <td bgcolor="Black" colspan="4">
    <!-- <hr /> -->
            <pre class="leftalign">@@body@@</pre>
    <!-- <hr /> -->
    </td>
</tr>
<tr>
    <td colspan="4">
        <img src="/graphics/msg_td_teal.gif" alt="Table Graphic." width="100%" height="11" border="0" />
    </td>
</tr>
</table>
<table class="msg" width="95%">
<tr>
    <td class="msgnavleft">@@prevlink@@</td>
    <td class="msgnavmid">@@JS:template.can_post?'<a href="reply.ssjs?msg_sub=^^sub:code^^&amp;reply_to=^^hdr:number^^">Reply</a>':'Reply'@@</td>
    @@JS:template.can_delete?'<td class="msgnavmid"><a href="management.ssjs?Action=Delete+Selected+Message%28s%29&amp;msg_sub='+template.sub.code+'&amp;number='+template.hdr.number+'">Delete</a></td>':'<td class="msgnavmid">Delete</td>'@@
    <td class="msgnavright">@@nextlink@@</td>
</tr>
@@JS:if(template.replyto!=undefined) '<tr class="msg"><td colspan="4"><b>In Reply To: </b><a href="msg.ssjs?msg_sub='+sub+'&amp;message='+template.replyto.number+'">'+template.replyto.subject+'</a> ('+template.replyto.from+')</td></tr>'; else '';@@
@@JS:if(template.replies==undefined || template.replies.length<=0) '<!--'; else '';@@
<tr class="msg"><td colspan="4"><b>Replies: </b><<REPEAT replies>><a href="msg.ssjs?msg_sub=^^sub:code^^&amp;message=^^replies:number^^">%%replies:subject%%</a> (%%replies:from%%)<<END REPEAT replies>></td></tr>
@@JS:if(template.replies==undefined || template.replies.length<=0) '-->'; else '';@@
@@JS:if(template.attachments==undefined) '<!--'; else '';@@
<tr class="msg"><td colspan="4"><b>Attachments:</b><br />
<<REPEAT attachments>>
<a href="attachments.ssjs/^^sub:code^^/^^hdr:number^^/^^attachments:name^^">%%attachments:name%%</a><br />
<<END REPEAT attachments>>
</td>
</tr>
@@JS:if(template.attachments==undefined) '-->'; else '';@@
</table>

