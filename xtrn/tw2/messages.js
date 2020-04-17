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

function ReadPMsg()
{
	console.writeln("The following happened to your ship since your last time on:");
	// TODO: Fix this up...
	db.lock(Settings.DB,'updates',LOCK_WRITE);
	var updates=db.read(Settings.DB,'updates');
	var count=0;
	var msgstr='';
	for(i=0; i<updates.length; i++) {
		var msg=updates[i];

		if(msg.To==player.Record && !msg.Read) {
			count++;
			if(msg.From==-98)
				msgstr += "A deleted player destroyed "+msg.Destroyed+" fighters.\r\n";
			else if(msg.From==-1) {
				msgstr += "The Cabal destroyed "+msg.Destroyed+" fighters.\r\n";
			}
			else {
				var otherplayer=players.Get(msg.From);

				msgstr += otherplayer.Alias+" "+(otherplayer.TeamNumber?" Team["+otherplayer.TeamNumber+"] ":"")+"destroyed "+msg.Destroyed+" of your fighters.";
			}
			updates.splice(i,1);
			i--;
		}
	}
	db.write(Settings.DB,'updates',updates);
	db.unlock(Settings.DB,'updates');
	if(count==0)
		console.writeln("Nothing");
	else
		console.write(msgstr);
	return(count);
}

function RadioMessage(from, to, msg)
{
	var i;

	var rmsg={};
	rmsg.Read=false;
	rmsg.From=from;
	rmsg.To=to;
	rmsg.Message=msg;
	db.push(Settings.DB,'radio',rmsg,LOCK_WRITE);
	console.writeln("Message sent.");
	return;
}

function ReadRadio()
{
	var i;
	var count=0;
	var msgstr='';
	
	console.crlf();
	console.writeln("Checking for Radio Messages sent to you.");
	db.lock(Settings.DB,'radio',LOCK_WRITE);
	var radio=db.read(Settings.DB,'radio');
	for(i=0; i<radio.length; i++) {
		if(radio[i].Read)
			continue;
		if(radio[i].To != player.Record)
			continue;
		msgstr += "Message from ";
		if(radio[i].From > 0) {
			var p=players.Get(radio[i].From);
			msgstr += p.Alias;
		}
		else {
			msgstr += "A deleted player";
		}
		msgstr += ":\r\n";
		msgstr += radio[i].Message+"\r\n";
		radio.splice(i,1);
		i--;
		count++;
	}
	db.write(Settings.DB,'radio',radio);
	db.unlock(Settings.DB,'radio');
	if(count < 1)
		console.writeln("None Received.");
	else
		console.write(msgstr);
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

function ResetAllMessages()
{
	var i;

	if(this.uifc) uifc.pop("SysOp Messages");
	db.write(Settings.DB,'log',[],LOCK_WRITE);
	db.push(Settings.DB,'log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:" TW 500 initialized"},LOCK_WRITE);

	if(this.uifc) uifc.pop("Player Messages");
	db.write(Settings.DB,'updates',[],LOCK_WRITE);

	if(this.uifc) uifc.pop("Radio Messages");
	db.write(Settings.DB,'radio',[],LOCK_WRITE);
}
