var TeamProperties = [
			{
				 prop:"Password"
				,name:"Team Password"
				,type:"String:4"
				,def:""
			}
			,{
				 prop:"Members"
				,name:"Team Members"
				,type:"Array:4:Integer"
				,def:[0,0,0,0]
			}
			,{
				 prop:"Size"
				,name:"Number of members"
				,type:"Integer"
				,def:0
			}
		];

var teams=new RecordFile(fname("teams.dat"), TeamProperties);

function TeamMenu()
{
	function TeamHelp() {
		console.crlf();
		if(player.TeamNumber > 0)
			console.printfile(fname("team-member.asc"));
		else
			console.printfile(fname("team-none.asc"));
	}

	TeamHelp();

	while(1) {
		var values=new Array();
		if(player.TeamNumber > 0)
			values=['1','2','3','4','5','6','7','?'];
		else
			values=['1','2','3','?'];
		console.crlf();
		console.attributes="W";
		console.write("Team Command (?=help)? ");
		switch(InputFunc(values)) {
			case '?':
				TeamHelp();
				break;
			case '1':
				return;
			case '2':
				CreateTeam();
				break;
			case '3':
				JoinTeam();
				break;
			case '4':
				QuitTeam();
				break;
			case '5':
				TeamTransfer("Credits");
				break;
			case '6':
				TeamTransfer("Fighters");
				break;
			case '7':
				LocateTeam();
				break;
		}
	}
}

function LocateTeam()
{
	console.writeln("Locating Other Team members.");
	console.crlf();
	console.writeln("Name                                     Sector");
	console.writeln("====================                     ======");
	var count=0;
	var i;
	for(i=1;i<players.length; i++) {
		otherplayer=players.Get(i);
		if(otherplayer.Record!=player.Record
				&& otherplayer.UserNumber!=0
				&& otherplayer.TeamNumber==player.TeamNumber) {
			count++;
		}
		console.write(format("%-41s ",otherplayer.Alias));
		if(otherplayer.KilledBy != 0)
			console.write(" Dead!");
		else
			console.write(otherplayer.Sector);
		console.crlf();
	}
	console.crlf();
	if(count==0)
		console.writeln("None Found");
}

function CreateTeam()
{
	if(player.TeamNumber > 0) {
		console.writeln("You already belong to a Team! ");
		console.writeln("You must quit a team to form another Team.");
		return(false);
	}
	var password='';
	while(password.length != 4) {
		console.writeln("To form a Team you will need to input a 4 character Team Password");
		console.write("Enter Password [ENTER]=quit? ");
		password=console.getstr(4);
		if(password=='')
			return(false);
		if(password.length != 4)
			console.writeln("Password MUST be 4 characters");
	}
	console.crlf();
	console.writeln("REMEMBER YOUR TEAM PASSWORD!");
	var team=null;
	for(i=1; i<teams.length; i++) {
		team=teams.Get(i);
		if(team.Size > 0)
			break;
		team=null;
	}
	if(team==null)
		team=teams.New();
	team.Password=password;
	team.Size=1;
	team.Members[0]=player.Record;
	player.TeamNumber=team.Record;
	team.Put();
	player.Put();
	console.attributes="HYI";
	twmsg.writeln(player.Alias+" Created Team "+team.Record+" the " /* TNAM$ */);
	console.writeln("Team number [" + team.Record + "] CREATED!");
	console.attributes="HK";
	return(true);
}

function JoinTeam()
{
	if(player.TeamNumber > 0) {
		console.writeln("You already belong to a Team! ");
		console.writeln("You must quit one team to join another Team.");
		return(false);
	}

	console.write("What Team number do you wish to join (0=quit)? ");
	var tnum=InputFunc([{min:0,max:(teams.length-1)}]);
	if(!(tnum > 0 && tnum < teams.length))
		return(false);

	var team=teams.Get(tnum);
	if(team.Size > 3) {
		console.writeln("The Team you picked has a maximum amount of members!");
		return(false);
	}
	console.write("Please enter Password to Join Team #"+team.Record+" ? ");
	var pass=console.getstr(4);
	if(pass != team.Password) {
		console.writeln("Invalid Password entered!");
		return(false);
	}
	var i;
	for(i=0; i<team.Members.length; i++) {
		if(team.Members[i]==0) {
			team.Members[i]=player.Record;
			team.Size++;
			player.TeamNumber=team.Record;
			team.Put();
			player.Put();
			twmsg.writeln(player.Alias + " Joined Team "+team.Record);
			console.attributes="HC";
			console.writeln("Your Team info has been recorded!  Have fun!");
			console.attributes="HK";
			return(true);
		}
	}
	console.attributes="HR";
	console.writeln("Corrupt team detected on join!  Please notify the Sysop!");
	twmsg.writeln("!!! Team "+team.Record+" is corrupted (join)!");
	return(false);
}

function QuitTeam()
{
	if(player.TeamNumber != 0) {
		console.writeln("You don't belong to a Team!");
		return(false);
	}
	console.write("Are you sure you wish to quit your Team [N]? ");
	if(InputFunc(['Y','N'])=='Y') {
		var team=teams.Get(player.TeamNumber);
		player.TeamNumber=0;
		var i;
		for(i=0; i<team.Members.length; i++) {
			if(team.Members[i]==player.Record) {
				team.Members[i]=0;
				team.Size--;
				player.Put();
				team.Put();
				console.crlf();
				console.attributes="HG";
				console.writeln("You have been removed from Team play");
				console.attributes="HK";
				return(true);
			}
		}
	}
	console.attributes="HR";
	console.writeln("Corrupt team detected on quit!  Please notify the Sysop!");
	twmsg.writeln("!!! Team "+team.Record+" is corrupted (quit)!");
	return(false);
}

function TeamTransfer(type)
{
	console.write("Transfer "+type+" to another Team member [N]? ");
	if(InputFunc(['Y','N'])!='Y')
		return(false);
	if(player[type] < 1) {
		console.writeln("You don't have any "+type+".");
		return(false);
	}
	var i;

	var otherplayer=null;
	for(i=1;i<players.length; i++) {
		var otherloc=playerLocation.Get(i);
		if(otherloc.Sector==player.Sector) {
			otherplayer=players.Get(i);
			if(otherplayer.Sector==player.Sector
					&& otherplayer.Record!=player.Record
					&& otherplayer.KilledBy!=0
					&& otherplayer.UserNumber!=0
					&& otherplayer.TeamNumber==player.TeamNumber) {
				console.write("Transfer "+type+" to " + otherplayer.Alias + " (Y/[N])? ");
				if(InputFunc(['Y','N'])=='Y')
					break;
			}
			otherplayer=null;
		}
	}
	if(otherplayer==null) {
		console.writeln("There is no one else in this sector");
		return(false);
	}
	while(1) {
		console.writeln("You have" + player[type] + " "+type.toLowercase()+".");
		console.writeln(otherplayer.Alias + " has" + otherplayer[type] + " "+type.toLowercase()+".");
		console.write("How many "+type+" do you wish to Transfer? ");
		var transfer=InputFunc([{min:0,max:player[type]}]);
		if(transfer<1 || transfer > player[type])
			return(false);
		if(type=="Credits") {
			if(otherplayer.Credits + transfer > 25000) {
				console.writeln("You Team mate will have more than 25,000 credits");
				console.write("Do you wish to complete the Transfer [N]? ");
				if(InputFunc(['Y','N'])!='Y')
					return(false);
			}
		}
		else if(type=="Fighters") {
			if(otherplayer.Fighters + transfer > 9999)
				console.writeln("Maximum amount of fighters per player is 9999");
		}
	}
	otherplayer[type] += transfer;
	player[type] -= transfer;
	player.Put();
	otherplayer.Put();
	console.writeln(type+" Transfered!");
	return(true);
}

function InitializeTeams()
{
	uifc.pop("Teams");
	var team=teams.New();
	team.Put();
}
