var connection=null;

function writeHTML(data)
{
	var frame=document.getElementById("frame");
	var doc=frame.contentDocument;
	var win=frame.contentWindow;
	var term=doc.getElementById("terminal");
	var bottom;
	var offe;

	term.innerHTML += data;

	var winVisible=win.innerHeight-win.scrollMaxY;
	/* Scroll to bottom of terminal container */
	if(term.scroll != undefined && term.clientHeight != undefined) {
		term.scroll(0, term.clientHeight);
	}
	else if(term.scrollHeight != undefined && term.scrollTop != undefined) {
		top=term.scrollHeight-term.clientHeight;
		if(top < 0)
			top=0;
		if(term.scrollTop != top)
			term.scrollTop=top;
	}
	else {
		alert("Scroll problems!");
	}

	/* Scroll window so that the bottom of the terminal container is visible */
	bottom=term.offsetHeight;
	offe=term;
	do {
		bottom += offe.offsetTop;
		offe = offe.offsetParent;
	} while(offe && offe.tagName != 'BODY');
	top = win.scrollY;
	if(bottom > win.scrollY+winVisible) {
		top=bottom-winVisible;
	}
	else if(bottom < win.scrollY) {
		/* TODO: Just show some of the bottom */
		top=bottom-winVisible;
	}
	if(top < 0)
		top=0;
	if(top != win.scrollY)
		win.scroll(win.scrollX,top);
}

function writeText(data)
{
	data=data.replace(/&/g,'&amp;');
	data=data.replace(/</g,'&lt;');
	data=data.replace(/>/g,'&gt;');
	data=data.replace(/'/g,'&apos;');
	data=data.replace(/"/g,'&quot;');
	data=data.replace(/ /g,'&nbsp;');
	writeHTML(data);
}

function handleCtrl(byte)
{
	var term=document.getElementById("frame").contentDocument.getElementById("terminal");
	var win=document.getElementById("frame").contentWindow;
	const sound = Components.classes["@mozilla.org/sound;1"].createInstance(Components.interfaces.nsISound);

	switch(byte) {
		case '\n':
			writeHTML('<br>');
			break;
		case '\t':
		case '\r':
			break;
		case '\b':
			if(term.innerHTML.length > 0) {
				switch(term.innerHTML.charCodeAt(term.innerHTML.length-1)) {
					case ';':
						term.innerHTML = term.innerHTML.replace(/&[^&]+;$/,'');
						break;
					case '>':
						break;
					default:
						term.innerHTML = term.innerHTML.replace(/.$/,'');
						break;
				}
			}
			break;
		case '\x0c':	// Formfeed -- clear screen
			term.innerHTML = '';
			break;
		case '\x07':	// BEL
			sound.beep();
			break;
		case '\x85':	// NEL (Next Line)
			writeHTML("<br>");
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
