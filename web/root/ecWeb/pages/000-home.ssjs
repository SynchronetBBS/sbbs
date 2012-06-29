// Home

// 000-home.ssjs from ecWeb v2 for Synchronet BBS 3.15+
// by Derek Mullin (echicken -at- bbs.electronicchicken.com)

// This is the default 'home' page.  Edit it as you see ift, but leave the file
// name the same (or, if you must change the filename, modify ../index.ssjs to
// reflect that change.

// Embed fTelnet
print("<span class=titleFont>Telnet</span><br><br>");
load(webIni.webRoot + '/fTelnet/fTelnet.ssjs');

// Embed lightIRC
print("<br><br><span class=titleFont>IRC Chat</span><br><br>");
load(webIni.webRoot + '/lightirc/lightirc.ssjs');
