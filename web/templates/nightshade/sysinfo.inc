<!-- $Id: sysinfo.inc,v 1.1 2006/02/22 02:16:17 runemaster Exp $ -->

<!-- Main Content -->  
    
<div id="contentcontainer3">
	<div id="content2">
	<div id="sinfo">
	<div align="center">

<table class="sysinfo" cellpadding="2" cellspacing="1">
  <tr>
    <td class="sysinforight">
      System:
    </td >
    <td class="sysinfo">
      %%system:name%%
    </td >
  </tr>
  <tr>
    <td class="sysinforight">
      QWK ID:
    </td >
    <td class="sysinfo">
      %%system:qwk_id%%
    </td >
  </tr>
<<REPEAT fidoaddrs>>
  <tr>
    <td class="sysinforight">
      FidoNet Address:
    </td >
    <td class="sysinfo">
      %%fidoaddrs:address%%
    </td >
  </tr>
<<END REPEAT fidoaddrs>>
  <tr>
    <td class="sysinforight">
      Location:
    </td >
    <td class="sysinfo">
      %%system:location%%
    </td >
  </tr>
  <tr>
    <td class="sysinforight">
      Operator:
    </td >
    <td class="sysinfo">
      %%system:operator%%
    </td >
  </tr>
  <tr>
    <td class="sysinforight">
      Nodes:
    </td >
    <td class="sysinfo">
      %%system:nodes%%
    </td >
  </tr>
  <tr>
    <td class="sysinforight">
      Node Phone:
    </td >
    <td class="sysinfo">
      Telnet and RLogin Only
    </td >
  </tr>
  <tr>
    <td class="sysinforight">
      Total Logons:
    </td >
    <td class="sysinfo">
      %%JS:system.stats.total_logons%%
    </td >
  </tr>
  <tr>
    <td class="sysinforight">
      Logons Today:
    </td >
    <td class="sysinfo">
      %%JS:system.stats.logons_today%%
    </td >
  </tr>
  <tr>
    <td class="sysinforight">
      Total Time:
    </td >
    <td class="sysinfo">
      %%system.stats.total_timeon%%
    </td >
  </tr>
  <tr>
    <td class="sysinforight">
      Time Today:
    </td >
    <td class="sysinfo">
      %%JS:system.stats.timeon_today%%
    </td >
  </tr>
</table>

<br />

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

<br />

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
		</div>
		</div>
	</div>
	<div class="cleardiv2"></div>
	</div>
</div>

<!-- end Main Content -->
