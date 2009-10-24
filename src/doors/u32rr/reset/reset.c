#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "files.h"

#include "todo.h"

int	fd;

static int mcounter=0;

int rand_num(int max)
{
	return(random()%max);
}

void add_monster(
	bool		auser,
	bool		wuser,
	long		weapnr,
	long		armnr,
	int			strength,
	const char	*name,
	bool		grabweap,
	bool		grabarm,
	const char	*phrase)
{
	int		i;
	struct monster	mon;

	memset(&mon, 0, sizeof(mon));
	strcpy(mon.name, name);
	mon.weapnr=weapnr;
	mon.armnr=armnr;
	mon.grabweap=grabweap;
	mon.grabarm=grabarm;
	strcpy(mon.phrase, phrase);
	mon.wuser=wuser;
	mon.auser=auser;
	mon.magicres=rand_num(50);
	mon.strength=strength+rand_num(10);
	mon.defence=mon.strength/2;
	if(!wuser)
		mon.strength += rand_num(20);
	if(!auser)
		mon.defence += rand_num(20);
	mon.iq=rand_num(100);
	mon.evil=rand_num(11);
	if(mcounter >= 10)
		mon.magiclevel=mcounter/10;
	else
		mon.magiclevel=1;
	if(mon.magiclevel > 100)
		mon.magiclevel=100;
	for(i=0; i<MAXMSPELLS; i++)
		if(rand_num(10)==0)
			mon.spell[i]=true;
	write(fd, &mon, sizeof(mon));
	mcounter++;
}

void add(const char *name, long val, long pow)
{
	struct armor arm;

	memset(&arm, 0, sizeof(arm));
	strcpy(arm.name, name);
	arm.val=val;
	arm.pow=pow;

	write(fd, &arm, sizeof(arm));
}

void Add_Object(
	const char		*name,
	enum objtype	ttype,
	long			value,
	int				hps,
	int				stamina,
	int				agility,
	int				charisma,
	int				dex,
	int				wisdom,
	int				mana,
	int				armor,
	int				attack,
	const char		*owned,
	bool			onlyone,
	enum cures		cure,
	bool			shop,
	bool			dng,					// can you find the item in the dungeons
	bool			cursed,
	int				minlev,
	int				maxlev,
	const char		*desc1_0,
	const char		*desc1_1,
	const char		*desc1_2,
	const char		*desc1_3,
	const char		*desc1_4,
	const char		*desc2_0,
	const char		*desc2_1,
	const char		*desc2_2,
	const char		*desc2_3,
	const char		*desc2_4,
	int				strength,
	int				defence,
	int				str_need)
{
	struct object obj;

	memset(&obj, 0, sizeof(obj));
	strcpy(obj.name, name);
	obj.ttype=ttype;
	obj.value=value;
	obj.hps=hps;
	obj.stamina=stamina;
	obj.agility=agility;
	obj.charisma=charisma;
	obj.dex=dex;
	obj.wisdom=wisdom;
	obj.mana=mana;
	obj.armor=armor;
	obj.attack=attack;
	strcpy(obj.owned, owned);
	obj.onlyone=onlyone;
	obj.cure=cure;
	obj.shop=shop;
	obj.dng=dng;
	obj.cursed=cursed;
	obj.minlev=minlev;
	obj.maxlev=maxlev;
	strcpy(obj.desc1[0], desc1_0);
	strcpy(obj.desc1[1], desc1_1);
	strcpy(obj.desc1[2], desc1_2);
	strcpy(obj.desc1[3], desc1_3);
	strcpy(obj.desc1[4], desc1_4);
	strcpy(obj.desc2[0], desc1_0);
	strcpy(obj.desc2[1], desc1_1);
	strcpy(obj.desc2[2], desc1_2);
	strcpy(obj.desc2[3], desc1_3);
	strcpy(obj.desc2[4], desc1_4);
	obj.strength=strength;
	obj.defence=defence;
	obj.str_need=str_need;

	write(fd, &obj, sizeof(obj));
}

#define add_level(val) \
	lvl.xpneed=(val); write(fd, &lvl, sizeof(lvl));
void create_level_file(void)
{
	struct level	lvl;
	int				fd;

	fd=open("level.dat", O_WRONLY|O_TRUNC|O_CREAT, 0666);
	memset(&lvl, 0, sizeof(lvl));

	add_level( 0 );
	add_level( 900 );
	add_level( 5000 );
	add_level( 10000 );
	add_level( 15000 );
	add_level( 20000 );
	add_level( 30000 );
	add_level( 40000 );
	add_level( 60000 );
	add_level( 85000 );
	add_level( 120000 );
	add_level( 140000 );
	add_level( 180000 );
	add_level( 240000 );
	add_level( 290000 );
	add_level( 340000 );
	add_level( 400000 );
	add_level( 500000 );
	add_level( 600000 );
	add_level( 850000 );
	add_level( 1000000 );
	add_level( 1200000 );
	add_level( 1400000 );
	add_level( 1600000 );
	add_level( 1800000 );
	add_level( 2200000 );
	add_level( 2500000 );
	add_level( 2700000 );
	add_level( 2900000 );
	add_level( 3200000 );
	add_level( 3400000 );
	add_level( 3600000 );
	add_level( 3800000 );
	add_level( 4200000 );
	add_level( 4400000 );
	add_level( 4600000 );
	add_level( 4800000 );
	add_level( 5000000 );
	add_level( 5200000 );
	add_level( 5400000 );
	add_level( 5600000 );
	add_level( 5800000 );
	add_level( 6000000 );
	add_level( 6200000 );
	add_level( 6400000 );
	add_level( 6600000 );
	add_level( 6800000 );
	add_level( 7000000 );
	add_level( 7200000 );
	add_level( 7400000 );
	add_level( 7600000 );
	add_level( 7800000 );
	add_level( 8000000 );
	add_level( 8200000 );
	add_level( 8400000 );
	add_level( 8600000 );
	add_level( 8800000 );
	add_level( 9000000 );
	add_level( 9200000 );
	add_level( 9400000 );
	add_level( 9600000 );
	add_level( 9800000 );
	add_level( 10000000 );
	add_level( 10200000 );
	add_level( 10400000 );
	add_level( 10600000 );
	add_level( 10800000 );
	add_level( 11000000 );
	add_level( 12000000 );
	add_level( 13000000 );
	add_level( 14000000 );
	add_level( 15000000 );
	add_level( 16000000 );
	add_level( 17000000 );
	add_level( 18000000 );
	add_level( 19000000 );
	add_level( 20000000 );
	add_level( 21000000 );
	add_level( 22000000 );
	add_level( 23000000 );
	add_level( 24000000 );
	add_level( 25000000 );
	add_level( 26000000 );
	add_level( 27000000 );
	add_level( 28000000 );
	add_level( 29000000 );
	add_level( 30000000 );
	add_level( 31000000 );
	add_level( 32000000 );
	add_level( 33000000 );
	add_level( 35000000 );
	add_level( 36000000 );
	add_level( 37000000 );
	add_level( 39000000 );
	add_level( 41000000 );
	add_level( 42000000 );
	add_level( 43000000 );
	add_level( 45000000 );
	add_level( 47000000 );
	add_level( 48000000 );
	add_level( 49000000 );
	add_level( 50000000 );
	close(fd);
}

void create_king_file(void)
{
	int	fd;
	struct	king king;

	fd=open("king.dat", O_WRONLY|O_TRUNC|O_CREAT, 0666);
	memset(&king, 0, sizeof(king));
	strcpy(king.name, "Nobody");
	king.ai=AI_Computer;
	king.sexy=Male;
	king.daysinpower=0;
	king.tax=3;
	king.taxalignment=AlignAll;
	strcpy(king.moatid, "crocodile");
	king.shop_weapon=true;
	king.shop_armor=true;
	king.shop_magic=true;
	king.shop_alabat=true;
	king.shop_plmarket=true;
	king.shop_healing=true;
	king.shop_drugs=true;
	king.shop_steroids=true;
	king.shop_orbs=true;
	king.shop_evilmagic=true;
	king.shop_bobs=true;
	king.shop_gigolos=true;
	write(fd, &king, sizeof(king));
	close(fd);
}

void create_onliner_file(void)
{
	int	fd;
	struct	onliner ol;
	int	i;

	fd=open("onliner.dat", O_WRONLY|O_TRUNC|O_CREAT, 0666);
	memset(&ol, 0, sizeof(ol));
	for(i=0; i<MAX_ONLINERS; i++)
		write(fd, &ol, sizeof(ol));
	close(fd);
}

void create_npc_file(void)
{
	int	fd;
	struct	player p;
	int	i;

	fd=open("npcs.dat", O_WRONLY|O_TRUNC|O_CREAT, 0666);
	memset(&p, 0, sizeof(p));
	p.deleted=true;
	for(i=0; i<MAX_PLAYERS; i++)
		write(fd, &p, sizeof(p));
	close(fd);
}

void create_player_file(void)
{
	int	fd;
	struct	player p;
	int	i;

	fd=open("players.dat", O_WRONLY|O_TRUNC|O_CREAT, 0666);
	memset(&p, 0, sizeof(p));
	p.deleted=true;
	for(i=0; i<MAX_PLAYERS; i++)
		write(fd, &p, sizeof(p));
	close(fd);
}

void create_poison_file(void)
{
	int	fd;
	struct	poison p;
	int	i;

	fd=open("poison.dat", O_WRONLY|O_TRUNC|O_CREAT, 0666);
	memset(&p, 0, sizeof(p));
	for(i=0; i<21; i++)
		write(fd, &p, sizeof(p));
	close(fd);

	map_file("poison.dat", &poison_map, MAX_POISON * sizeof(struct poison));
	poison=poison_map.data;

	strcpy(poison[0].name, "Snake Bite");
	poison[0].strength=4;
	poison[0].cost=1500;

	strcpy(poison[1].name, "Xaminah Stir");
	poison[1].strength=10;
	poison[1].cost=11000;

	strcpy(poison[2].name, "Zargothicia");
	poison[2].strength=14;
	poison[2].cost=25000;

	strcpy(poison[3].name, "Diamond Sting");
	poison[3].strength=17;
	poison[3].cost=100000;

	strcpy(poison[4].name, "Mynthia");
	poison[4].strength=21;
	poison[4].cost=300000;

	strcpy(poison[5].name, "Exxodus");
	poison[5].strength=41;
	poison[5].cost=550000;

	strcpy(poison[6].name, "Wolf Spit");
	poison[6].strength=51;
	poison[6].cost=850000;

	strcpy(poison[7].name, "Joy of Death");
	poison[7].strength=71;
	poison[7].cost=1250000;

	strcpy(poison[8].name, "Eusebius Cure");
	poison[8].strength=81;
	poison[8].cost=1500000;

	strcpy(poison[9].name, "Yxaxxiantha");
	poison[9].strength=85;
	poison[9].cost=1900000;

	strcpy(poison[10].name, "Polluted Lung");
	poison[10].strength=90;
	poison[10].cost=3000000;

	strcpy(poison[11].name, "Postheria");
	poison[11].strength=96;
	poison[11].cost=6000000;

	strcpy(poison[12].name, "Red Sledge");
	poison[12].strength=100;
	poison[12].cost=9000000;

	strcpy(poison[13].name, "Mullamia");
	poison[13].strength=110;
	poison[13].cost=9100000;

	strcpy(poison[14].name, "Cobra High");
	poison[14].strength=115;
	poison[14].cost=9200000;

	strcpy(poison[15].name, "Stomach Claw");
	poison[15].strength=120;
	poison[15].cost=9300000;

	strcpy(poison[16].name, "Fasanathievh");
	poison[16].strength=125;
	poison[16].cost=9400000;

	strcpy(poison[17].name, "Urpathxiaveth");
	poison[17].strength=130;
	poison[17].cost=9500000;

	strcpy(poison[18].name, "Dragon Flame");
	poison[18].strength=135;
	poison[18].cost=9600000;

	strcpy(poison[19].name, "Usilamahs Bite");
	poison[19].strength=140;
	poison[19].cost=9700000;

	strcpy(poison[20].name, "Devils Cure");
	poison[20].strength=145;
	poison[20].cost=9900000;

	unmap_file(&poison_map);
}

int main(int argc, char **argv)
{
	srandomdev();

	fd=open("abody.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_ABody();
	close(fd);

	fd=open("armor.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Armor();
	close(fd);

	fd=open("arms.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Arms();
	close(fd);

	fd=open("body.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Body();
	close(fd);

	fd=open("drink.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Drink();
	close(fd);

	fd=open("face.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Face();
	close(fd);

	fd=open("feets.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Feets();
	close(fd);

	fd=open("food.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Food();
	close(fd);

	fd=open("hands.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Hands();
	close(fd);

	fd=open("head.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Head();
	close(fd);

	fd=open("legs.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Legs();
	close(fd);

	fd=open("neck.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Neck();
	close(fd);

	fd=open("rings.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Rings();
	close(fd);

	fd=open("shields.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Shields();
	close(fd);

	fd=open("waist.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Waist();
	close(fd);

	fd=open("monsters.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Monsters();
	close(fd);

	fd=open("weapons.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Weapons();
	close(fd);

	fd=open("weapon.dat", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	Reset_Weapon();
	close(fd);

	create_poison_file();
	create_player_file();
	create_npc_file();
	create_king_file();
	create_onliner_file();
	create_level_file();
}
