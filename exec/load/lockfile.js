// lockfile.js

// File locking library for JS

// $Id$

// @format.tab-size 4, @format.use-tabs true

var LockedFiles=new Object();
js.on_exit("UnlockAll()");

function Lock(filename, lockid, forwrite, timeout)
{
	var readlock=new File(filename+".lock."+lockid);
	var writelock=new File(filename+".lock");
	var endtime=system.timer+timeout;

	/* Do we already hold a lock on this file? */
	if(LockedFiles[filename] != undefined) {
		/* do we hold THIS lock on this file? */
		if(LockedFiles[filename].lockid == lockid) {
			if(LockedFiles[filename].forwrite == forwrite)
				return(true);
			/*
			 * We currently hold a write lock and are requesting a read lock...
			 */
			if(forwrite==false) {
				if(readlock.open("we")) {
					readlock.close();
					LockedFiles[filename].forwrite==false;
					file_remote(writelock.name);
				}
				else {
					log(LOG_ERROR, "!LOCK ERROR! -- cannot read lock a file we have write locked!");
					return(false);
				}
			}
			else {
				/* We want to upgrade a read lock to a write lock... */
			}
		}
		else {
			/* We currently hold some other lock on this file.  As a result, we CANNOT relock with a different ID */
			log(LOG_WARNING, "!LOCK ERROR! -- attempt to lock "+filename+" with ID '"+lockid+"' while we hold a lock with ID '"+LockedFiles[filename].lockid+"'");
			return(false);
		}
	}

	while(system.timer <= endtime) {
		/* No lock can be created when there is a write lock */
		if(!file_exists(filename+".lock")) {
			if(forwrite) {
				/* We are locking for write... attempt to create write lock file */
				if(writelock.open("we")) {
					writelock.close();
					/* If we are upgading from a read lock, delete our read lock */
					if(LockedFiles[filename]!=undefined)
						file_remove(readlock.name);
					/* We have got the lock... wait for all read locks to close */
					while(directory(filename+".lock.*").length) {
						mswait(1);
						if(system.timer > endtime) {
							/* If we were upgrading, restor our old lock... */
							if(LockedFiles[filename]!=undefined) {
								if(readlock.open("we")) {
									readlock.close();
								}
							}
							file_remove(writelock.name);
							return(false);
						}
					}
					LockedFiles[filename]=new Object();
					LockedFiles[filename].forwrite=true;
					LockedFiles[filename].lockid=lockid;
					return(true);
				}
			}
			else {
				if(readlock.open("we")) {
					readlock.close();
					LockedFiles[filename]=new Object();
					LockedFiles[filename].forwrite=false;
					LockedFiles[filename].lockid=lockid;
					return(true);
				}
			}
		}
		mswait(1);
	}

	return(false);
}

function Unlock(filename)
{
	var readlock;
	var writelock=new File(filename+".lock");

	if(LockedFiles[filename]==undefined)
		return;
	readlock=new File(filename+".lock."+LockedFiles[filename].lockid);

	if(LockedFiles[filename].forwrite)
		file_remove(writelock.name);
	else
		file_remove(readlock.name);

	delete LockedFiles[filename];
}

function UnlockAll()
{
	for(filename in LockedFiles) {
		Unlock(filename);
	}
}
