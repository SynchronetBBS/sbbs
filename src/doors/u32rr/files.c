#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>

#include "macros.h"
#include "files.h"

#include "todo.h"

struct map_file {
	int		fd;
	off_t	len;
	void	*data;
};

struct map_file poison_map;
struct poison	*poison;

struct map_file player_map;
struct player	*player;
struct player	*players;

struct map_file npc_map;
struct player	*npcs;

struct map_file onliner_map;
extern struct onliner	*onliner;
extern struct onliner	*onliners;

struct map_file object_map;
struct player	*objects;

static size_t align(size_t val)
{
	int		pagesize=getpagesize();

	if(val % pagesize)
		val=val-(val%pagesize)+pagesize;
	return(val);
}

static void map_file(char *name, struct map_file *map, size_t len)
{
	map->fd=open(name, O_RDWR);
	if(map->fd == -1)
		CRASHF("Error opening map \"%s\"\n", name);
	map->data=mmap(0, align(len), PROT_READ|PROT_WRITE, MAP_SHARED, map->fd, 0);
	if(map->data==MAP_FAILED)
		CRASH;
}

void open_files(void)
{
	map_file("poison.bin", &poison_map, MAX_POISON * sizeof(struct poison));
	poison=poison_map.data;
	map_file("players.dat", &player_map, MAX_PLAYERS * sizeof(struct player));
	players=player_map.data;
	map_file("npcs.dat", &npc_map, MAX_NPCS * sizeof(struct player));
	npcs=npc_map.data;
	map_file("objects.dat", &object_map, MAX_OBJECTS * sizeof(struct object));
	objects=object_map.data;
}

void create_poison_file(void)
{
	map_file("poison.bin", &poison_map, MAX_POISON * sizeof(struct poison));
	poison=poison_map.data;
	strcpy(poison[0].name, "None");
	poison[0].strength=0;
	poison[0].cost=0;

	strcpy(poison[1].name, "Snake Bite");
	poison[1].strength=4;
	poison[1].cost=1500;

	strcpy(poison[2].name, "Xaminah Stir");
	poison[2].strength=10;
	poison[2].cost=11000;

	strcpy(poison[3].name, "Zargothicia");
	poison[3].strength=14;
	poison[3].cost=25000;

	strcpy(poison[4].name, "Diamond Sting");
	poison[4].strength=17;
	poison[4].cost=100000;

	strcpy(poison[5].name, "Mynthia");
	poison[5].strength=21;
	poison[5].cost=300000;

	strcpy(poison[6].name, "Exxodus");
	poison[6].strength=41;
	poison[6].cost=550000;

	strcpy(poison[7].name, "Wolf Spit");
	poison[7].strength=51;
	poison[7].cost=850000;

	strcpy(poison[8].name, "Joy of Death");
	poison[8].strength=71;
	poison[8].cost=1250000;

	strcpy(poison[9].name, "Eusebius Cure");
	poison[9].strength=81;
	poison[9].cost=1500000;

	strcpy(poison[10].name, "Yxaxxiantha");
	poison[10].strength=85;
	poison[10].cost=1900000;

	strcpy(poison[11].name, "Polluted Lung");
	poison[11].strength=90;
	poison[11].cost=3000000;

	strcpy(poison[12].name, "Postheria");
	poison[12].strength=96;
	poison[12].cost=6000000;

	strcpy(poison[13].name, "Red Sledge");
	poison[13].strength=100;
	poison[13].cost=9000000;

	strcpy(poison[14].name, "Mullamia");
	poison[14].strength=110;
	poison[14].cost=9100000;

	strcpy(poison[15].name, "Cobra High");
	poison[15].strength=115;
	poison[15].cost=9200000;

	strcpy(poison[16].name, "Stomach Claw");
	poison[16].strength=120;
	poison[16].cost=9300000;

	strcpy(poison[17].name, "Fasanathievh");
	poison[17].strength=125;
	poison[17].cost=9400000;

	strcpy(poison[18].name, "Urpathxiaveth");
	poison[18].strength=130;
	poison[18].cost=9500000;

	strcpy(poison[19].name, "Dragon Flame");
	poison[19].strength=135;
	poison[19].cost=9600000;

	strcpy(poison[20].name, "Usilamahs Bite");
	poison[20].strength=140;
	poison[20].cost=9700000;

	strcpy(poison[21].name, "Devils Cure");
	poison[21].strength=145;
	poison[21].cost=9900000;
}

void create_files(void)
{
	create_poison_file();
}
