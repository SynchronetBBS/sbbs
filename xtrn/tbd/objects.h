typedef struct {
    char type,color,symbol,name[50];
    int value,misc;
} object_t;
object_t object[62]={
    0,0,' '," ",0,0,
    1,(BLUE),'',"Dagger",0,4,
    1,(LIGHTBLUE),'',"Long Dagger",1250,5,                /* is the damage    */
    1,(LIGHTGREEN),'',"Short Sword",2500,6,               /* that they do     */
    1,(LIGHTCYAN),'',"Long Sword",3740,7,
    1,(LIGHTRED),'',"Broad Sword",4360,8,
    1,(LIGHTMAGENTA),'',"Bastard Sword",4680,9,
    1,(YELLOW),'',"2-Handed Sword",4840,10,
    2,(LIGHTBLUE),'È',"Leather Armor",1250,1,             /* Misc for Armor   */
    2,(LIGHTGREEN),'È',"Studded Armor",2500,2,            /* is their A.C.    */
    2,(LIGHTCYAN),'È',"Scale Armor",3740,3,
    2,(LIGHTRED),'È',"Chain Armor",4360,4,
    2,(LIGHTMAGENTA),'È',"Banded Armor",4680,5,
    2,(YELLOW),'È',"Plate Armor",4840,6,
    3,(YELLOW),'$',"",0,0,
    4,(LIGHTBLUE),'Ë',"Warp 2 Other Scroll",100,1,
    4,(LIGHTBLUE),'Î',"Invisibilty Potion",2500,2,          /* 30 seconds */
    4,(LIGHTGREEN),'Î',"Healing Potion",500,3,              /* 1-5 points */
    4,(LIGHTGREEN),'Ë',"Scroll of Xtra Heal",1000,4,        /* 2-10 points */
    4,(LIGHTMAGENTA),'Ë',"Mystery Scroll",200,7,
    4,(LIGHTMAGENTA),'Î',"Mystery Potion",200,8,
    4,(LIGHTRED),'¯', "Ring of Resurrect",2500,0,
    4,(LIGHTCYAN),'Î',"Potion of Full Heal",2500,5,
    4,(YELLOW),'Î',"Potion of Strength",1500,6,
    4,(BLINK|YELLOW),'',"Magical Key",0,0,
    4,(BLINK|LIGHTRED),'Ù',"Staff of Power",0,0,
    4,(WHITE),'@',"Magical Compass",0,0,
    4,(BROWN),'F',"Food",0,0,
    5,(YELLOW),'',"Stairs Going Up (+ to climb up)",0,0,
    5,(LIGHTGREEN),'',"Stairs Going Down (+ to climb down)",0,1,
    6,(LIGHTCYAN),'„',"Weapons Shop Shopkeeper",0,1,
    6,(LIGHTGREEN),'„',"Armor Shop Shopkeeper",0,2,
    6,(LIGHTRED),'„',"Specialty Store Shopkeeper",0,3,
    7,(LIGHTBLUE),'ù',"a Giant Caterpillar",1,1,            /* Gold first - */
    7,(LIGHTGREEN),'Í',"a Frag",1,1,                        /* then level   */
    7,(BROWN),'ë',"a Giant Snail",1,2,
    7,(LIGHTRED),'Ä',"a Deadly Crab",2,2,
    7,(LIGHTCYAN),'ü',"a Rattlesnake",2,3,
    7,(LIGHTBLUE),'û',"a Giant",3,3,
    7,(YELLOW),'‚',"a Brainsucker",3,4,
    7,(LIGHTMAGENTA),'‰',"a Bonecrusher",4,4,
    7,(BLUE),'ê',"a Thraxion Stabber",4,5,
    7,(GREEN),'å',"a Conehead",5,5,
    7,(RED),'ì',"a Snubber",5,6,
    7,(MAGENTA),'î',"a Little Bojay",6,6,
    7,(LIGHTMAGENTA),'ô',"a Big Bojay",6,7,
    7,(BLUE),'Å',"a Little Flerp",7,7,
    7,(LIGHTBLUE),'ö',"a Big Flerp",7,8,
    7,(LIGHTRED),'®',"a Neck Puller",8,8,
    7,(LIGHTCYAN),'ã',"an Inhaler",8,9,
    7,(YELLOW),'í',"a Hunchback",9,9,
    7,(GREEN),'∞',"a Creeping Moss",9,10,
    7,(YELLOW),'Ì',"a Killer Hornet",10,10,
    7,(WHITE),'ò',"a Zombie",10,10,
    7,(LIGHTBLUE),'Ó',"a Killer Mantis",0,15,
    7,(LIGHTRED),'·',"The Beast",0,20,
    8,(GREEN),'',"Tree",0,0,
    8,(YELLOW),'∞',"Quicksand",0,0,
    8,(MAGENTA),'',"Pillar",0,0,
    8,(WHITE),'˛',"Pillar",0,0,
    8,(BLUE),'˜',"Water",0,0,
    8,(WHITE),'Ï',"Rock",0,0
};
