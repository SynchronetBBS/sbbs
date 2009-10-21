#ifndef _STRUCTS_H_
#define _STRUCTS_H_

#include <inttypes.h>

#define MAXMSPELLS	6

struct monster {
	char	name[31];	// Name of creature
	long	weapnr;		// Weapon (from weapon data file) used
	long	armnr;		// Armour (from data file) used
	bool	grabweap;	// Can weapon be taken
	bool	grabarm;	// Car armour be taken
	char	phrase[71];	// Intro phrase from monster
	int	magicres;	// Magic resistance
	int	strength;
	int	defence;
	bool	wuser;		// Weapon User
	bool	auser;		// Armour User
	long	hps;
	char	weapon[41];	// Name of weapon
	char	armor[41];	// Name of armour
	bool	disease;	// Infected by a disease?
	long	weappow;	// Weapon Power
	long	armpow;		// Armour power
	int	iq;
	uint8_t	evil;		// Evilne4ss (0-100%)
	uint8_t	magiclevel;	// The higher this is, the better the magic is
	int	mana;		// Manna remaining
	int	maxmana;
	bool	spell[MAXMSPELLS];	// Monster Spells

	long	punch;		// Temporary battle var(!)
	bool	poisoned;	// Temporary battle var(!)
	int	target;		// Temp. battle var
};

#endif
