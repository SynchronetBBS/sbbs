/*
	Javascript Help File browser 
	by Matt Johnson - 2009

	SAMPLE HELP FILE: 

	[BEGIN] 
	//This section gets discarded
	[SECTION 1 TITLE]
	Section 1 help text.
	No blank spaces required between sections.
	[SECTION 2 TITLE]
	Section 2 help text.
	Can be any number of lines
	......
	[SECTION 3 TITLE]
	blehhhh
	[END]
	//This section also gets discarded
*/

function HelpFile(helpfile)
{
	this.file=new File(helpfile);
	this.sections=new Array();
	
	this.initSections=function()
	{
		var sections=this.file.iniGetSections();
		var positions=new Array;
		for(var s=0;s<sections.length;s++)
		{
			this.file.iniGetValue(sections[s],null);
			positions.push(this.file.position);
			this.sections.push(new Section(sections[s]));
		}
		return positions;
	}
	this.loadSections=function(positions)
	{
		this.sections.shift();
		for(var p=0;p<positions.length;p++)
		{
			if(!this.sections[p]) break;
			var text=new Array();
			this.file.position=positions[p];
			while(this.file.position<positions[p+1])
			{
				text.push(this.file.readln());
			}
			text.pop();
			this.sections[p].text=text;
		}
		this.sections.pop();
	}
	this.list=function(fullscreen)
	{
		if(fullscreen)
		{
			for(var s=1;s<=this.sections.length;s++)
			{
				writeln("\1y\1h" + s + ":\1n\1h " + this.sections[s-1].title);
			}
		}
		else
		{
			var list=[];
			for(var s=1;s<=this.sections.length;s++)
			{
				list.push("\1y\1h" + s + ":\1n\1h " + this.sections[s-1].title);
			}
			return list;
		}
	}
	this.show=function(section)
	{
		console.clear();
		if(!section)
		{
			for(var s in this.sections)
			{
				var sec=this.sections[s];
				writeln("\1n\1h" + sec.title + "\r\n");
				for(var ln in sec.text)
				{
					writeln("\1n" + sec.text[ln]);
				}
				console.crlf();
			}
		}
		else
		{
			for(var ln in this.sections[section-1].text)
			{
				writeln("\1n" + this.sections[section-1].text[ln]);
			}
		}
	}
	function Section(title)
	{
		this.title=title;
		this.text;
	}
	
	this.file.open('r',true);
	this.loadSections(this.initSections());
	this.file.close();
}
