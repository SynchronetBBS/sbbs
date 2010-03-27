var currtext='';
var stringsequence={storing:false,value:'',type:0};

function writeHTML(data)
{
	var frame=document.getElementById("frame");
	var doc=frame.contentDocument;
	var win=frame.contentWindow;
	var term=doc.getElementById("terminal");
	var bottom;
	var offe;

	currtext += data;
	if(term==null)
		return;
	term.innerHTML = currtext;

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
	if(stringsequence.storing) {
		stringsequence.value += data;
	}
	else {
		if(Zuul.escapeHTML) {
			data=data.replace(/&/g,'&amp;');
			data=data.replace(/</g,'&lt;');
			data=data.replace(/>/g,'&gt;');
			data=data.replace(/'/g,'&apos;');
			data=data.replace(/"/g,'&quot;');
			data=data.replace(/ /g,'&nbsp;');
		}
		writeHTML(data);
	}
}

function handleString(obj)
{
	var doc=document.getElementById("frame").contentDocument;
	var term=doc.getElementById("terminal");
	var win=document.getElementById("frame").contentWindow;
	var tmp;

	switch(obj.type) {
		case '\x90':
			tmp = obj.value.match(/^([^\s]+) (.*)/);
			if(tmp != null) {
				switch(tmp[1]) {
					case 'GET':
						win.location=tmp[2];
						break;
				}
			}
			break;
		case '\x9f':
			eval(obj.value, win);
			break;
	}
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
			if(currtext.length > 0) {
				switch(currtext.charAt(currtext.length-1)) {
					case ';':
						currtext = currtext.replace(/&[^&]+;$/,'');
						term.innerHTML=currtext;
						break;
					case '>':
						break;
					default:
						currtext = currtext.replace(/.$/,'');
						term.innerHTML=currtext;
						break;
				}
			}
			break;
		case '\x0c':	// Formfeed -- clear screen
			currtext = '';
			term.innerHTML=currtext;
			break;
		case '\x07':	// BEL
			sound.beep();
			break;
		case '\x85':	// NEL (Next Line)
			writeHTML("<br>");
			break;
		case '\x98':	// SOS (Start Of String)
		case '\x90':	// DCS (Device Control String)
		case '\x9d':	// OSC (Operating System Command)
		case '\x9e':	// PM  (Privacy Message)
		case '\x9f':	// APC (Application Program Command)
			stringsequence.storing=true;
			stringsequence.value='';
			stringsequence.type=byte;
			break;
		case '\x9c':	// ST  (String Terminator)
			stringsequence.storing=false;
			handleString(stringsequence);
			break;
	}
}

function UpdateTerm(data)
{
	var val;

	while(data.length) {
		data=data.replace(/^([^\x00-\x1F\x80-\x9f]+)/, function(matched, text) {
			writeText(text);
			return '';
		});
		if(data.length) {
			val=data.charCodeAt(0);
			while(val < 32 || (val >= 0x90 && val <= 0x9f)) {
				handleCtrl(data.substr(0,1));
				data=data.substr(1);
				val=data.charCodeAt(0);
			}
		}
	}
}

function doTerm(host, port)
{
	var ConnOpt=document.getElementById("MainConnectionMenu-connect").disabled=true;
	var DisconnOpt=document.getElementById("MainConnectionMenu-disconnect").disabled=false;
	Zuul.connection=new RLoginConnection(host,port,UpdateTerm);
	currtext='';
	document.getElementById("frame").contentWindow.location="chrome://ZuulTerm/content/default.html";
	Zuul.escapeHTML=true;
}

function endTerm()
{
	if(Zuul.connection != null)
		Zuul.connection.close();
	Zuul.connection=null;

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
	if(Zuul.connection != null) {
		Zuul.connection.write(translateKey(key));
	}
}
