<!-- $Id: subinfo.inc,v 1.1 2006/02/22 02:16:41 runemaster Exp $ -->

<!-- Main Content -->  
    
<div id="contentcontainer">
	<div id="content2">
	<div id="sbinfo">
<table class="subinfo" cellpadding="2" cellspacing="1">
  <tr>
    <td class="subinfbold">
      Description
    </td>
    <td class="subinfo">
      %%sub:description%%
    </td>
  </tr>
  <tr>
    <td class="subinfo">
      Newsgroup Name
    </td>
    <td class="subinfo">
      %%sub:newsgroup%%
    </td>
  </tr>
  <tr>
    <td class="subinfbold">
      Name
    </td>
    <td class="subinfo">
      %%sub:name%%
    </td>
  </tr>
  <tr>
    <td class="subinfbold">
      QWK Name
    </td>
    <td class="subinfo">
      %%sub:qwk_name%%
    </td>
  </tr>
  <tr>
    <td class="subinfbold">
      Is Moderated
    </td>
    <td class="subinfo">
      %%sub:is_moderated%%
    </td>
  </tr>
  <tr>
    <td class="subinfbold">
      Can Post
    </td>
    <td class="subinfo">
      %%sub:can_post%%
    </td>
  </tr>
  <tr>
    <td class="subinfbold">
      Can Read
    </td>
    <td class="subinfo">
      %%sub:can_read%%
    </td>
  </tr>
  <tr>
    <td class="subinfbold">
      Am Operator
    </td>
    <td class="subinfo">
      %%sub:is_operator%%
    </td>
  </tr>
  <tr>
    <td class="subinfbold">
      Max Msgs
    </td>
    <td class="subinfo">
      %%sub:max_msgs%%
    </td>
  </tr>
  <tr>
    <td class="subinfbold">
      Network Type
    </td>
    <td class="subinfo">
      %%type%%
    </td>
  </tr>
  @@JS:if(template.sub.settings & SUB_FIDO || template.sub.settings & SUB_QNET || template.sub.settings & SUB_PNET) '<tr class="subinfo"><td class="subinfbold">Tagline/Origin</td><td class="subinfonwr">'+template.tagline+'</td></tr>'; else '';@@
</table>
		</div>
		</div>
		</div>
	</div>
	<div class="cleardiv2"></div>
	</div>
</div>

<!-- end Main Content -->



