<p class="navigation"><a class="navigation" href="http://^^JS:system.inet_addr^^">Home</a> : <a class="navigation" href="/members/">Members</a> : <a class="navigation" href="index.ssjs">Groups</a> : <a class="navigation" href="subs.ssjs?msg_grp=^^group:number^^">%%group:description%%</a> :
<a class="navigation" href="msgs.ssjs?msg_grp=^^group:number^^&amp;msg_sub=^^sub:code^^">%%sub:description%%</a>
<a class="navigation" href="subinfo.ssjs?msg_grp=^^group:number^^&amp;msg_sub=^^sub:code^^">[?]</a><br></p>
<table class="msg" border="0" width="95%">
<tr>
    <td class="msgheader" nowrap>Subject:</td>
    <td nowrap>%%hdr:subject%%</td>
    <td class="msgheader" nowrap>To:</td>
    <td nowrap>%%hdr:to%%</td>
</tr>
<tr>
    <td class="msgheader" nowrap>From:</td>
    <td nowrap>%%hdr:from%%</td>
    <td class="msgheader" nowrap>Date:</td>
    <td nowrap>%%JS:system.timestr(@@hdr:when_written_time@@)%%</td>
</tr>
<tr><td colspan="4"><hr></td></tr>
<tr>
    <td colspan="4">
        <object class="msg" type="text/html" border=2 data="msgframe.ssjs/^^group:number^^/^^sub:code^^/^^hdr:number^^" standby="Loading Message ..." width="100%" height="450">
            <pre class="leftalign">@@body@@</pre>
        </object>
    </td>
</tr>
</table>
@@JS:if(template.attachments==undefined) '<!--'; else '';@@
<b>Attachments:</b><br>
<<REPEAT attachments>>
<a href="attachments.ssjs/^^group:number^^/^^sub:code^^/^^hdr:number^^/^^attachments:name^^">%%attachments:name%%</a><br>
<<END REPEAT attachments>>
@@JS:if(template.attachments==undefined) '-->'; else '';@@
<p class="msgcenter"><a href="reply.ssjs?msg_grp=^^group:number^^&amp;msg_sub=^^sub:code^^&amp;reply_to=^^hdr:number^^">Reply</a></p>

