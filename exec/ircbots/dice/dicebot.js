/*
	IRC bot module - by Matt Johnson (MCMLXXIX) - 2010
	This is a sample IRC bot module to demonstrate how such modules work.
	More than one module can be loaded into the main IRC bot at a time,
	but be careful when setting your bot commands (in '<botname>_commands.js')
	if there are duplicate command names, they will be superseded in the order
	in which they are loaded (in the order they are listed in ircbot.ini).
*/
working_dir=this.dir;

/* This method is executed by the IRCBot during its "save_everything()" cycle */
function save()
{
	//var s_file=new File(working_dir + "file.ini");
	//if(!s_file.open(file_exists(s_file.name)?"r+":"w+")) return false;
	
	/*
		do some work, save some data.....
	*/
	
	//s_file.close();
}
/* This method is executed by the IRCBot during its "main()" loop, once per cycle (DO NOT MAKE A LOOP) */
function main(srv)
{	
	/*	Do some work here.
		You can use a timer to time events or process scores.
		You have access to server methods and properties.	*/
}

/* Module objects here: Everything here is loaded within the context of the "Modules()" object */