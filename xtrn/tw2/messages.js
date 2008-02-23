var PlayerMessageProperties = [
			{
				 prop:"Read"
				,name:"Message has been read"
				,type:"Boolean"
				,def:true
			}
			,{
				 prop:"To"
				,name:"Player the message is to"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"From"
				,name:"Player the message is from"
				,type:"SignedInteger"
				,def:0
			}
			,{
				 prop:"Destroyed"
				,name:"Number of Fighters Destroyed"
				,type:"Integer"
				,def:0
			}
		];

var RadioMessageProperties = [
			{
				 prop:"Read"
				,name:"Message has been read"
				,type:"Boolean"
				,def:false
			}
			,{
				 prop:"To"
				,name:"Player the message is to"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"From"
				,name:"Player the message is from"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Message"
				,name:"Test of the message"
				,type:"String:74"
				,def:""
			}
		];

var twpmsg=new RecordFile(fname("twpmesg.dat"), PlayerMessageProperties);
var twrmsg=new RecordFile(fname("twrmesg.dat"), RadioMessageProperties);

function ReadPMsg()
{
	console.writeln("The following happened to your ship since your last time on:");
	var count=0;
	for(i=0; i<twpmsg.length; i++) {
		var msg=twpmsg.Get(i);

		if(msg.To==player.Record && !msg.Read) {
			count++;
			if(msg.From==-98)
				console.writeln("A deleted player destroyed "+msg.Destroyed+" fighters.");
			else if(msg.From==-1) {
				console.attributes="R";
				console.writeln("The Cabal destroyed "+msg.Destroyed+" fighters.");
			}
			else {
				var otherplayer=players.Get(msg.From);

				console.writeln(otherplayer.Alias+" "+(otherplayer.TeamNumber?" Team["+otherplayer.TeamNumber+"] ":"")+"destroyed "+msg.Destroyed+" of your fighters.");
			}
			msg.Read=true;
			msg.Put();
		}
	}
	if(count==0)
		console.writeln("Nothing");
	return(count);
}

function RadioMessage(from, to, msg)
{
	var i;

	var rmsg;
	for(i=0; i<twrmsg.length; i++) {
		var rmsg=twrmsg.Get(i);
		if(rmsg.Read)
			break;
		rmsg=null;
	}
	if(rmsg==null)
		rmsg=twrmsg.New();
	rmsg.Read=false;
	rmsg.From=from;
	rmsg.To=to;
	rmsg.Message=msg;
	rmsg.Put();
	console.writeln("Message sent.");
	return;
}

function ReadRadio()
{
	var i;
	var rmsg;
	var count=0;
	
	console.crlf();
	console.writeln("Checking for Radio Messages sent to you.");
	for(i=0; i<twrmsg.length; i++) {
		var rmsg=twrmsg.Get(i);
		if(rmsg.Read)
			continue;
		if(rmsg.To != player.Record)
			continue;
		console.write("Message from ");
		if(rmsg.From > 0) {
			var p=players.Get(rmsg.From);
			console.write(p.Alias);
		}
		else {
			console.write("A deleted player");
		}
		console.writeln(":");
		console.writeln(rmsg.Message);
		rmsg.Read=true;
		rmsg.Put();
		count++;
	}
	if(count < 1)
		console.writeln("None Received.");
}

function SendRadioMessage()
{
	console.crlf();
	console.writeln("Warming up sub-space radio.");
	mswait(500);
	console.crlf();
	console.write("Who do you wish to send this message to (search string)? ");
	var sendto=console.getstr(42);
	var p=MatchPlayer(sendto);
	if(p==null)
		return(false);
	console.writeln("Tuning in to " + p.Alias + "'s frequency.");
	console.crlf();
	console.writeln("Due to the distances involved, messages are limited to 74 chars.");
	console.writeln("Pressing [ENTER] with no input quits");
	console.writeln("  [------------------------------------------------------------------------]");
	console.attributes="HY";
	console.write("? ");
	console.attributes="W";
	var msg=console.getstr(74);
	if(msg==null)
		return(false);
	msg=msg.replace(/\s*$/,'');
	if(msg=='')
		return(false);
	RadioMessage(player.Record, p.Record, msg);
	return(true);
}
