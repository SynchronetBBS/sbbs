/*
	Javascript clone of Aspect Technologies Post-It Notes (BAJA)
	Inspired by all post-it notes apps, one liners, books and Jesus
	Thanks to Jas Hud for being a big whiny douche
	by Matt Johnson (2008)

	SAMPLE CONFIGURATION:

	[Post-It Notes]
	 1: Name                       Post-It Notes
	 2: Internal Code              POSTIT
	 3: Start-up Directory         \sbbs\xtrn\postit\
	 4: Command Line               *..\xtrn\postit\postit.js postit 60
	 5: Clean-up Command Line
	 6: Execution Cost             None
	 7: Access Requirements
	 8: Execution Requirements
	 9: Multiple Concurrent Users  Yes
	10: Intercept Standard I/O     No
	11: Native (32-bit) Executable No
	12: Use Shell to Execute       No
	13: Modify User Data           No
	14: Execute on Event           Logon
	15: Pause After Execution      No
	16: BBS Drop File Type         None
	17: Place Drop File In         Node Directory
	18: Time Options...

`	Please note that the program takes two arguments which determine the minimum
	minimum security level to add notes and also the root file name for data storage.

	while this file has been uploaded to the \sbbs\exec\ repository, it may be advisable
	to move it to its own directory in \sbbs\xtrn\ so that the related data files and log files
	can be easily maintained.
	
	Command line parameters:

	 4: Command Line               *..\xtrn\postit\postit.js <root filename> <minimum security level>
	
*/
load("sbbsdefs.js");
load("funclib.js");
load("logging.js");

var security_required=argv[1]; 	//SECOND PASSED ARGUMENT FOR SECURITY LEVEL REQUIRED TO POST
var root_name=argv[0]; 			//ROOT DATAFILE NAME, USED TO SEPARATE INSTANCES OF THE PROGRAM
var root_dir;
try { barfitty.barf(barf); } catch(e) { root_dir = e.fileName; }
root_dir = root_dir.replace(/[^\/\\]*$/,'');
var logger=new Logger(root_dir,root_name);

var current=new MessageList(root_dir,root_name,".dat");
var history=new MessageList(root_dir,root_name,".his");

var max_msg_length=3; 			//MAXIMUM MESSAGE LENGTH FOR 
var default_msg_color="\1g\1h"; //DEFAULT MESSAGE COLOR 
var sysop_mode=false;

function main()
{
	if(user.compare_ars("SYSOP") || (bbs.sys_status&SS_TMPSYSOP))
	{
		sysop_mode=true;
		max_msg_length=20;
		Log("Sysop mode enabled");
	}
	var can_add=(user.security.level>=security_required);
	if(!current.Load()) return false;
	while(1)
	{
		current.Display();
		console.putmsg(format("\r\n\1n\1c%s[\1hH\1n\1c]istory, or [\1hEnter\1n\1c] to [\1hQ\1n\1c]uit: ",can_add?"[\1hA\1n\1c]dd, ":""),P_SAVEATR);		 
		var cmd=console.getkey(K_NOECHO|K_NOSPIN);
		switch(cmd.toUpperCase())
		{
		case 'H':
			ShowHistory(history);
			break;
		case 'A':
			if(user.security.level>=security_required)
				AddNote();
			break;
		case 'Q':
		case '\r':
			return;
		default:
			break;
		}
	}
}
function AddNote()
{
	var msg=[];
	var add_to_msg=true;
	var finished=false;
	var buffer="";
	
	console.putmsg("\r\n\1n\1cEnter up to \1h" + max_msg_length + "\1n\1c lines of text\1h:\1g\r\n",P_SAVEATR);
	while(msg.length<max_msg_length && !finished)
	{
		var next_line=false;
		if(buffer.length) console.putmsg(buffer,P_SAVEATR);
		while(console.strlen(strip_ctrl(buffer))<79 && !next_line && !finished)
		{
			add_to_msg=true;
			var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO);
			switch(k)
			{
				case '\b':
					add_to_msg=false;
					if(buffer.length>0) 
					{
						console.left(buffer.length);
						buffer=buffer.substr(0,buffer.length-1);
						console.write(buffer);
						console.cleartoeol();
					}
					else if(msg.length>0) 
					{
						buffer=msg.pop();
						buffer=buffer.substr(0,buffer.length-1);
						console.up();
						console.putmsg("\r" + default_msg_color);
						console.write(buffer,P_SAVEATR);
						console.cleartoeol();
					}
					else return;
					break;
				case ' ':
					console.putmsg("\r" + buffer,P_SAVEATR);
					console.cleartoeol();
					break;
				case '\r':
					add_to_msg=false;
					if(buffer=="") finished=true;
					else next_line=true;
					console.left(buffer.length);
					console.putmsg(buffer,P_SAVEATR);
					console.cleartoeol();
					console.crlf();
					break;
				default:
					break;
			}
			if(add_to_msg) 
			{
				console.putmsg(k);
				buffer+=k;
			}
		}
		var buffer_length=console.strlen(strip_ctrl(buffer));
		if(buffer_length>0) 
		{
			if(	buffer_length==79 && 
				buffer.lastIndexOf(" ")<buffer.length &&
				buffer.lastIndexOf(" ")>0)
			{
				msg.push(RemoveSpaces(buffer.substr(0,buffer.lastIndexOf(" "))));
				console.up();
				print("\r");
				console.putmsg(msg[msg.length-1],P_SAVEATR);
				console.cleartoeol();
				console.crlf();
				buffer=GetLastWord(buffer);
			}
			else 
			{
				msg.push(truncsp(buffer));
				buffer="";
			}
		}
	}
	if(msg.length && console.yesno("\1n\1cSave this message?")) current.AddPost(msg);
	UpdateHistory(current.Truncate());
}
function ShowHistory(list)
{
	if(!list.Load()) return false;
	var temp_list=list.array.slice();
	Log("number of messages: " + temp_list.length);

	var pages=SetPageBreaks(temp_list);
	var start=0;
	
	while(1)
	{
		console.clear();
		var index=pages[start].start;
		while(index<=pages[start].end)
		{
			temp_list[index].Display();
			index++;
		}
		console.putmsg("\r\n\1n\1c[\1hR\1n\1c]everse, [\1h\1n\1c/\1h\1n\1c], [\1hQ\1n\1c]uit, or [\1hEnter\1n\1c] to continue: ");
		var cmd=console.getkey(K_NOECHO|K_NOSPIN);
		switch(cmd.toUpperCase())
		{
		case 'Q':
			return;
		case 'R':
			temp_list=temp_list.reverse();
			start=0;
			pages=SetPageBreaks(temp_list);
			break;
		case KEY_UP:
			if(start>0) start--;
			break;
		case KEY_DOWN:
		case '\r':
			if(pages.length-start>1) start++;
			break;
		default:
			continue;
		}
	}
}
function SetPageBreaks(list)
{
	var pages=[];
	var start=0;
	var end=0;
	while(start<list.length)
	{
		end=CountSegment(start,list);
		logger.Log("setting page break: " + end);
		pages.push({start:start,end:end});
		start=end+1;
	}
	return pages;
}
function CountSegment(index,array)
{
	var lines=0;
	while(lines<console.screen_rows-2)
	{
		if(!array[index]) return index-1;
		lines+=array[index].message.length+1;
		if(lines>console.screen_rows-2) return index-1;
		else if(lines==console.screen_rows-2) return index;
		index++;
	}
	return index;
}
function FormatDate(date)
{
	return strftime("\1n%m\1y\1h/\1n%d\1y\1h/\1n%y \1k\1h[\1n%H\1y\1h:\1n%M\1y\1h:\1n%S\1k\1h]",date);
}
function UpdateHistory(array)
{
	for(msg in array)
	{
		history.array.push(array[msg]);
	}
	history.FileOut('a+',array);
}

//MESSAGE LIST OBJECT
function MessageList(root,name,ext)
{
	this.array=[];
	this.root=root;
	this.name=name;
	this.file=new File(this.root + this.name + ext);
	
	this.Truncate=function()
	{
		var truncated=[];
		var total_lines=this.CountLines();
		var trim=total_lines-(console.screen_rows-2);
		while(trim>0)
		{
			Log("truncating message: " + this.array[0].message);
			trim-=this.array[0].message.length+1;
			truncated.push(this.array.shift());
		}
		this.FileOut('w+',this.array);
		return truncated;
	}
	this.AddPost=function(msg)
	{	
		Log("adding post: " + msg);
		this.Load(); //UPDATE POSTS FROM FILE BEFORE ADDING NEW POST TO MAINTAIN ORDER & CURRENCY
		this.array.push(new Post(user.alias,time(),msg));
	}
	this.Load=function()
	{
		this.array=[];
		this.file.open(file_exists(this.file.name) ? 'a+':'w+');
		if(this.file.is_open)
		{
			while(!this.file.eof)
			{
				var text=strip_ctrl(this.file.readln());
				if(text=="null") break;
				var author=text.substring(0,text.indexOf("@"));
				var date=text.substr(text.indexOf("@")+1);
				var message=[];
				while(!this.file.eof)
				{
					var temp=this.file.readln();
					if(strip_ctrl(temp)=="null" || strip_ctrl(temp)=="") break;
					message.push(temp);
				}
				this.array.push(new Post(author,date,message));
			}
			this.file.close();
			return true;
		}
		return false;
	}
	this.CountLines=function()
	{
		var count=0;
		for(msg in this.array)
		{
			count+=this.array[msg].message.length+1;
		}
		return count;
	}
	this.Display=function()
	{
		console.clear();
		for(post in this.array)
		{
			this.array[post].Display();
		}
	}
	this.FileOut=function(mode,array)
	{
		this.file.open(mode);
		for(msg in array)
		{
			this.file.writeln(array[msg].author + "@" + array[msg].date);
			for(line in array[msg].message)
			{
				this.file.writeln(array[msg].message[line]);
			}
			this.file.writeln();
		}
		this.file.close();
	}
}
//MESSAGE OBJECT
function Post(author,date,message)
{
	this.author=author;
	this.date=date;
	this.message=message;
	this.header;
	this.Display=function()
	{
		console.putmsg(this.header + default_msg_color,P_SAVEATR);
		for(msg in this.message)
		{
			console.putmsg(this.message[msg],P_SAVEATR);
			console.crlf();
		}
		console.putmsg("\1n",P_SAVEATR);
	}
	this.CreateHeaderOld=function()
	{
		var head_txt="\1k\1h\xB4\1w" + this.author + " \1k[\1y\1k]" + FormatDate(this.date);
		var head_len=console.strlen(head_txt);
		var leader_len=console.screen_columns-head_len;
		var leader="\1n\1h\1k";
		for(lead=0;lead<leader_len;lead++)
		{
			leader+="\xC4";
		}
		this.header=leader+head_txt;
	}
	this.CreateHeader=function()
	{
		var head_txt="\1k\1h\xC4\xC4\xB4\1n" + this.author + " \1h\1k\1y\1k\xC3";
		var date_txt="\xB4" + FormatDate(this.date);
		var head_len=console.strlen(head_txt + date_txt);
		var leader_len=console.screen_columns-head_len;
		var leader="\1n\1h\1k";
		for(lead=0;lead<leader_len;lead++)
		{
			leader+="\xC4";
		}
		this.header=head_txt + leader + date_txt;
	}
	this.CreateHeader();
}

function Log(msg)
{
	logger.Log(msg);
}

main();
