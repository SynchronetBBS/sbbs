#include <stdio.h>
#include "platform.h"

#include "defines.h"
#include "door.h"
#include "semfile.h"
#include "video.h"

bool CreateSemaphor(uint16_t Node)
{
	char content[32];
	snprintf(content, sizeof(content), "Node: %" PRIu16 "\n", Node);
	return plat_CreateSemfile("online.flg", content);
}

void WaitSemaphor(uint16_t Node)
{
	int count = 0;
	while (!CreateSemaphor(Node)) {
		if (count++ == 0)
			LogDisplayStr("Waiting for online flag to clear\n");
		if (count == 60)
			count = 0;
		plat_Delay(1000);
	}
}

void RemoveSemaphor(void)
{
	plat_DeleteFile("online.flg");
}
