#ifndef _CLASSES_H_
#define _CLASSES_H_

enum class {
	Alchemist,
	Assassin,
	Barbarian,
	Bard,
	Cleric,
	Jester,
	Magician,
	Paladin,
	Ranger,
	Sage,
	Warrior,
	MAXCLASSES
};

extern const char *UCClassNames[MAXCLASSES];
extern const char *LCClassNames[MAXCLASSES];
extern const char *PluralClassNames[MAXCLASSES];

#endif
