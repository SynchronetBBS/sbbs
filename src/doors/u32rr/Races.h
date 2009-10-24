#ifndef _RACES_H_
#define _RACES_H_

enum race {
	Human,
	Hobbit,
	Elf,
	HalfElf,
	Dwarf,
	Troll,
	Orc,
	Gnome,
	Gnoll,
	Mutant,
	RACE_COUNT
};

extern const char *UCRaceName[RACE_COUNT];
extern const char *LCRaceName[RACE_COUNT];
extern const char *PluralRaceName[RACE_COUNT];

#endif
