function HelpFile(helpfile)
{
	this.file=new File(helpfile);
	this.help=function(section)
	{
		//TODO: display section help
	}
	this.load=function()
	{
		//TODO: parse help information from a single help file divided into sections
	}
	this.load();
}
