<!-- $Id: subinfo.inc,v 1.5 2006/01/31 23:14:21 runemaster Exp $ -->

<!-- Main Content -->  
    
    <td class="main" valign="top"><br />

<table class="subinfo">
  <tr class="subinfo">
    <td class="subinfbold">
      Description
    </td>
    <td>
      %%sub:description%%
    </td>
  </tr>
  <tr class="subinfo">
    <td class="subinfo">
      Newsgroup Name
    </td>
    <td>
      %%sub:newsgroup%%
    </td>
  </tr>
  <tr class="subinfo">
    <td class="subinfbold">
      Name
    </td>
    <td>
      %%sub:name%%
    </td>
  </tr>
  <tr class="subinfo">
    <td class="subinfbold">
      QWK Name
    </td>
    <td>
      %%sub:qwk_name%%
    </td>
  </tr>
  <tr class="subinfo">
    <td class="subinfbold">
      Is Moderated
    </td>
    <td>
      %%sub:is_moderated%%
    </td>
  </tr>
  <tr class="subinfo">
    <td class="subinfbold">
      Can Post
    </td>
    <td>
      %%sub:can_post%%
    </td>
  </tr>
  <tr class="subinfo">
    <td class="subinfbold">
      Can Read
    </td>
    <td>
      %%sub:can_read%%
    </td>
  </tr>
  <tr class="subinfo">
    <td class="subinfbold">
      Am Operator
    </td>
    <td>
      %%sub:is_operator%%
    </td>
  </tr>
  <tr class="subinfo">
    <td class="subinfbold">
      Max Msgs
    </td>
    <td>
      %%sub:max_msgs%%
    </td>
  </tr>
  <tr class="subinfo">
    <td class="subinfbold">
      Network Type
    </td>
    <td>
      %%type%%
    </td>
  </tr>
  @@JS:if(template.sub.settings & SUB_FIDO || template.sub.settings & SUB_QNET || template.sub.settings & SUB_PNET) '<tr class="subinfo"><td class="subinfbold">Tagline/Origin</td><td class="subinfonwr">'+template.tagline+'</td></tr>'; else '';@@
</table>
<br />
<br />

<!-- end Main Content -->



