var connection=null;

function UpdateTerm(data)
{
	var term=document.getElementById("frame").contentDocument.getElementById("terminal");
	var win=document.getElementById("frame").contentWindow;

	term.innerHTML += data;
	win.scroll(0, term.clientHeight);
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
