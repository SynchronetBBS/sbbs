<!-- $Id: posted.inc,v 1.1 2006/02/22 02:16:41 runemaster Exp $ -->

<!-- Main Content -->  

<div id="contentcontainer">
	<div id="content2">
	<div id="msgposted">
    
<table class="posted" cellspacing="1" cellpadding="2">
    <tbody>
        <tr>
        <td class="posted">
            @@JS:if(sub!='mail')'<h3 align="center">Message posted successfully.</h3>'; else '<h3 align="center">E-Mail sent successfully.</h3>';@@
            <p class="posted"><a href="msgs.ssjs?msg_sub=^^sub:code^^">Return to %%sub:description%%</a><br /><br /></p>
        </td>
        </tr>
    </tbody>
</table>
		</div>
		</div>
	</div>
	<div class="cleardiv2"></div>
	</div>
</div>

