/*
 * Sets the current DOS text attribute to attr
 */
void door_textattr(int attr);

/*
 * Sends the string str to the remote system
 */
void door_outstr(const char *str);

/*
 * Sends a newline sequence to the remote
 */
void door_nl(void);

/*
 * Reads a single **UNSIGNED** character from the remote, returns -1 on error
 */
int door_readch(void);

/*
 * Clears the screen (either to black OR to corrent attr... doesn't matter
 */
void door_clearscreen(void);

