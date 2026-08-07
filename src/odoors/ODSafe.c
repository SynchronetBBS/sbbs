/* Internal checked-size helpers. */
#include <stddef.h>

#include "ODSafe.h"

int ODSizeAdd(size_t left, size_t right, size_t *result)
{
   size_t maximum = (size_t)-1;

   if(result == NULL || right > maximum - left)
      return(0);
   *result = left + right;
   return(1);
}

int ODSizeMultiply(size_t left, size_t right, size_t *result)
{
   size_t maximum = (size_t)-1;

   if(result == NULL || (right != 0 && left > maximum / right))
      return(0);
   *result = left * right;
   return(1);
}
