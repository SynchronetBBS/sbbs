<!-- $Id: leftnav.inc,v 1.20 2006/02/25 16:43:51 runemaster Exp $ -->

    <!-- Left Navigation (Includes Main table start) -->
        
<table align="center" bgcolor="#EEEEEE" width="90%"  border="0" cellspacing="0" cellpadding="0">
  <tr>
    <td width="1%" valign="top" align="left"><img alt="" src="@@image_dir@@/tl_curve_grey.gif" height="6" width="6" /></td>
    <td>
    <table width="100%"  border="0" cellspacing="0" cellpadding="0">
  <tr>
  
    <td valign="top" width="1%">
    <div id="pageNav"> 
        <div id="sectionLinks"> 
			<a href="/nodelist.ssjs">Who's Online</a><<REPEAT leftnav>>@@leftnav:html@@<<END REPEAT leftnav>><a href="@@ftp_url@@%%ftpidx%%">File Libraries</a>
    	</div>

<!-- start Nodelisting -->

      @@JS:if(!template.node_list.length || http_request.virtual_path=="/nodelist.ssjs")'<!--';@@
Who's Online now ...<br /><table class="left_nodelist"><tbody><<REPEAT node_list>><tr><td class="left_nodelist"><a href="mailto:@@node_list:email@@">%%node_list:name%%</a> %%node_list:action%%</td></tr><<END REPEAT node_list>></tbody></table>
@@JS:if(!template.node_list.length || http_request.virtual_path=="/nodelist.ssjs")'-->';@@
     </div>
   </td>
 
                          
<!-- end Left Navigation -->
            
