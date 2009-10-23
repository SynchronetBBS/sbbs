#include <sys/mman.h>
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

struct map_file object_map;
struct object	*objects;

struct map_file king_map;
struct king		*king;

static size_t align(size_t val)
{
	int		pagesize=getpagesize();

	if(val % pagesize)
		val=val-(val%pagesize)+pagesize;
	return(val);
}

void map_file(char *name, struct map_file *map, size_t len)
{
	map->fd=open(name, O_RDWR);
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
	map_file("poison.dat", &poison_map, MAX_POISON * sizeof(struct poison));
	poison=poison_map.data;
	map_file("players.dat", &player_map, MAX_PLAYERS * sizeof(struct player));
	players=player_map.data;
	map_file("npcs.dat", &npc_map, MAX_NPCS * sizeof(struct player));
	npcs=npc_map.data;
	map_file("objects.dat", &object_map, MAX_OBJECTS * sizeof(struct object));
	objects=object_map.data;
	map_file("onliner.dat", &onliner_map, MAX_OBJECTS * sizeof(struct onliner));
	onliners=onliner_map.data;
	map_file("king.dat", &king_map, MAX_OBJECTS * sizeof(struct king));
	king=king_map.data;
}
