#ifndef THE_CLANS__SEMFILE___H
#define THE_CLANS__SEMFILE___H

bool CreateSemaphor(uint16_t Node);
void RemoveSemaphor(void);
void WaitSemaphor(uint16_t Node);

#endif
