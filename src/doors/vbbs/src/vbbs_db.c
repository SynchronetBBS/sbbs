/*****************************************************************************
 *
 * File ..................: v3_db.cpp
 * Purpose ...............: File DB routines
 *
 *****************************************************************************
 * Copyright (C) 1999-2002  Darryl Perry
 * Copyright (C) 2002-2003  Michael Montague
 *
 * This file is part of Virtual BBS.
 *
 *****************************************************************************/

#include <fcntl.h>
#include <sys/stat.h>
#include <filewrap.h>

#include "vbbs.h"
#include "v3_defs.h"
#include "vbbs_db.h"

// our system db object
#ifdef FIXME
vbbs_db_system gSystemDB;
#endif

int	db_file=-1;

/////////////////////////////
// vbbs_db_system functions
int db_init()
{
	// build the path
	char path[_MAX_PATH];
	sprintf(path, "%ssystem.vb3", /*global*/ gamedir);
	// open the file
	if ((db_file=open(path,O_RDWR|O_BINARY|O_CREAT,S_IREAD|S_IWRITE)) < 0)
	{
		printf("unabled to open %s\n", path);
		return -1;
	}

	return 0;
}

void db_shutdown()
{
	// closes the file and removes all locks
	close(db_file);
}

int db_read(record* rec)
{
	// seek to read location
	long offset = 0L * sizeof(record);
	if (lseek(db_file, offset, SEEK_SET) < 0L)
		return -1;
	// read the record
	if (read(db_file, rec, sizeof(record)) != sizeof(record))
		return -1;

	return 0;
}

int db_update(record* rec)
{
	long offset = 0L * sizeof(record);
	if (lock(db_file, offset, sizeof(record)))
		return -1;
	if (lseek(db_file, offset, SEEK_SET) < 0L)
	{
		unlock(db_file, offset, sizeof(record));
		return -1;
	}
	if (read(db_file, rec, sizeof(record)) != sizeof(record))
	{
		unlock(db_file, offset, sizeof(record));
		return -1;
	}

	return 0;
}

int db_commit(record* rec)
{
	long offset = 0L * sizeof(record);
	if (lseek(db_file, offset, SEEK_SET) < 0L)
	{
		unlock(db_file, offset, sizeof(record));
		return -1;
	}
	if (write(db_file, rec, sizeof(record)) != sizeof(record))
	{
		unlock(db_file, offset, sizeof(record));
		return -1;
	}
	if (unlock(db_file, offset, sizeof(record)))
		return -1;

	return 0;
}
