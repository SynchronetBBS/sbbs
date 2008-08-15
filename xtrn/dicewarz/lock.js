function	Locked(fileName,timeOut)
{
	var fname=(game_dir+fileName+".lck");
	if(file_exists(fname))
	{
		if(!timeOut) return true;
		var max_attempts=20;
		for(attempt=0;attempt<max_attempts;attempt++)
		{
			if(file_exists(fname))
			{
				mswait(250);
			}
			else return false;
		}
	}
	else return false;
	return true;
}
function	Lock(fileName)
{
	var fname=(game_dir+fileName+".lck");
	var lockfile=new File(fname);
		lockfile.open('we', false); 
	if(!lockfile.is_open) 
		return false;
	else 
	{
		lockfile.close();
		activeGame=fileName;
		js.on_exit("Unlock(activeGame)");
		return fileName;
	}
}
function	Unlock(fileName)
{
	if(fileName==-1 || !fileName) return;
	var fname=(game_dir+fileName+".lck");
	file_remove(fname);
}
function	UnlockAll()
{
	var lockList=directory(game_dir + "*.lck"); 		
	if(lockList.length)
	{
		for(lf in lockList)
		{	
			file_remove(lockList[lf]);
		}
	}
}
