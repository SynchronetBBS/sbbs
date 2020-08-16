<!-- $Id: sysinfo.inc,v 1.7 2006/01/31 23:13:43 runemaster Exp $ -->

<!-- Main Content -->  
    
    <td class="main" valign="top"><br />


<table class="sysinfo" cellpadding="2" cellspacing="2">
  <tr>
    <td class="sysinfo">
      System:
    </td >
    <td class="sysinfo">
      %%system:name%%
    </td >
  </tr>
  <tr>
    <td class="sysinfo">
      QWK ID:
    </td >
    <td class="sysinfo">
      %%system:qwk_id%%
    </td >
  </tr>
<<REPEAT fidoaddrs>>
  <tr>
    <td class="sysinfo">
      FidoNet Address:
    </td >
    <td class="sysinfo">
      %%fidoaddrs:address%%
    </td >
  </tr>
<<END REPEAT fidoaddrs>>
  <tr>
    <td class="sysinfo">
      Location:
    </td >
    <td class="sysinfo">
      %%system:location%%
    </td >
  </tr>
  <tr>
    <td class="sysinfo">
      Operator:
    </td >
    <td class="sysinfo">
      %%system:operator%%
    </td >
  </tr>
  <tr>
    <td class="sysinfo">
      Nodes:
    </td >
    <td class="sysinfo">
      %%system:nodes%%
    </td >
  </tr>
  <tr>
    <td class="sysinfo">
      Node Phone:
    </td >
    <td class="sysinfo">
      Telnet and RLogin Only
    </td >
  </tr>
  <tr>
    <td class="sysinfo">
      Total Logons:
    </td >
    <td class="sysinfo">
      %%JS:system.stats.total_logons%%
    </td >
  </tr>
  <tr>
    <td class="sysinfo">
      Logons Today:
    </td >
    <td class="sysinfo">
      %%JS:system.stats.logons_today%%
    </td >
  </tr>
  <tr>
    <td class="sysinfo">
      Total Time:
    </td >
    <td class="sysinfo">
      %%system.stats.total_timeon%%
    </td >
  </tr>
  <tr>
    <td class="sysinfo">
      Time Today:
    </td >
    <td class="sysinfo">
      %%JS:system.stats.timeon_today%%
    </td >
  </tr>
</table>
<div align="center">
<p>Synchronet BBS for %%system:platform%%
Version %%system:full_version%%</p>
<p>Revision %%system:revision%%
%%system:beta_version%%
%%system:compiled_when%%
%%system:msgbase_lib%%
%%system:compiled_with%%</p>
<p>%%system:copyright%% - <a href="http://www.synchro.net/">http://www.synchro.net</a></p>
<p>%%system:js_version%%</p>
<p>%%system:socket_lib%%</p>
<p>%%system:os_version%%</p>
<table>
<tr>
    <td><pre class="leftalign">@@sysinfo@@</pre></td>
</tr>
<tr>
    <td>&nbsp;</td>
</tr>
<tr>
    <td><pre class="leftalign">@@logon@@</pre></td>
</tr>
</table>
</div>

<!-- end Main Content -->
