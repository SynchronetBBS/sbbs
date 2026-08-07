#include "OpenDoor.h"

int main(void)
{
    return od_control_get() == &od_control ? 0 : 1;
}
