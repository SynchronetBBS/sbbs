/* Error will hold the error message on a failed return... */
/* Must be at least 40 bytes long                          */
int http_get_fd(char *URL, size_t *len, char *error);
int sockreadline(int sock, char *buf, size_t length, char *error);
