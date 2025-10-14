/*
 * System functions
 */

void System_Init(void);
/*
 * Initializes whole system.
 */

void System_Close(void);
/*
 * purpose  Closes down the system, no matter WHERE it is called.
 *          Should be foolproof.
 */

void System_Error(char *szErrorMsg);
/*
 * purpose  To output an error message and close down the system.
 *          This SHOULD be run from anywhere and NOT fail.  It should
 *          be FOOLPROOF.
 */

void System_Maint(void);

void Config_Init(void);
