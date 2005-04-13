<!-- $Id$ -->

<!-- Main Content -->  
  <td class="main" valign="top"><br />
   
<table class="userstats" border="0" cellpadding="2" cellspacing="2">
<tr>
    <<REPEAT first>>
        @@first:html@@
    <<END REPEAT first>>
</tr>
<tr>
  <td class="userstats">Time on:</td>
  <td class="userstats">@@total_timeon@@</td>
  <td class="userstats">Today:</td>
  <td class="userstats"> 
  <table width="100%" border="0" cellpadding="0" cellspacing="0">
    <tr>
        <<REPEAT second>>
            @@second:html@@
        <<END REPEAT second>>
    </tr>
  </table>
  </td>
  <td class="userstats">Last Call:</td>
  <td class="userstats">
    <table width="100%" border="0" cellpadding="0" cellspacing="0">
    <tr>
        <<REPEAT third>>
            @@third:html@@
        <<END REPEAT third>>
    </tr>
    </table>
  </td>
  <td class="userstats">Extra:</td>
  <td class="userstats">@@extra_time@@</td>
</tr>
<tr>
  <td class="userstats">Logons:</td>
  <td class="userstats">@@total_logons@@</td>
  <td class="userstats">Today:</td>
  <td class="userstats">
    <table width="100%" border="0" cellpadding="0" cellspacing="0">
    <tr>
        <<REPEAT fourth>>
            @@fourth:html@@
        <<END REPEAT fourth>>
    </tr>
    </table>
  </td>
  <td class="userstats">Posts:</td>
  <td class="userstats">
    <table width="100%" border="0" cellpadding="0" cellspacing="0">
    <tr>
        <<REPEAT fifth>>
            @@fifth:html@@
        <<END REPEAT fifth>>
    </tr>
    </table>
  </td>
  <td class="userstats">Today:</td>
  <td class="userstats">@@posts_today@@</td>
</tr>
<tr>
    <<REPEAT sixth>>
        @@sixth:html@@
    <<END REPEAT sixth>>
</tr>
<tr>
  <td class="userstats" colspan="8">&nbsp;</td>
</tr>
  <tr>
    <td class="userstats">Uploads:</td>
    <td class="userstats" align="right">@@bytes_uploaded@@</td>
    <td class="userstats" colspan="6">bytes in @@files_uploaded@@ files</td>
  </tr>
  <tr>
    <td class="userstats">Downloads:</td>
    <td class="userstats" align="right">@@bytes_downloaded@@</td>
    <td class="userstats" colspan="6">bytes in @@files_downloaded@@ files</td>
  </tr>
  <tr>
    <td class="userstats">Credits:</td>
    <td class="userstats" align="right">@@credits@@</td>
    <td class="userstats" colspan="6">free: @@freecreds@@ (@@freeday@@ per day)</td>
  </tr>
  <tr>
    <td class="userstats">Minutes:</td>
    <td class="userstats" align="right">@@tbank@@</td>
    <td class="userstats" colspan="6">&nbsp;</td>
  </tr>
</table>
<br />
<br />

<!-- end Main Content -->

