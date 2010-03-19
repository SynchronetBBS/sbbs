var connection=null;

function writeText(data)
{
	var term=document.getElementById("frame").contentDocument.getElementById("terminal");
	var win=document.getElementById("frame").contentWindow;

	term.innerHTML += data;
	win.scroll(0, term.clientHeight);
}

function handleCtrl(byte)
{
	var term=document.getElementById("frame").contentDocument.getElementById("terminal");
	var win=document.getElementById("frame").contentWindow;
	const sound = Components.classes["@mozilla.org/sound;1"].createInstance(Components.interfaces.nsISound);

	switch(byte) {
		case '\n':
		case '\t':
		case '\r':
			writeText(byte);
			break;
		case '\b':
			term.innerHTML = term.innerHTML.replace(/[^\x00-\x1F]$/,'');
			break;
		case '\x0c':	// Formfeed -- clear screen
			term.innerHTML = '';
			break;
		case '\x07':	// BEL
			sound.beep();
			break;
	}
}

function UpdateTerm(data)
{
	while(data.length) {
		data=data.replace(/^([^\x00-\x1F\x80-\x9f]*)/, function(matched, text) {
			writeText(text);
			return '';
		});
		if(data.length) {
			while(data.charCodeAt(0) < 32) {
				handleCtrl(data.substr(0,1));
				data=data.substr(1);
			}
		}
	}
}

function doTerm(host, port)
{
	var ConnOpt=document.getElementById("MainConnectionMenu-connect").disabled=true;
	var DisconnOpt=document.getElementById("MainConnectionMenu-disconnect").disabled=false;
	connection=new RLoginConnection(host,port,UpdateTerm);
}

function endTerm()
{
	if(connection != null)
		connection.close();
	connection=null;

	var ConnOpt=document.getElementById("MainConnectionMenu-connect").disabled=false;
	var DisconnOpt=document.getElementById("MainConnectionMenu-disconnect").disabled=true;
}

function dumpObj(obj,name)
{
	var i;

	for(i in obj) {
//		if(typeof(obj[i]=='object')) {
//			dumpObj(obj[i],name+"["+i+"]");
//		}
//		else {
			alert(name+"["+i+"]="+obj[i]);
//		}
	}
}

function translateKey(key)
{
	var str;

	if(key.charCode==0) {
		if(key.keyCode < 32)
			return String.fromCharCode(key.keyCode);
		alert("Unhandled key code "+key.keyCode);
		return('');
	}
	return String.fromCharCode(key.charCode);
}

function sendKey(key)
{
	if(connection != null) {
		connection.write(translateKey(key));
	}
}
