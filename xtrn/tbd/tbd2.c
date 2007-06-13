/******************************************************************************
 Functions for The Beast's Domain
******************************************************************************/

#include "tbd_pack.h"

int in_shop=0;
long cost_per_min=0L,total_cost=0L,times_per_day=0L;
user_t   user;
rmobj_t  rmobj[55];
rmobj_t  rmcmp[55];
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
    4,(YELLOW),'',"Magical Key",0,0,
    4,(LIGHTRED),'Ù',"Staff of Power",0,0,
    4,(WHITE),'@',"Magical Compass",0,0,
    4,(BROWN),'F',"Food",0,9,
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
/******************************************************************************
 This function draws the room you are currently in (it also automatically
 calls the 'draw_others' routine to draw any other people in the room.
******************************************************************************/
void drawpos(int x,int y,int z)
{
    char s,s2,color[10],color2[10],str[256],str1[256],str2[256],str3[256],
         str4[256];

    if(z==6 || z==7 || z==0) { s=''; strcpy(color,"\1n\1g"); }
    if(z==1 || z==5) { s='±'; strcpy(color,"\1n\1y"); }
    if(z==2 || z==3 || z==4) { s='∞'; strcpy(color,"\1h\1y"); }
    if(z==8) { s=''; strcpy(color,"\1n\1m"); }
    s2=s; strcpy(color2,color);

    if(z==7 && y==0) { s=''; strcpy(color,"\1n\1m"); }
    sprintf(str1,"%s%c%c%c%c%c%c%c%c%c%c%c%c%c",color,
            (map[z][y][x]&NW_DOOR) ? ' ':s,
            s,s,s,s,s,(map[z][y][x]&N_DOOR) ? ' ':s,s,s,s,s,s,
            (map[z][y][x]&NE_DOOR) ? ' ':s);
    if(z==7 && y==0 && x==(SQUARE/2)) {
        if(!user.key)
            sprintf(str1,"\1n\1m\1yÕÕÕÕÕ\1m");
        else
            sprintf(str1,"\1n\1m     "); }
    s=s2; strcpy(color,color2);
    if(z==1 && x==0) { s=''; strcpy(color,"\1n\1m"); }
    else if(z==5 && x==SQUARE-1) { s2=''; strcpy(color2,"\1n\1m"); }
    sprintf(str2,"%s%c           %s%c",color,s,color2,s2);
    sprintf(str3,"%s%c           %s%c",color,
            (map[z][y][x]&W_DOOR) ? ' ':s,color2,
            (map[z][y][x]&E_DOOR) ? ' ':s2);
    if(z==1 && x==0) { s=s2; strcpy(color,color2); }
    else if(z==5 && x==SQUARE-1) { s2=s; strcpy(color2,color); }
    if(z==3 && y==SQUARE-1) { s=''; strcpy(color,"\1n\1m"); }
    sprintf(str4,"%s%c%c%c%c%c%c%c%c%c%c%c%c%c",color,
            (map[z][y][x]&SW_DOOR) ? ' ':s,
            s,s,s,s,s,(map[z][y][x]&S_DOOR) ? ' ':s,s,s,s,s,s,
            (map[z][y][x]&SE_DOOR) ? ' ':s);
    if(z==8 && y==SQUARE-1 && x==(SQUARE/2)) {
        sprintf(str4,"\1n\1m     "); }

    if(z<9) {
        bprintf("\x1b[6;34H%s",str1);
        bprintf("\x1b[7;34H%s",str2);
        bprintf("\x1b[8;34H%s",str2);
        bprintf("\x1b[9;34H%s",str3);
        bprintf("\x1b[10;34H%s",str2);
        bprintf("\x1b[11;34H%s",str2);
        bprintf("\x1b[12;34H%s",str4);
    } else {
    bprintf("\1n\1h");
    if(map[z][y][x]&NW_DOOR)
        bprintf("\x1b[6;34H⁄");
    else
        bprintf("\x1b[6;34H…");

    if(map[z][y][x]&N_DOOR)
        bprintf("ÕÕÕÕÕƒÕÕÕÕÕ");
    else
        bprintf("ÕÕÕÕÕÕÕÕÕÕÕ");

    if(map[z][y][x]&NE_DOOR)
        bprintf("ø");
    else
        bprintf("ª");

    bprintf("\x1b[7;34H∫           ∫");
    bprintf("\x1b[8;34H∫           ∫");
    if(map[z][y][x]&W_DOOR)
        bprintf("\x1b[9;34H≥");
    else
        bprintf("\x1b[9;34H∫");

    if(map[z][y][x]&E_DOOR)
        bprintf("           ≥");
    else
        bprintf("           ∫");

    bprintf("\x1b[10;34H∫           ∫");
    bprintf("\x1b[11;34H∫           ∫");

    if(map[z][y][x]&SW_DOOR)
        bprintf("\x1b[12;34H¿");
    else
        bprintf("\x1b[12;34H»");

    if(map[z][y][x]&S_DOOR)
        bprintf("ÕÕÕÕÕƒÕÕÕÕÕ");
    else
        bprintf("ÕÕÕÕÕÕÕÕÕÕÕ");

    if(map[z][y][x]&SE_DOOR)
        bprintf("Ÿ");
    else
        bprintf("º");
    }

    switch(z) {
        case 0:
            sprintf(str,"\1h\1m[\1h\1y Forest of Evil SE \1h\1m]");
            break;
        case 1:
            sprintf(str,"\1h\1m[\1h\1yPlains of Peril E  \1h\1m]");
            break;
        case 2:
            sprintf(str,"\1h\1m[\1h\1yDesert of Death NE \1h\1m]");
            break;
        case 3:
            sprintf(str,"\1h\1m[\1h\1yDesert of Death N  \1h\1m]");
            break;
        case 4:
            sprintf(str,"\1h\1m[\1h\1yDesert of Death NW \1h\1m]");
            break;
        case 5:
            sprintf(str,"\1h\1m[\1h\1yPlains of Peril W  \1h\1m]");
            break;
        case 6:
            sprintf(str,"\1h\1m[\1h\1y Forest of Evil SW \1h\1m]");
            break;
        case 7:
            sprintf(str,"\1h\1m[\1h\1y Forest of Evil S  \1h\1m]");
            break;
        case 8:
            sprintf(str,"\1h\1m[\1h\1y Fortress of Doom  \1h\1m]");
            break;
        case 9:
            sprintf(str,"\1h\1m[\1h\1y  Dungeon of Doom  \1h\1m]");
            break;
    }
/*    status_message(0,"\1m\1hROOM: %3d  LEVEL: %3d",y*SQUARE+(x+1),z+1); */
    bprintf("\x1b[13;30H%s",str);
    if(user.compass) bprintf("\x1b[5;36H\1h\1m[\1h\1y%3d;%3d\1h\1m]",x,y);
    get_room_objects(z,(y*SQUARE)+x,1); draw_others(x,y,z);
}
/******************************************************************************
 This function is used for printing status messages under the map window
******************************************************************************/
void status_message(int line,char *str, ...)
{
    char sbuf[1024];
    int len,x;
	va_list	vp;

	va_start(vp, str);
    vsprintf(sbuf,str,vp);
	va_end(vp);
    len=67-bstrlen(sbuf);
    for(x=0;x<len;x++) strcat(sbuf," ");
    bprintf("\x1b[%d;7H%-67s",line+14,sbuf);
}
/******************************************************************************
 This function writes your movements to disk, locking your region of the file
 in the process
******************************************************************************/
void write_movement(int pic,int x,int y,int z,int gx,int gy)
{
    char chbuf[8],count=0;

    lseek(chfile,(long)(node_num-1)*8L,SEEK_SET);
    while(lock(chfile,(long)(node_num-1)*8L,8) && count++<100);
    read(chfile,chbuf,8);
    if(!(chbuf[0]==pic && chbuf[1]==x && chbuf[2]==y && chbuf[3]==z &&
       chbuf[4]==gx && chbuf[5]==gy)) {
        chbuf[0]=pic; chbuf[1]=x; chbuf[2]=y; chbuf[3]=z; chbuf[4]=gx;
        chbuf[5]=gy; chbuf[6]=user.level; chbuf[7]=object[user.armor].misc;
        lseek(chfile,-8L,SEEK_CUR);
        write(chfile,chbuf,8); }
    unlock(chfile,(long)(node_num-1)*8L,8);
}
/******************************************************************************
 This function checks to see if someone is in your way.  If so it returns a 1
 otherwise it returns 0
******************************************************************************/
int initial_inway(int x,int y,int z,int gx,int gy)
{
    char chbuf[8],count;
    int n;

    lseek(chfile,0L,SEEK_SET);
    for(n=0;n<sys_nodes;n++) {
        count=0;
        if(n==node_num-1) lseek(chfile,8L,SEEK_CUR);
        if(n!=node_num-1) {
            while(read(chfile,chbuf,8)==-1 && count++<100);
            if(count>=100) lseek(chfile,8L,SEEK_CUR);
            if(chbuf[0]) {
                if(chbuf[1]==x && chbuf[2]==y && chbuf[3]==z &&
                    chbuf[4]==gx && chbuf[5]==gy) return 1;
            }
        }
    }
    if(user.mapz==z && user.mapy==y && user.mapx==x && gx>=0 && gx<=10 &&
        gy>=0 && gy<=4)
        if(rmobj[(gy*11)+gx].item) {
            if(object[rmobj[(gy*11)+gx].item].type==BLOCKAGE ||
                object[rmobj[(gy*11)+gx].item].type==MONSTER) return 1;
        }
    return 0;
}
/******************************************************************************
 This function checks to see if someone is in your way.  If so it returns a 1
 otherwise it returns 0
******************************************************************************/
int inway(int x,int y,int z,int gx,int gy)
{
    char chbuf[8],count;
    node_t node;
    int n,damage,mlevel;

    lseek(chfile,0L,SEEK_SET);
    for(n=0;n<sys_nodes;n++) {
        count=0;
        if(n==node_num-1) lseek(chfile,8L,SEEK_CUR);
        if(n!=node_num-1) {
/*            lseek(chfile,(long)(n*8),SEEK_SET); */
            while(read(chfile,chbuf,8)==-1 && count++<100);
            if(count>=100) lseek(chfile,8L,SEEK_CUR);
            if(chbuf[0]) {
                if(chbuf[1]==x && chbuf[2]==y && chbuf[3]==z &&
                    chbuf[4]==gx && chbuf[5]==gy) {
                    getnodedat(n+1,&node,0);
                    if(weapon_ready) {
                        if((damage=player_attack(n+1,chbuf[6],chbuf[7],
                            chbuf[0]))>0)
                            status_message(1,"\1g\1hYou hit %s for %d damage!",
                                (chbuf[0]==32) ? "something" :
                                username(node.useron),damage);
                        else
                            status_message(1,"\1b\1hYou swing and miss %s!",
                                (chbuf[0]==32) ? "something" :
                                username(node.useron));
                        lasthit=-1;
                        return 1;
                    }
                    if(chbuf[0]==32) {
                        lasthit=-1;
                        status_message(2,"\1r\1hSomething is in the way!");
                        return 1;
                    }
                    if(lasthit!=n) {
                        status_message(2,"\1g\1hNode %d: %s",n+1,
                            username(node.useron));
                        lasthit=n; return 1;
                    } else return 1;
                }
            }
        }
    }
    if(user.mapz==z && user.mapy==y && user.mapx==x && gx>=0 && gx<=10 &&
        gy>=0 && gy<=4) {
        if(rmobj[(gy*11)+gx].item) {
            if(object[rmobj[(gy*11)+gx].item].type==BLOCKAGE) {
                if(rmobj[(gy*11)+gx].item==NUM_BLOCKAGE+4) {
                    if(time(NULL)-user.left_game>=120+rand()%30) {
                        status_message(1,"\1b\1hYou've found a magical "
                            "fountain!  You feel much better!");
                        user.health=user.max_health;
                        bprintf("\x1b[4;41H\1h\1y%6d",user.health);
                        user.left_game=time(NULL);
                    } else {
                        status_message(1,"\1r\1hYou were just here!  Don't "
                            "abuse the fountain, come back later!");
                    }
                }
                status_message(2,"\1g\1h%s",
                    object[rmobj[(gy*11)+gx].item].name);
                return 1;
            } else if(object[rmobj[(gy*11)+gx].item].type==MONSTER) {
                status_message(2,"\1g\1h%s",
                    object[rmobj[(gy*11)+gx].item].name);
                if(weapon_ready) {
                    if((damage=attack_monster(object[rmobj[(gy*11)+gx].item]
                        .misc))>0) {  /* Player attacks monster */
                        status_message(1,"\1g\1hYou hit %s for %d damage!",
                            object[rmobj[(gy*11)+gx].item].name,damage);
                        if(rmobj[(gy*11)+gx].val<damage)
                            rmobj[(gy*11)+gx].val=0;
                        else rmobj[(gy*11)+gx].val-=damage;
                        if(!rmobj[(gy*11)+gx].val) {
                            status_message(1,"\1y\1hYou killed %s!",
                                object[rmobj[(gy*11)+gx].item].name);
                            mlevel=object[rmobj[(gy*11)+gx].item].misc;
                            if(mlevel>(int)user.level)
                                user.exp+=(mlevel-(int)user.level)*10;
                            else if((user.level-5)<=mlevel) user.exp+=
                                (long)mlevel;
                            if(rmobj[(gy*11)+gx].item==NUM_MONSTER+21 &&
                               !user.key) {
                                rmobj[(gy*11)+gx].val=0;        /* Key      */
                                rmobj[(gy*11)+gx].item=NUM_SPECIAL+9;
                            } else if(rmobj[(gy*11)+gx].item==NUM_MONSTER+22) {
                                rmobj[(gy*11)+gx].val=0;        /* Staff    */
                                rmobj[(gy*11)+gx].item=NUM_SPECIAL+10;
                            } else {
                                if((rand()%100)<=5 && !user.compass) {
                                    rmobj[(gy*11)+gx].val=0;    /* Compass  */
                                    rmobj[(gy*11)+gx].item=NUM_SPECIAL+11;
                                } else if(rand()%100<60 &&
                                    (user.level-5)<=mlevel) {   /* Gold     */
                                    rmobj[(gy*11)+gx].val=rand()%9+1;
                                    rmobj[(gy*11)+gx].item=NUM_GOLD;
                                } else {
                                    rmobj[(gy*11)+gx].val=0;    /* Food     */
                                    rmobj[(gy*11)+gx].item=NUM_SPECIAL+12; } }
                        } else {
                            status_message(2,"\1g\1h%s looks about %%%d "
                                "damaged.",object[rmobj[(gy*11)+gx].item].name,
                                100-((rmobj[(gy*11)+gx].val*100)/
                                ((object[rmobj[(gy*11)+gx].item].misc*10)+10)));
                            monster_attack(object[rmobj[(gy*11)+gx].item].misc);
                        }
                        put_single_object(z,(y*SQUARE)+x,(gy*11)+gx);
                        get_room_objects(z,(y*SQUARE)+x,1);
                    } else
                        status_message(1,"\1b\1hYou swing and miss %s!",
                            object[rmobj[(gy*11)+gx].item].name);
                    lasthit=-1;
                    return 1;
                } else {
                    monster_attack(object[rmobj[(gy*11)+gx].item].misc);
                    return 1;
                }
            }
        }
    }
    return 0;
}
/******************************************************************************
 This function draws any other players in the game in the current screen
******************************************************************************/
void draw_others(int x,int y,int z)
{
    char chbuf[8],count;
    int n;

    lseek(chfile,0L,SEEK_SET);
    for(n=0;n<sys_nodes;n++) {
        count=0;
        if(n==node_num-1) lseek(chfile,8L,SEEK_CUR);
        if(n!=node_num-1) {
            while(read(chfile,chbuf,8)==-1 && count++<100);
            if(count>=100) lseek(chfile,8L,SEEK_CUR);
            if(chbuf[0] && chbuf[1]==x && chbuf[2]==y && chbuf[3]==z)
                if(chbuf[4]>-1 && chbuf[5]>-1) {
                    attr((n+1)&0x1f);
                    bprintf("\x1b[%d;%dH%c",chbuf[5]+7,chbuf[4]+35,
                            chbuf[0]);
                    attr((n+1));
                }
        }
    }
}
/******************************************************************************
 This function is used for displaying the right hand window, depending upon
 what 'menu' is set to is what gets displayed:
    0 = Game Commands                   1 = Inventory List
******************************************************************************/
void game_commands(int menu,int line)
{
    int x;

    switch(menu) {
        case 0:
            bprintf("\x1b[2;7H\1n\1y\1h   Movement Commands    ");
            bprintf("\x1b[4;7H                        ");
            bprintf("\x1b[5;7H\1c\1h          8\1n\1c/\1hW           ");
            bprintf("\x1b[6;7H\1c\1h      7\1n\1c/\1hQ  \1n\1h≥  \1c9\1n\1c/"
                    "\1hE       ");
            bprintf("\x1b[7;7H\1n\1h         \\ \1gN \1n\1h/          ");
            bprintf("\x1b[8;7H\1c\1h     4\1n\1c/\1hA \1n\1hƒ\1gW E\1n\1hƒ "
                    "\1c6\1n\1c/\1hD      ");
            bprintf("\x1b[9;7H\1n\1h         / \1gS \1n\1h\\          ");
            bprintf("\x1b[10;7H\1c\1h      1\1n\1c/\1hZ  \1n\1h≥  \1c3\1n\1c/"
                    "\1hC       ");
            bprintf("\x1b[11;7H\1c\1h          2\1n\1c/\1hX           ");
            bprintf("\x1b[12;7H                        ");
            break;
        case 1:
            bprintf("\x1b[2;7H\1y\1h       Inventory       ");
            for(x=0;x<6;x++) {
                if(line==(x+1) || line==-1) {
                    bprintf("\x1b[%d;6H\1c\1h %d]\1n\1c %-19.19s",x+4,x+1,
                            (user.item[x]) ? object[user.item[x]].name :
                            "Empty");
                    attr(object[user.item[x]].color);
                    bprintf(" %c",(user.item[x]) ? object[user.item[x]].symbol
                        : 32);
                }
            }
            if(line==7 || line==-1 || line==-2) {
                bprintf("\x1b[10;6H\1c\1h Weapon: \1n\1c%-14.14s",(user.weapon)
                        ? object[user.weapon].name : "Hands");
                attr(object[user.weapon].color);
                bprintf(" %c",(user.weapon) ? object[user.weapon].symbol : 32);
            }
            if(line==8 || line==-1 || line==-3) {
                bprintf("\x1b[11;6H\1c\1h Armor : \1n\1c%-14.14s",(user.armor) ?
                        object[user.armor].name : "Cloth");
                attr(object[user.armor].color);
                bprintf(" %c",(user.armor) ? object[user.armor].symbol : 32);
            }
            if(line==9 || line<0) {
                bprintf("\x1b[12;6H\1c\1h Gold  : \1n\1c%-4d",user.gold);
                bprintf("\x1b[12;20H\1c\1h Ring : \1n\1c%s",(user.ring) ?
                    "Yes" : "No"); }
            break;
    }
}
/******************************************************************************
 This function reads in the user account, if no account exists a 0 is returned
 else a 1 is returned.  If GET_EMPTY is set on, this function will look for an
 empty user slot, and return the slot number.
******************************************************************************/
long read_user(int get_empty)
{
    int file,x,count;
	char	buf[USER_T_SIZE];

    record_number=0L;
    if(!fexist("tbd.usr")) return 0L;        /* User file doesn't exist */

    if((file=nopen("tbd.usr",O_RDWR))==-1) {
        printf("Error opening user data file\r\n");
        exit(1); }
	ateof=0;
    while(!ateof) {
        count=0;
        while(lock(file,record_number*(long)(USER_T_SIZE),
            (long)(USER_T_SIZE)) && count++<500);
		if(!read(file,buf,USER_T_SIZE))
			ateof=1;
		unpack_user_t_struct(&user, buf);
        truncsp(user.handle);
        if(!user.status) {
            if(get_empty) {
                lseek(file,-(long)(USER_T_SIZE),SEEK_CUR);
                user.status=1; write(file,&user.status,2);
                close(file); return 1;
            } else if(!stricmp(user.handle,user_name)) {
                close(file); return 0; }
        }
        if(!get_empty && !stricmp(user.handle,user_name)) {
            close(file); return 1; }
        unlock(file,record_number*(long)(USER_T_SIZE),
            (long)(USER_T_SIZE));
        ++record_number;
    }
    if(get_empty) { user.status=1; write(file,&user.status,2); }
    close(file);
    if(get_empty) return 1;
    return 0;
}
/******************************************************************************
 This function writes the user info to disk when they go to place an order
******************************************************************************/
void write_user()
{
    int file;
	char	buf[USER_T_SIZE];

    strcpy(user.handle,user_name);
    if((file=nopen("tbd.usr",O_WRONLY|O_CREAT))==-1) {
        printf("Error opening user data file\r\n"); }
    lseek(file,record_number*(long)USER_T_SIZE,SEEK_SET);
	pack_user_t_struct(buf, &user);
	write(file,buf,USER_T_SIZE);
    close(file);
}
/******************************************************************************
 This is for listing the users that have/are playing in the game.
******************************************************************************/
void list_users()
{
    char statname[11][26],str[256];
    uchar statlev[11];
    long statexp[11];
    int file,num,x;
	char buf[USER_T_SIZE];

    for(x=0;x<11;x++) {
        statlev[x]=0; statexp[x]=0L; strcpy(statname[x],"Nobody"); }

    if(!fexist("tbd.usr")) {
        bprintf("\r\n\1y\1hNo one has dared enter The Beast's Domain yet!");
        return; }

    if((file=nopen("tbd.usr",O_RDONLY))==-1) {
        printf("Error opening user data file\r\n");
        exit(1); }
	bprintf("\r\n\1n\1hReading user database...");
	ateof=0;
    while(!ateof) {
		if(!read(file,buf,USER_T_SIZE))
			ateof=1;
		unpack_user_t_struct(&user, buf);
        truncsp(user.handle);
        statlev[10]=user.level; strcpy(statname[10],user.handle);
        statexp[10]=user.exp;
        num=0;
        if(!user.status) statexp[10]=-1;
        while(statexp[10]<statexp[num] && num<9) num++;
        for(x=8;x>=num;x--) {
            statlev[x+1]=statlev[x]; strcpy(statname[x+1],statname[x]);
            statexp[x+1]=statexp[x]; }
        statlev[num]=statlev[10]; strcpy(statname[num],statname[10]);
        statexp[num]=statexp[10];
    }
    close(file);
	lncntr=0;	/* no pause prompt */
    cls();
    center_wargs("\1m\1h÷ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒ∑");
    center_wargs("\1m\1h∫ \1yTop Ten Warriors \1m∫");
    center_wargs("\1m\1h÷ƒƒƒƒƒƒƒƒƒ–ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒ–ƒƒƒƒƒƒƒƒ∑");
    center_wargs("\1m\1h∫ \1yPlayer Name                   Level \1m∫");
    center_wargs("\1m\1h”ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒΩ");
    for(x=0;x<10;x++) {
        sprintf(str,"\1c\1h%-25.25s     \1g%5d",statname[x],statlev[x]);
        center_wargs(str); }
    return;
}
/******************************************************************************
 This function reads in the objects that are in room number 'room'
******************************************************************************/
void get_room_objects(int level,int room,int display)
{
    char count=0;
    int x,y,z=0;

    lseek(rmfile,(long)(level*SQUARE*SQUARE*110L)+(long)(room*110L),SEEK_SET);
	/* TODO this reads an array of 55 { char, char } structs. */
    while(read(rmfile,rmobj,110)==-1 && count++<100);
    if(display) {
        for(x=0;x<55;x++) {
            if(rmobj[x].item) {
                y=x/11;
                if(object[rmobj[x].item].type==TRADINGPOST) {
                    in_shop=object[rmobj[x].item].misc; z++;
                    status_message(1,"\1y\1hYou've found a trading post!"); }
                attr(object[rmobj[x].item].color);
                bprintf("\x1b[%d;%dH%c",y+7,(x-(y*11))+35,
                        object[rmobj[x].item].symbol);
            }
        }
        if(!z) in_shop=0;
    }   
}
/******************************************************************************
 This function compares room objects from the last read
******************************************************************************/
void comp_room_objects(int level,int room)
{
    char count=0;
    int x,y;

    lseek(rmfile,(long)(level*SQUARE*SQUARE*110L)+(long)(room*110L),SEEK_SET);
	/* TODO this reads an array of 55 { char, char } structs. */
    while(read(rmfile,rmcmp,110)==-1 && count++<100);
    for(x=0;x<55;x++) {
        y=x/11;
        if(rmobj[x].item!=rmcmp[x].item) {
            if(rmcmp[x].item && !rmobj[x].item) {
                attr(object[rmcmp[x].item].color);
                bprintf("\x1b[%d;%dH%c",y+7,(x-(y*11))+35,
                        object[rmcmp[x].item].symbol);
            }
            if(rmobj[x].item && !rmcmp[x].item) {
                bprintf("\x1b[%d;%dH ",y+7,(x-(y*11))+35);
            }
            rmobj[x]=rmcmp[x];
        }
    }
}
/******************************************************************************
 This function is used for reading a single object from the room object file
******************************************************************************/
void get_single_object(int level,int room,int num)
{
    char count=0;

    lseek(rmfile,(long)(level*SQUARE*SQUARE*110L)+(long)(room*110L)+
        (num*2),SEEK_SET);
	/* TODO this reads a { char, char } struct. */
    while(read(rmfile,&rmobj[num],2)==-1 && count++<100);
}
/******************************************************************************
 This function writes the objects that are in room number 'room' to the room
 file, this occurs when a user picks up or drops an object.
******************************************************************************/
void put_single_object(int level,int room,int num)
{
    char count=0;

    lseek(rmfile,(long)(level*SQUARE*SQUARE*110L)+(long)(room*110L)+
        (num*2),SEEK_SET);
    while(lock(rmfile,(long)(level*SQUARE*SQUARE*110L)+(long)(room*110L)+
        (num*2),2) && count++<100);
	/* TODO this writes a { char, char } structs. */
    write(rmfile,&rmobj[num],2);
    unlock(rmfile,(long)(level*SQUARE*SQUARE*110L)+(long)(room*110L)+
        (num*2),2);
}
/******************************************************************************
 Call this function to pickup an object or use the stairs
******************************************************************************/
int pickup_object(int x,int y,int z,int room)
{
    char str[256];
    unsigned char location,amount;
    int w,found_empty=0,temp;

    location=rmobj[(y*11)+x].item;
    amount=rmobj[(y*11)+x].val;
    if(object[location].type==STAIRS) {
        if(!object[location].misc || object[location].misc==2) {
            if(!(inway((room/SQUARE),(room%SQUARE),user.mapz-1,x,y)))
                user.mapz--;
        } else {
            if(!(inway((room/SQUARE),(room%SQUARE),user.mapz+1,x,y)))
                user.mapz++;
        }
        return 0;
    }
    if(object[location].type==GOLD) {
        if(user.gold>=2500) {
            status_message(0,"\1r\1hYou can't carry anymore gold!");
            return 0;
        } else {
            user.gold+=(amount*10);
            if(user.gold>2500) {
                w=user.gold-2500; user.gold-=w;
                status_message(0,"\1g\1hPicked up %d gold pieces",
                               (amount*10)-w);
                amount=w/10; rmobj[(y*11)+x].val=amount;
                put_single_object(z,room,(y*11)+x); return 9;
            }
            rmobj[(y*11)+x].item=0; rmobj[(y*11)+x].val=0;
            put_single_object(z,room,(y*11)+x);
            status_message(0,"\1g\1hPicked up %d gold pieces",amount*10);
            return 9;
        }
    }
    if(object[location].type==WEAPON) {
        if(in_shop) {
            if(user.weapon) {
                if(user.gold<(object[location].value-
                   (object[user.weapon].value/2))) {
                    status_message(0,"\1r\1hYou don't have enough gold!");
                    return 0;
                }
                status_message(1,"\1c\1hI'll give you %d gold for your %s.",
                        object[user.weapon].value/2,object[user.weapon].name);
                sprintf(str,"Purchase a %s for %d gold PLUS your weapon",
                        object[location].name,object[location].value-
                        (object[user.weapon].value/2));
                status_message(0,""); bprintf("\x1b[14;7H");
                if(yesno(str)) {
                    rmobj[(y*11)+x].item=user.weapon;
                    user.gold-=(object[location].value-(object[user.weapon].
                                value/2));
                    user.weapon=location;
                    put_single_object(z,room,(y*11)+x);
                    return -2;
                } return 0;
            } else {
                if(user.gold<object[location].value) {
                    status_message(0,"\1r\1hYou don't have enough gold!");
                    return 0;
                }
                sprintf(str,"Purchase a %s for %d gold",object[location].name,
                        object[location].value);
                status_message(0,""); bprintf("\x1b[14;7H");
                if(yesno(str)) {
                    rmobj[(y*11)+x].item=0; rmobj[(y*11)+x].val=0;
                    user.weapon=location;
                    user.gold-=object[location].value;
                    put_single_object(z,room,(y*11)+x);
                    return -2;
                } return 0;
            }
        } else {
            if(user.weapon) {
                status_message(0,"\1y\1hSwapped your %s for a %s.",
                    object[user.weapon].name,object[location].name);
                rmobj[(y*11)+x].item=user.weapon;
                user.weapon=location;
                put_single_object(z,room,(y*11)+x);
                return 7;
            } else {
                rmobj[(y*11)+x].item=0; rmobj[(y*11)+x].val=0;
                user.weapon=location;
                status_message(0,"\1g\1hPicked up a %s",object[user.weapon].
                               name);
                put_single_object(z,room,(y*11)+x);
                return 7;
            }
        }
    }
    if(object[location].type==ARMOR) {
        if(in_shop) {
            if(user.armor) {
                if(user.gold<(object[location].value-
                   (object[user.armor].value/2))) {
                    status_message(0,"\1r\1hYou don't have enough gold!");
                    return 0;
                }
                status_message(1,"\1c\1hI'll give you %d gold for your %s.",
                        object[user.armor].value/2,object[user.armor].name);
                sprintf(str,"Purchase a %s for %d gold PLUS your armor",
                        object[location].name,object[location].value-
                        (object[user.armor].value/2));
                status_message(0,""); bprintf("\x1b[14;7H");
                if(yesno(str)) {
                    rmobj[(y*11)+x].item=user.armor;
                    user.gold-=(object[location].value-(object[user.armor].
                                value/2));
                    user.armor=location;
                    put_single_object(z,room,(y*11)+x);
                    return -3;
                } return 0;
            } else {
                if(user.gold<object[location].value) {
                    status_message(0,"\1r\1hYou don't have enough gold!");
                    return 0;
                }
                sprintf(str,"Purchase a %s for %d gold",object[location].name,
                        object[location].value);
                status_message(0,""); bprintf("\x1b[14;7H");
                if(yesno(str)) {
                    rmobj[(y*11)+x].item=0; rmobj[(y*11)+x].val=0;
                    user.armor=location;
                    user.gold-=object[location].value;
                    put_single_object(z,room,(y*11)+x);
                    return -3;
                } return 0;
            }
        } else {
            if(user.armor) {
                status_message(0,"\1y\1hSwapped your %s for some %s.",
                    object[user.armor].name,object[location].name);
                rmobj[(y*11)+x].item=user.armor;
                user.armor=location;
                put_single_object(z,room,(y*11)+x);
                return 8;
            } else {
                rmobj[(y*11)+x].item=0; rmobj[(y*11)+x].val=0;
                user.armor=location;
                status_message(0,"\1g\1hPicked up some %s",
                               object[user.armor].name);
                put_single_object(z,room,(y*11)+x);
                return 8;
            }
        }
    }
    if(in_shop && object[location].type!=TRADINGPOST) {
        if(user.gold<object[location].value) {
            status_message(0,"\1r\1hYou don't have enough gold!");
            return 0;
        }
        if(rmobj[(y*11)+x].item==NUM_SPECIAL+6) {   /* Ring of Resurrect */
            if(!user.ring) {
                sprintf(str,"Purchase a %s for %d gold",object[location].name,
                    object[location].value);
                status_message(0,""); bprintf("\x1b[14;7H");
                if(yesno(str)) {
                    user.ring=rmobj[(y*11)+x].item;
                    user.gold-=object[location].value;
                    rmobj[(y*11)+x].item=0; rmobj[(y*11)+x].val=0;
                    put_single_object(z,room,(y*11)+x);
                    return -1;
                } return 0;
            } else {
                status_message(0,"\1r\1hYou can only wear one ring at a time!");
                return 0; } }

        for(w=0;w<6;w++) {
            if(!user.item[w]) {
                sprintf(str,"Purchase a %s for %d gold",object[location].name,
                    object[location].value);
                status_message(0,""); bprintf("\x1b[14;7H");
                if(yesno(str)) {
                    rmobj[(y*11)+x].item=0; rmobj[(y*11)+x].val=0;
                    user.item[w]=location;
                    user.gold-=object[location].value;
                    put_single_object(z,room,(y*11)+x);
                    return -1;
                } return 0;
            }
        }
        status_message(0,"\1r\1hYou can't carry anymore!");
        return 0;
    }
    if(rmobj[(y*11)+x].item==NUM_SPECIAL+9) {   /* Key to get into castle */
        if(!user.key) {
            rmobj[(y*11)+x].item=0; rmobj[(y*11)+x].val=0;
            put_single_object(z,room,(y*11)+x);
            user.key=1; bprintf("\x1b[13;7H\1m\1h[\1rKEY\1m]");
            return 8;
        } else {
            status_message(0,"\1r\1hYou only need one key!");
            return 0; } }
    if(rmobj[(y*11)+x].item==NUM_SPECIAL+10) {
        rmobj[(y*11)+x].item=NUM_MONSTER+22;
        rmobj[(y*11)+x].val=(object[NUM_MONSTER+22].misc*10)+10;
        put_single_object(z,room,(y*11)+x);
        return 255; }
    if(rmobj[(y*11)+x].item==NUM_SPECIAL+11) {  /* Magical Compass */
        if(!user.compass) {
            rmobj[(y*11)+x].item=0; rmobj[(y*11)+x].val=0;
            put_single_object(z,room,(y*11)+x);
            bprintf("\x1b[5;36H\1h\1m[\1h\1y%3d;%3d\1h\1m]",x,y);
            user.compass=1; return 9;
        } else {
            status_message(0,"\1r\1hYou only need one compass!");
            return 0; } }
    if(rmobj[(y*11)+x].item==NUM_SPECIAL+12) {  /* Food */
        temp=(rand()%10)+1; user.health+=temp;
        if(user.health>user.max_health) user.health=user.max_health;
        status_message(1,"\1c\1hYou heal %d points!",temp);
        if((user.health*100)/user.max_health<=25)
            bprintf("\1h\1r\1i");
        else bprintf("\1h\1y");
        bprintf("\x1b[4;41H%6d\1n",user.health);
        rmobj[(y*11)+x].item=0; rmobj[(y*11)+x].val=0;
        put_single_object(z,room,(y*11)+x);
        return 0; }
    if(object[location].type==MONSTER || object[location].type==BLOCKAGE ||
       object[location].type==TRADINGPOST) {
        status_message(0,"\1r\1hYou can't pick that up!");
        return 0; }
    for(w=0;w<6;w++) {
        if(!user.item[w]) {
            user.item[w]=location;
            status_message(0,"\1g\1hPicked up %s",object[user.item[w]].
                name);
            rmobj[(y*11)+x].item=0; rmobj[(y*11)+x].val=0;
            found_empty=1;
            put_single_object(z,room,(y*11)+x);
            return(w+1);
        }
    }
    if(!found_empty) status_message(0,"\1r\1hYou can't carry anymore!");
    return 0;
}
/******************************************************************************
 Call this function to drop an object (including armor, weapon and gold)
******************************************************************************/
int drop_object(int x,int y,int z,int room)
{
    int item;

    if(in_shop) {
        status_message(0,"\1r\1hShops do not buy, only trade and sell!");
        return 0;
    }
    status_message(0,"\1c\1hDrop Which Item (1-6,W,A,G,Q)? ");
    bprintf("\x1b[14;38H"); item=getkeys("wagq",6);

    if(item=='Q') return 0;
    if(item=='W') {
        if(!user.weapon) { status_message(0,"\1r\1hYou have no weapon!");
            return 0;
        } else {
            status_message(0,"\1y\1hDropped your %s.",object[user.weapon].name);
            rmobj[(y*11)+x].item=user.weapon; user.weapon=0;
            put_single_object(z,room,(y*11)+x); return 7;
        }
    }
    if(item=='A') {
        if(!user.armor) { status_message(0,"\1r\1hYou have no armor!");
            return 0;
        } else {
            status_message(0,"\1y\1hDropped your %s.",object[user.armor].name);
            rmobj[(y*11)+x].item=user.armor; user.armor=0;
            put_single_object(z,room,(y*11)+x); return 8;
        }
    }
    if(item=='G') {
        if(!user.gold) { status_message(0,"\1r\1hYou have no gold!");
            return 0;
        } else {
            status_message(0,"\1n\1gDrop how much gold here [\1hQ=Quit, 2500 "
                           "Max\1n\1g]: ");
            status_message(1,"\1r\1hNOTE: Drop gold in increments of 10 GP's");
            bprintf("\x1b[14;50H"); item=getnum(2500);
            item-=item%10;
            if((item/10)<=0) { status_message(0,"\1r\1hNothing dropped!");
                return 0; }
            else {
                if(user.gold<item) item=user.gold;
                user.gold-=item;
                rmobj[(y*11)+x].item=14; rmobj[(y*11)+x].val=(item/10);
                status_message(0,"\1y\1hDropped %d gold pieces.",item);
                put_single_object(z,room,(y*11)+x); return 9;
            }
        }
    }
    if(item&0x8000) item&=~0x8000;
    if(item<=0) return 0;
    if(!user.item[item-1]) {
        status_message(0,"\1r\1hNothing there to drop!");
        return 0;
    }
    status_message(0,"\1y\1hDropped your %s.",object[user.item[item-1]].name);
    rmobj[(y*11)+x].item=user.item[item-1];
    user.item[item-1]=0; put_single_object(z,room,(y*11)+x);
    return(item);
}
/******************************************************************************
 This function is for using a special item, it returns the misc value of the
 item to determine which item it is.
******************************************************************************/
int use_item()
{
    int item,temp;

    status_message(0,"\1c\1hUse Which Item (1-6)? ");
    bprintf("\x1b[14;29H"); item=getnum(6);
    if(item<=0) { status_message(0," "); return 0; }
    temp=user.item[item-1]; user.item[item-1]=0;
    status_message(0," ");
    return(object[temp].misc);
}
/******************************************************************************
 This function reads in ONE floor of the dungeon map.
******************************************************************************/
void read_map_level(int level)
{
    int x,file;

    if((file=nopen("tbdmap.dab",O_RDONLY))==-1) {
        printf("Error opening map file\r\n");
        pause();
        exit(1);}
    lseek(file,(level*(SQUARE*SQUARE)),SEEK_SET);
    for(x=0;x<SQUARE;x++)
        read(file,map[level][x],SQUARE);
    close(file);
}
/******************************************************************************
 This function is where the players can attack each other.  It returns a 0 if
 the attack was unsuccessful.  If the attack was successful, it returns the
 amount of damage done.
******************************************************************************/
int player_attack(int player,uchar level,int ac,int symbol)
{
    char str[256];
    uchar x;
    int hitchance=70,file;
    
    if(user.level>level)
        hitchance+=(user.level-level)*2;
    else if(user.level<level)
        hitchance-=(level-user.level)*2;

    if(ac) hitchance-=ac*5;
    if(symbol==32) hitchance-=20;
    if(symbol==tpic) hitchance+=20;

    if((rand()%100)<=hitchance) {
        if(user.weapon) x=(rand()%(object[user.weapon].misc))+1;
        else x=(rand()%3)+1;                    /* Player is using hands */
        if(symbol==tpic) x=x*2;                 /* double if back stab */
        if(strong) x=x*2;                       /* Potion of Strength    */
    } else x=0;
    sprintf(str,"damage.%d",player);
    if((file=nopen(str,O_WRONLY|O_CREAT))!=-1) {
        write(file,&node_num,1); write(file,&x,1); write(file,&level,1);
        close(file);
    }
    return x;
}
/******************************************************************************
 This function is where the players attack monsters.  It returns a 0 if
 the attack was unsuccessful.  If the attack was successful, it returns the
 amount of damage done.
******************************************************************************/
int attack_monster(uchar level)
{
    char str[256];
    int hitchance=70,x;
    
    if(user.level>level)
        hitchance+=(user.level-level)*2;
    else if(user.level<level)
        hitchance-=(level-user.level)*2;

    if((rand()%100)<=hitchance) {
        if(user.weapon) x=(rand()%(object[user.weapon].misc))+1;
        else x=(rand()%2)+1;                    /* Player is using hands */
        if(strong) x=x*2;                       /* Potion of Strength    */
        return x;
    }
    return 0;
}
/******************************************************************************
 This is where the monster gets to attack a player!
******************************************************************************/
void monster_attack(uchar level)
{
    char str[256];
    uchar x=0,temp;
    int hitchance=70,file;
    
    if(user.level<level)
        hitchance+=(level-user.level)*2;
    else if(user.level>level)
        hitchance-=(user.level-level)*2;

    if(object[user.armor].misc) hitchance-=object[user.armor].misc*5;
    if(tpic==32) hitchance-=20;

    if((rand()%100)<=hitchance) {
        x=(rand()%level)+1;
    }
    sprintf(str,"damage.%d",node_num);
    temp=0;
    if((file=nopen(str,O_WRONLY|O_CREAT))!=-1) {
        write(file,&temp,1); write(file,&x,1); write(file,&level,1);
        close(file);
    }
}
/******************************************************************************
 This routine will scatter a users' items throughout the current room.  This
 occurs when a user is killed in combat
******************************************************************************/
void scatter_items(int z,int room)
{
    int w,spot;

    if(user.gold) {
        spot=find_empty();
        rmobj[spot].item=NUM_GOLD; rmobj[spot].val=(user.gold/10);
        user.gold=0;
        put_single_object(z,room,spot);
    }
    if(user.weapon) {
        spot=find_empty();
        rmobj[spot].item=user.weapon; rmobj[spot].val=0;
        user.weapon=0;
        put_single_object(z,room,spot);
    }
    if(user.armor) {
        spot=find_empty();
        rmobj[spot].item=user.armor; rmobj[spot].val=0;
        user.armor=0;
        put_single_object(z,room,spot);
    }
    for(w=0;w<6;w++) {
        if(user.item[w]) {
            spot=find_empty();
            rmobj[spot].item=user.item[w]; rmobj[spot].val=0;
            user.item[w]=0;
            put_single_object(z,room,spot);
        }
    }
}
/******************************************************************************
 This function will return an empty location in a room (used by the 'scatter
 items' function).  If an empty space is not found after 100 tries, it returns
 the value of the upper left corner of the room, and will overwrite anything
 already existing there.
******************************************************************************/
int find_empty()
{
    int w,x,y;

    for(w=0;w<100;w++) {
        x=rand()%10; y=rand()%4;
        if(!(rmobj[(y*11)+x].item))
            return((y*11)+x);
    }
    return(12);             /* couldn't find a spot after 100 tries, so put  */
                            /* it in the upper left corner of the screen     */
}
/******************************************************************************
 This is used for centering text
******************************************************************************/
void center_wargs(char *str,...)
{
    int i,j;
    va_list argptr;
	char sbuf[1024];

    va_start(argptr,str);
    vsprintf(sbuf,str,argptr);
    va_end(argptr);
    j=bstrlen(sbuf);
    for(i=0;i<(80-j)/2;i++)
        outchar(' ');
//        str1[x]=32;
//    str1[x]=0;
//    strcat(str1,sbuf);
//    strcat(str1,"\r\n\1n");
//    bputs(str1);
    bputs(sbuf);
    bputs("\r\n\1n");
}
/******************************************************************************
 This function reads in a line of data from a file
******************************************************************************/
void readline(char *outstr, int maxlen, FILE *stream)
{
	char str[257];

    fgets(str,256,stream);
    sprintf(outstr,"%-.*s",maxlen,str);
    truncsp(outstr);
}
/******************************************************************************
 This function checks for monsters in the immediate area.  If a monster is
 within 4 squares, it moves to attack.  If a monster is standing next to the
 player, it attacks.
******************************************************************************/
void monster_check(int gx,int gy)
{
    char count=0;
    int x,y,mx,my;

    if(!(gx>=0 && gx<=10 && gy>=0 && gy<=4)) return;

    for(y=-1;y<2;y++) {
        for(x=-1;x<2;x++) {
            if(gx+x>=0 && gx+x<=10 && gy+y>=0 && gy+y<=4) {
                if(rmobj[((gy+y)*11)+(gx+x)].item) {
                    if(object[rmobj[((gy+y)*11)+(gx+x)].item].type==MONSTER &&
                      (clock_tick>=18 || (object[rmobj[((gy+y)*11)+(gx+x)].item]
                      .misc>9 && clock_tick>=9)))
                        monster_attack(object[rmobj[((gy+y)*11)+(gx+x)].item].
                            misc);
                }
            }
        }
    }
    for(y=-4;y<5;y++) {
        for(x=-5;x<6;x++) {         /* Was -10 to 11, now they cover 1/2 room */
            if((x<-1 || x>1 || y<-1 || y>1) && gx+x>=0 && gx+x<=10 &&
                gy+y>=0 && gy+y<=4) {
                count=0;
                if(object[rmobj[((gy+y)*11)+(gx+x)].item].type==MONSTER) {
                    while(lock(rmfile,(long)(user.mapz*SQUARE*SQUARE*110L)+
                        (long)(((user.mapy*SQUARE)+user.mapx)*110L)+
                        ((((gy+y)*11)+(gx+x))*2),2) && count++<100);
                if(count<100) get_single_object(user.mapz,(user.mapy*SQUARE)+
                        user.mapx,((gy+y)*11)+(gx+x)); }
                if(object[rmobj[((gy+y)*11)+(gx+x)].item].type==MONSTER &&
                   count<100 && (clock_tick>=18 || (object[rmobj[((gy+y)*11)+
                   (gx+x)].item].misc>9 && clock_tick>=9))) {
                    if(y<0) my=1; if(x<0) mx=1; if(y==0) my=0;
                    if(x==0) mx=0; if(y>0) my=-1; if(x>0) mx=-1;
                    if(!(rmobj[((gy+(y+my))*11)+(gx+(x+mx))].item) &&
                       !(initial_inway(user.mapx,user.mapy,user.mapz,
                       (x+mx+gx),(y+my+gy)))) {
                        rmobj[((gy+(y+my))*11)+(gx+(x+mx))].item=
                            rmobj[((gy+y)*11)+(gx+x)].item;
                        rmobj[((gy+(y+my))*11)+(gx+(x+mx))].val=
                            rmobj[((gy+y)*11)+(gx+x)].val;
                        rmobj[((gy+y)*11)+(gx+x)].item=0;
                        rmobj[((gy+y)*11)+(gx+x)].val=0;
                        put_single_object(user.mapz,(user.mapy*SQUARE)+
                            user.mapx,((gy+y)*11)+(gx+x));
                        put_single_object(user.mapz,(user.mapy*SQUARE)+
                            user.mapx,((gy+(y+my))*11)+(gx+(x+mx)));
                        bprintf("\x1b[%d;%dH ",(gy+y)+7,(gx+x)+35);
                        attr(object[rmobj[((gy+(y+my))*11)+(gx+(x+mx))].item].
                            color);
                        bprintf("\x1b[%d;%dH%c\1y\1h",(gy+(y+my))+7,
                            (gx+(x+mx))+35,
                            object[rmobj[((gy+(y+my))*11)+(gx+(x+mx))].item].
                            symbol);
                        return;
                    }
                }
                unlock(rmfile,(long)(user.mapz*SQUARE*SQUARE*110L)+
                    (long)(((user.mapy*SQUARE)+user.mapx)*110L)+
                    ((((gy+y)*11)+(gx+x))*2),2);
            }
        }
    }
    return;
}
/******************************************************************************
 This function checks to see if someone is there to warp to.  Returns 1 if
 there is otherwise it returns 0.
******************************************************************************/
int warp_to_other()
{
    char chbuf[8],count;
    int n,whonum=0,who[256];

    lseek(chfile,0L,SEEK_SET);
    for(n=0;n<sys_nodes;n++) {
        count=0;
        if(n==node_num-1) lseek(chfile,8L,SEEK_CUR);
        if(n!=node_num-1) {
            while(read(chfile,chbuf,8)==-1 && count++<100);
            if(count>=100) lseek(chfile,8L,SEEK_CUR);
            if(chbuf[0]) who[whonum++]=n;
        }
    }
    if(!whonum) return (whonum);
    do {
        n=who[xp_random(whonum)]; count=0;
        lseek(chfile,8L*(long)n,SEEK_SET);
        while(read(chfile,chbuf,8)==-1 && count++<100);
    } while(count>=100);
    user.mapx=chbuf[1]; user.mapy=chbuf[2]; user.mapz=chbuf[3];
    user.roomx=chbuf[4]; user.roomy=chbuf[5];
    return(whonum);
}
