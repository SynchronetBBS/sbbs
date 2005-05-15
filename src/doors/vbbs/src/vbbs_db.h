#ifndef V3_DB_H
#define V3_DB_H

#ifdef FIXME
#include "mx_file.h"
#endif

///////////////////////////////////
//  holds all global system data
struct vbbs_db_system_record
{
	u32 last_maint;
};

typedef struct vbbs_db_system_record record;

// returns -1 on error, 0 on success
int db_init(void);
void db_shutdown(void);

// use for getting a snapshot of the data
// returns -1 on error, 0 on success
int db_read(record* rec);

// used for writing data safely
// returns -1 on error, 0 on success
int db_update(record* rec);
int db_commit(record* rec);

extern	int	db_file;

#define gSystemDB_init		db_init
#define gSystemDB_shutdown	db_shutdown
#define gSystemDB_read		db_read
#define gSystemDB_update	db_update
#define	gSystemDB_commit	db_commit
#define	gSystemDB_file		db_file

#endif
