<p class="navigation"><a class="navigation" href="http://^^JS:system.inet_addr^^">Home</a> : <a class="navigation" href="/members/">Members</a> : <a class="navigation" href="index.ssjs">Groups</a> : <a class="navigation" href="subs.ssjs?msg_grp=^^group:name^^">%%group:description%%</a> :
<a class="navigation" href="msgs.ssjs?msg_sub=^^sub:code^^">%%sub:description%%</a>
<a class="navigation" href="subinfo.ssjs?msg_sub=^^sub:code^^">[?]</a><br /></p>
<table class="msg" border="0" width="95%">
<tr class="msg">
    <td class="msgheader" nowrap="nowrap">Subject:</td>
    <td nowrap="nowrap">%%hdr:subject%%</td>
    <td class="msgheader" nowrap="nowrap">To:</td>
    <td nowrap="nowrap">%%hdr:to%%</td>
</tr>
<tr class="msg">
    <td class="msgheader" nowrap="nowrap">From:</td>
    <td nowrap="nowrap">%%hdr:from%%</td>
    <td class="msgheader" nowrap="nowrap">Date:</td>
    <td nowrap="nowrap">%%JS:system.timestr(@@hdr:when_written_time@@)%%</td>
</tr>
<tr><td colspan="4"><hr /></td></tr>
<tr>
    <td colspan="4">
        <object class="msg" type="text/html" border="2" data="msgframe.ssjs/^^sub:code^^/^^hdr:number^^" standby="Loading Message ..." width="100%" height="450">
            <pre class="leftalign">@@body@@</pre>
        </object>
    </td>
</tr>
<tr>
<td class="msgnavleft">@@prevlink@@</td>
<td class="msgnavmid"><a href="reply.ssjs?msg_sub=^^sub:code^^&amp;reply_to=^^hdr:number^^">Reply</a></td>
@@JS:template.can_delete?'<td class="msgnavmid"><a href="management.ssjs?Action=Delete+Message%28s%29&amp;msg_sub=^^sub:code^^&amp;number=^^hdr:number^^">Delete</a></td>':'<td class="msgnavmid">Delete</td>'@@
<td class="msgnavright">@@nextlink@@</td>
</tr>
@@JS:if(template.replyto!=undefined) '<tr class="msg"><td colspan="4"><b>In Reply To: </b><a href="msg.ssjs?msg_sub='+sub+'&amp;message='+template.replyto.number+'">'+template.replyto.subject+'</a> ('+template.replyto.from+')</td></tr>'; else '';@@
@@JS:if(template.replies.length>0) '<tr class="msg"><td colspan="4"><b>Replies: </b>'; else '';@@
<<REPEAT replies>>
<a href="msg.ssjs?msg_sub=^^sub:code^^&amp;message=^^replies:number^^">%%replies:subject%%</a> (%%replies:from%%)
<<END REPEAT replies>>
</td>
</tr>
</table>
@@JS:if(template.attachments==undefined) '<!--'; else '';@@
<b>Attachments:</b><br />
<<REPEAT attachments>>
<a href="attachments.ssjs/^^sub:code^^/^^hdr:number^^/^^attachments:name^^">%%attachments:name%%</a><br />
<<END REPEAT attachments>>
@@JS:if(template.attachments==undefined) '-->'; else '';@@
<p class="msgcenter"> </p>
