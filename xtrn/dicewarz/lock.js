function	Locked(fileName,timeOut)
{
	var fname=(root+fileName+".lck");
	if(file_exists(fname))
	{
		if(!timeOut) return true;
		var max_attempts=20;
		for(var attempt=0;attempt<max_attempts;attempt++){
			if(file_exists(fname))	{
				mswait(250);
			}
			else return false;
		}
	}
	else 
		return false;
	return true;
}
function	Lock(fileName)
{
	var fname=(root+fileName+".lck");
	var lockfile=new File(fname);
		lockfile.open('wx', false); 
	if(!lockfile.is_open) 
		return false;
	else  {
		lockfile.close();
		return fileName;
	}
}
function	Unlock(fileName)
{
	if(fileName==-1 || !fileName) 
		return;
	var fname=(root+fileName+".lck");
	file_remove(fname);
}
function	UnlockAll()
{
	var lockList=directory(root + "*.lck"); 		
	if(lockList.length) {
		for(var lf in lockList)	{	
			file_remove(lockList[lf]);
		}
	}
}

js.on_exit("if(activeGame != undefined && activeGame != '') Unlock(activeGame);");
