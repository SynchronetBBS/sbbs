Note that OpenDoors installs signal handlers and *REQUIRES* that they be called
for the following signals:

SIGALRM - Drives the od_kernel
SIGIO   - Drives user input

If you install your own handles, be absolutely sure that they call the old
handlers as well.
