function Logger(root, name)
{
	this.maxlogs=10;
	this.maxage=10;
	this.root=root;
	this.prefix=name;
	this.logfile;
	this.Init=function()
	{
		var now=time();
		var loglist=directory(this.root + this.prefix + "*.log"); 	
		for(l in loglist)
		{
			var then=file_date(loglist[l]);
			var diff=now-then;
			var daysold=((diff/60)/60)/24;
			if(daysold>this.maxage)
			{
				var remove=loglist[l];
				file_remove(remove);
			}
		}
		loglist=directory(this.root + this.prefix + "*.log"); 	
		var numlogs=loglist.length;
		for(n=0;n<numlogs;n++)
		{
			for(m = 0; m < (numlogs-1); m++) 
			{
				if(file_date(loglist[m]) < file_date(loglist[m+1])) 
				{
					holder = loglist[m+1];
					loglist[m+1] = loglist[m];
					loglist[m] = holder;
				}
			}
		}
		while(loglist.length>=this.maxlogs)
		{
			var remove=loglist.shift();
			file_remove(remove);
		}
		var num="01";
		while(file_exists(this.root+this.prefix+num+".log"))
		{
			num++;
			if(num<10) num="0"+num;
		}
		this.logfile=new File(this.root + this.prefix + num + ".log");
		this.Log("****" + system.datestr() +" - "+ system.timestr());
	}
	this.ScreenDump=function()	
	{
		console.clear();
		console.printfile(this.logfile.name);
		console.pause();
	}
	this.Log=function(text)
	{
		this.logfile.open(file_exists(this.logfile.name) ? 'r+':'w+');
		this.logfile.writeln(text);
		this.logfile.close();
	}
	this.Init();
}

