<!-- $Id$ -->

<!-- Main Content -->  
    
    <td class="main" valign="top"><br />


<table class="sysinfo" cellpadding="2" cellspacing="2">
  <tr>
    <td class="sysinfo">
      System:
    </td >
    <td class="sysinfo">
      %%JS:system.name%%
    </td >
  </tr>
  <tr>
    <td class="sysinfo">
      QWK ID:
    </td >
    <td class="sysinfo">
      %%JS:system.qwk_id%%
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
      %%JS:system.location%%
    </td >
  </tr>
  <tr>
    <td class="sysinfo">
      Operator:
    </td >
    <td class="sysinfo">
      %%JS:system.operator%%
    </td >
  </tr>
  <tr>
    <td class="sysinfo">
      Nodes:
    </td >
    <td class="sysinfo">
      %%JS:system.nodes%%
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
      %%JS:system.stats.total_timeon%%
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
<p>Synchronet BBS for %%JS:system.platform%%
Version %%JS:system.full_version%%</p>
<p>Revision %%JS:system.revision%%
%%JS:system.beta_version%%
%%JS:system.compiled_when%%
%%JS:system.msgbase_lib%%
%%JS:system.compiled_with%%</p>
<p>%%JS:system.copyright%% - <a href="http://www.synchro.net/">http://www.synchro.net</a></p>
<p>%%JS:system.js_version%%</p>
<p>%%JS:system.socket_lib%%</p>
<p>%%JS:system.os_version%%</p>
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
