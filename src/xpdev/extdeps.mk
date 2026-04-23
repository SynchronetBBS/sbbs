# Historical per-object Cryptlib dependency hints for ini_file.c,
# genwrap.c, and os_info.c.  Now obsolete — none of those files
# include cryptlib.h after the xp_crypt migration.  File intentionally
# left (effectively) empty so the make rules that -include it don't
# error out on a missing file.
