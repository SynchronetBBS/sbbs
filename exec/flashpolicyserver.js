// $Id$

/***

Flash Policy Server
by Shawn Rapp

This is a service javascript addon that adds a Flash Policy Server to Synchronet BBS version 3 and later.
This script was designed to support FlashTerm hosted on a Synchronet Website.

Copy the file flashpolicyserver.js to your Synchronet's exec directory (example C:\sbbs\exec)
Put the flashpolicy.xml to the ctrl directory (example C:\sbbs\ctrl)

Now edit ctrl\services.ini 
Goto Services->Configure and than click the button "Edit Services Configuration File"
Than add the bellow
	===================
	[FlashPolicy]
	Port=843
	MaxClients=10
	Options=0
	Command=flashpolicyserver.js
	===================

Restart Synchronet's Services server.

Download http://www.flashterm.com/files/flashterm_full.zip
Extract the files in a new directory web\html\flashterm (example C:\sbbs\web\html\flashterm)
Edit settings.xml to your settings.
Now open a browser to your Synchronet's website and goto the \flashterm directory.
Example http://thedhbbs.com/flashterm/

***/

// Write a string to the client socket
function write(str)
{
	client.socket.send(str);
}

function read_policy()
{
	f = new File("../ctrl/flashpolicy.xml");
	if(!f.open("r")) 
		return;
	var txt = f.readAll().toString();
	txt = txt.replace(/(\r|\n|\t|,)/g, '');
	f.close();
	return(txt);
}


terminator = false;
request = "";
//<policy-file-request/>

while(client.socket.is_connected && !terminator)
{
	inByte = client.socket.recvBin(1);
	inChar = String.fromCharCode(inByte);
	if((inByte == 0) || (inChar == " "))
	{
		terminator = true;
	}
	else
	{
		request = request + inChar;
	}
}
if(client.socket.is_connected == false)
{
	exit();
}

//request = client.socket.recv(22);

request = truncsp(request);
log(LOG_DEBUG, "client request: " + request);

if(request == "<policy-file-request/>")
{
	policy = read_policy();
	write(policy);
	exit();
}