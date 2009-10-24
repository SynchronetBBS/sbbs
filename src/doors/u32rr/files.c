#include <sys/limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>

#include "macros.h"
#include "files.h"

#include "todo.h"

struct map_file poison_map;
struct poison	*poison;

struct map_file player_map;
struct player	*player;
struct player	*players;

struct map_file npc_map;
struct player	*npcs;

struct map_file onliner_map;
struct onliner	*onliner;
struct onliner	*onliners;

struct map_file king_map;
struct king		*king;

struct map_file abody_map;
struct object	*abody;

struct map_file armor_map;
struct armor	*armor;

struct map_file arms_map;
struct object	*arms;

struct map_file body_map;
struct object	*body;

struct map_file drink_map;
struct object	*drink;

struct map_file face_map;
struct object	*face;

struct map_file feets_map;
struct object	*feets;

struct map_file food_map;
struct object	*food;

struct map_file hands_map;
struct object	*hands;

struct map_file head_map;
struct object	*head;

struct map_file legs_map;
struct object	*legs;

struct map_file neck_map;
struct object	*neck;

struct map_file rings_map;
struct object	*rings;

struct map_file shields_map;
struct object	*shields;

struct map_file waist_map;
struct object	*waist;

struct map_file weapons_map;
struct object	*weapons;

struct map_file weapon_map;
struct weapon	*weapon;

struct map_file monster_map;
struct monster	*monster;

struct map_file level_map;
struct level	*levels;

static size_t align(size_t val)
{
	int		pagesize=getpagesize();

	if(val % pagesize)
		val=val-(val%pagesize)+pagesize;
	return(val);
}

void map_file(char *name, struct map_file *map, size_t len)
{
	struct stat st;

	map->fd=open(name, O_RDWR);
	if(len==SIZE_T_MAX) {
		if(fstat(map->fd, &st)!=0)
			CRASH;
		len=st.st_size;
	}
	if(map->fd == -1)
		CRASHF("Error opening map \"%s\"\n", name);
	map->len=align(len);
	map->data=mmap(0, map->len, PROT_READ|PROT_WRITE, MAP_SHARED, map->fd, 0);
	if(map->data==MAP_FAILED)
		CRASH;
}

void unmap_file(struct map_file *map)
{
	msync(map->data, map->len, MS_SYNC);
	munmap(map->data, map->len);
	close(map->fd);
}

void open_files(void)
{
	map_file("abody.dat", &abody_map, SIZE_T_MAX);
	abody=abody_map.data;

	map_file("armor.dat", &armor_map, SIZE_T_MAX);
	armor=armor_map.data;

	map_file("arms.dat", &arms_map, SIZE_T_MAX);
	arms=arms_map.data;

	map_file("body.dat", &body_map, SIZE_T_MAX);
	body=body_map.data;

	map_file("drink.dat", &drink_map, SIZE_T_MAX);
	drink=drink_map.data;

	map_file("face.dat", &face_map, SIZE_T_MAX);
	face=face_map.data;

	map_file("feets.dat", &feets_map, SIZE_T_MAX);
	feets=feets_map.data;

	map_file("food.dat", &food_map, SIZE_T_MAX);
	food=food_map.data;

	map_file("hands.dat", &hands_map, SIZE_T_MAX);
	hands=hands_map.data;

	map_file("head.dat", &head_map, SIZE_T_MAX);
	head=head_map.data;

	map_file("king.dat", &king_map, SIZE_T_MAX);
	king=king_map.data;

	map_file("legs.dat", &legs_map, SIZE_T_MAX);
	legs=legs_map.data;

	map_file("monsters.dat", &monster_map, SIZE_T_MAX);
	monster=monster_map.data;

	map_file("neck.dat", &neck_map, SIZE_T_MAX);
	neck=neck_map.data;

	map_file("npcs.dat", &npc_map, SIZE_T_MAX);
	npcs=npc_map.data;

	map_file("onliner.dat", &onliner_map, SIZE_T_MAX);
	onliners=onliner_map.data;

	map_file("players.dat", &player_map, SIZE_T_MAX);
	players=player_map.data;

	map_file("poison.dat", &poison_map, SIZE_T_MAX);
	poison=poison_map.data;

	map_file("rings.dat", &rings_map, SIZE_T_MAX);
	rings=rings_map.data;

	map_file("shields.dat", &shields_map, SIZE_T_MAX);
	shields=shields_map.data;

	map_file("waist.dat", &waist_map, SIZE_T_MAX);
	waist=waist_map.data;

	map_file("weapon.dat", &weapon_map, SIZE_T_MAX);
	weapon=weapon_map.data;

	map_file("weapons.dat", &weapons_map, SIZE_T_MAX);
	weapons=weapons_map.data;

	map_file("level.dat", &level_map, SIZE_T_MAX);
	levels=level_map.data;
}
