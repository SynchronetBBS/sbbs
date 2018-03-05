<!-- $Id$ -->

<div id="contentcontainer">
	<div id="content2">
		<h3 align='center'>fTelnet</h3>

<!-- fTelnet --> 

	<div id="fTelnetContainer" class="fTelnetContainer"></div>
	<script>document.write('<script src="//embed-v2.ftelnet.ca/js/ftelnet-loader.norip.noxfer.js?v=' + (new Date()).getTime() + '"><\/script>');</script>
	<script>
		var Options = new fTelnetOptions();
		Options.BareLFtoCRLF = false;
		Options.BitsPerSecond = 57600;
		Options.ConnectionType = 'telnet';
		Options.Emulation = 'ansi-bbs';
		Options.Enter = '\r';
		Options.Font = 'CP437';
		Options.ForceWss = false;
		Options.Hostname = '@@HostName@@';
		Options.LocalEcho = false;
		Options.Port = @@Port@@;
		Options.ScreenColumns = 80;
		Options.ScreenRows = 25;
		var fTelnet = new fTelnetClient('fTelnetContainer', Options);
	</script>

<!-- end fTelnet -->

	</div>
</div>