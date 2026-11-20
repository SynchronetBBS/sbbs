/* Model an embedded provider's own public archive symbol. It must stay callable
 * inside the final shared object without becoming part of that object's exports. */
#include "../deucessh-portable.h"
DSSH_PUBLIC int dssh_visibility_archive(void);
DSSH_PUBLIC int
dssh_visibility_archive(void)
{
	return 42;
}
