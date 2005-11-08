/*
 *  The Beast's Domain door game
 *  Copyright (C) 2005  Domain Entertainment
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <time.h>
#include <fcntl.h>
#if 0
#include <dos.h>
#include <io.h>
#include <conio.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "xsdk.h"		/* Colour definitions */
#include "objects.h"

#define N  (1<<0)
#define S  (1<<1)
#define E  (1<<2)
#define W  (1<<3)
#define NW (1<<4)
#define NE (1<<5)
#define SW (1<<6)
#define SE (1<<7)

#define LEVELS      10
#define SQUARE      30

#define WEAPON      1   /* Is the object a weapon? */
#define ARMOR       2   /* Is the object armor? */
#define GOLD        3   /* Is the object gold? */
#define SPECIAL     4   /* Is the object special (wand/spell/potion)? */
#define STAIRS      5   /* Stairway to another level */
#define TRADINGPOST 6   /* Store for Buy/Sell */
#define MONSTER     7
#define BLOCKAGE    8

#define NUM_WEAPON      1  /* 7  */
#define NUM_ARMOR       8  /* 6  */
#define NUM_GOLD        14 /* 1  */
#define NUM_SPECIAL     15 /* 13 */
#define NUM_STAIRS      28 /* 2  */
#define NUM_TRADINGPOST 30 /* 3  */
#define NUM_MONSTER     33 /* 23 */
#define NUM_BLOCKAGE    56 /* 6  */

int main(int argc, char **argv)
{
    FILE *stream;
    char door=0,room[SQUARE][SQUARE],str[128];
    unsigned char ch,val;
    int file,v=0,w=0,x=0,y=0,z=1,weapons[10],weapons2[10],armor[10],armor2[10],
        magic[10],magic2[10],num_monster,num_gold,fountain[10],stairs,staff,
        save=0;
    long lastrun;

    if(argc > 1) {
        if(!stricmp(argv[1],"/?")) {
            if(argc);
            printf("\r\nThe Beast's Domain  Reroller  Copyright 1993 Domain "
                    "Entertainment\r\n");
            printf("\r\nUsage: REROLL /SAVE\r\n");
            printf("\r\nWhere /SAVE     prevents the user data file "
                  "from being deleted\r\n");
            return(1);
        }
        if(!stricmp(argv[1],"/SAVE")) save=1;
    }

    xp_randomize();
#if 0
    clrscr();
#endif
    printf("Creating The Beast's Domain (may take several minutes)!\r\n\r\n");
    if(!save) unlink("tbd.usr");
    else printf("Saving current user file...");
    unlink("tbd.mnt");
    unlink("topten.ans");
    unlink("tbdmap.dab");
    unlink("tbdtoday.log");
    unlink("tbdchar.pos");
/*    if((file=open("tbdmap.dab",O_RDWR|O_BINARY|O_CREAT,S_IWRITE|S_IREAD))==-1) {
        printf("Error opening map file\r\n");
        exit(0); } */
    if((stream=fopen("tbdmap.dab","w+b"))==NULL) {
        printf("Error opening map file\r\n");
        exit(0); }

    printf("Creating rooms and checking throughways.");
for (w=0;w<LEVELS;w++) {
    printf(".");
/*    lseek(file,w*SQUARE*SQUARE,SEEK_SET); */
    fseek(stream,w*SQUARE*SQUARE,SEEK_SET);
    for (x=0;x<SQUARE*SQUARE;x++) {
        for (y=0;y<7;y++) {         /* number of possible doors in each room */
            if (xp_random(4)) {
                switch(y) {
                    case 0:
                        if (x>SQUARE-1) door|=N;
                        break;
                    case 1:
                        if (x<(SQUARE*SQUARE)-SQUARE) door|=S;
                        break;
                    case 2:
                        if ((x%SQUARE)<SQUARE-1) door|=E;
                        break;
                    case 3:
                        if ((x%SQUARE)>0) door|=W;
                        break;
                    case 4:
                        if (x>SQUARE-1 && (x%SQUARE)>0) door|=NW;
                        break;
                    case 5:
                        if (x>SQUARE-1 && (x%SQUARE)<SQUARE-1) door|=NE;
                        break;
                    case 6:
                        if (x<(SQUARE*SQUARE)-SQUARE && (x%SQUARE)>0) door|=SW;
                        break;
                    case 7:
                        if (x<(SQUARE*SQUARE)-SQUARE && (x%SQUARE)<SQUARE-1)
                            door|=SE;
                        break;
                }
            }
        }
        if (!door) {
            if(w!=7) {
                if (x>SQUARE-1) door|=N;
                else if (x<(SQUARE*SQUARE)-SQUARE) door|=S;
                else if ((x%SQUARE)<SQUARE-1) door|=E;
                else if ((x%SQUARE)>0) door|=W;
    /*          else { door|=N; door|=S; door|=E; door|=W; } */
            }
        }
        if((w==0 || w==1 || w==5 || w==6) && (x>=0 && x<=SQUARE-1))
            door|=N;
        if(w==7 && x==SQUARE/2) door|=N;
        if(w==7 && x!=SQUARE/2) door&=~N;
        if((w==1 || w==2 || w==4 || w==5) && (x>=(SQUARE*SQUARE)-SQUARE
            && x<=(SQUARE*SQUARE)))
            door|=S;
        if(w==8 && x==((SQUARE*SQUARE)-(SQUARE/2))) door|=S;
        if(w==8 && x!=((SQUARE*SQUARE)-(SQUARE/2))) door&=~S;
        if((w==3 || w==4 || w==6) && ((x%SQUARE)==SQUARE-1))
            door|=E;
        if((w==2 || w==3 || w==7) && ((x%SQUARE)==0))
            door|=W;
        if(w==0) door&=~W;
        if(w==7) door&=~E;

/*        write(file,&door,1); */
        fwrite(&door,sizeof(door),1,stream);
        z++; if (z>SQUARE) { z=1; } door=0;
    }
/*    lseek(file,w*SQUARE*SQUARE,SEEK_SET); */
    fseek(stream,w*SQUARE*SQUARE,SEEK_SET);
    for (y=0;y<SQUARE;y++)
        for (x=0;x<SQUARE;x++) /* read(file,&room[y][x],1); */
            fread(&room[y][x],1,1,stream);
    for (y=0;y<SQUARE;y++) {
        for (x=0;x<SQUARE;x++) {
            if (y>0 && (room[y][x]&N))
                if (!(room[y-1][x]&S)) room[y-1][x]|=S;
            if (y<SQUARE-1 && (room[y][x]&S))
                if (!(room[y+1][x]&N)) room[y+1][x]|=N;
            if (x>0 && (room[y][x]&W))
                if (!(room[y][x-1]&E)) room[y][x-1]|=E;
            if (x<SQUARE-1 && (room[y][x]&E))
                if (!(room[y][x+1]&W)) room[y][x+1]|=W;
            if (y>0 && x>0 && (room[y][x]&NW))
                if (!(room[y-1][x-1]&SE)) room[y-1][x-1]|=SE;
            if (y>0 && x<SQUARE-1 && (room[y][x]&NE))
                if (!(room[y-1][x+1]&SW)) room[y-1][x+1]|=SW;
            if (y<SQUARE-1 && x>0 && (room[y][x]&SW))
                if (!(room[y+1][x-1]&NE)) room[y+1][x-1]|=NE;
            if (y<SQUARE-1 && x<SQUARE-1 && (room[y][x]&SE))
                if (!(room[y+1][x+1]&NW)) room[y+1][x+1]|=NW;
        }
    }
/*    lseek(file,w*SQUARE*SQUARE,SEEK_SET); */
    fseek(stream,w*SQUARE*SQUARE,SEEK_SET);
    for (y=0;y<SQUARE;y++)
        for (x=0;x<SQUARE;x++) /* write(file,&room[y][x],1); */
            fwrite(&room[y][x],1,1,stream);
}
/*   close(file); */
fclose(stream);

    for (x=0;x<LEVELS;x++) {
        do {
            weapons[x]=(rand()%(SQUARE*SQUARE));
            armor[x]=(rand()%(SQUARE*SQUARE));
            magic[x]=(rand()%(SQUARE*SQUARE));
            fountain[x]=(rand()%(SQUARE*SQUARE));
        } while (weapons[x]==armor[x] || weapons[x]==magic[x] || armor[x]==
                 magic[x] || fountain[x]==magic[x] || fountain[x]==armor[x] ||
                 fountain[x]==weapons[x]);
        do {
            weapons2[x]=(rand()%(SQUARE*SQUARE));
            armor2[x]=(rand()%(SQUARE*SQUARE));
            magic2[x]=(rand()%(SQUARE*SQUARE));
        } while(weapons2[x]==armor2[x] || weapons2[x]==magic2[x] || armor2[x]==
                magic2[x]);
    }
    unlink("tbdobj.dab");
/*    if((file=open("tbdobj.dab",O_RDWR|O_BINARY|O_CREAT,S_IWRITE|S_IREAD))==-1) {
        printf("Error opening object data file\r\n");
        exit(0); } */
    if((stream=fopen("tbdobj.dab","w+b"))==NULL) {
        printf("Error opening object data file\r\n");
        exit(0); }

    stairs=xp_random(SQUARE*SQUARE);
    do { staff=xp_random(SQUARE*SQUARE); } while(staff==stairs);
    printf("\r\n\r\nStocking The Beast's Domain.");
for (w=0;w<LEVELS;w++) {
    for (x=0;x<SQUARE*SQUARE;x++) {
        if(x%SQUARE==0) printf(".");
        num_gold=num_monster=0;
        if(fountain[w]==x && w<8) {
            val=0;
            for(y=0;y<55;y++) {
                if((y>14 && y<18) || (y>24 && y<30) || (y>36 && y<40))
                    ch=NUM_BLOCKAGE+4;
                else ch=0;
/*                write(file,&ch,1); write(file,&val,1); } */
                fwrite(&ch,sizeof(ch),1,stream);
                fwrite(&val,sizeof(val),1,stream); }
        } else if((weapons[w]==x || weapons2[w]==x) && w<8) {
            ch=NUM_TRADINGPOST; val=0;
/*            write(file,&ch,1); write(file,&val,1); */
            fwrite(&ch,sizeof(ch),1,stream);
            fwrite(&val,sizeof(val),1,stream);
            for (y=0;y<3;y++) {
                for(v=0;v<18;v++) {
                    if(w<5 && w>1) ch=y+NUM_WEAPON+1;
                    else ch=y+4+NUM_WEAPON;
/*                    write(file,&ch,1); write(file,&val,1); */
                    fwrite(&ch,sizeof(ch),1,stream);
                    fwrite(&val,sizeof(val),1,stream);

                }
            }
        } else if((armor[w]==x || armor2[w]==x) && w<8) {
            ch=NUM_TRADINGPOST+1; val=0;
/*            write(file,&ch,1); write(file,&val,1); */
            fwrite(&ch,sizeof(ch),1,stream);
            fwrite(&val,sizeof(val),1,stream);

            for (y=0;y<3;y++) {
                for(v=0;v<18;v++) {
                    if(w<5 && w>1) ch=y+NUM_ARMOR;
                    else ch=y+3+NUM_ARMOR;
/*                    write(file,&ch,1); write(file,&val,1); */
                    fwrite(&ch,sizeof(ch),1,stream);
                    fwrite(&val,sizeof(val),1,stream);

                }
            }
        } else if((magic[w]==x || magic2[w]==x) && w<8) {
            ch=NUM_TRADINGPOST+2; val=0;
/*            write(file,&ch,1); write(file,&val,1); */
            fwrite(&ch,sizeof(ch),1,stream);
            fwrite(&val,sizeof(val),1,stream);

            for (y=0;y<9;y++) {
                for(v=0;v<6;v++) {
                    ch=y+NUM_SPECIAL;
/*                    write(file,&ch,1); write(file,&val,1); */
                    fwrite(&ch,sizeof(ch),1,stream);
                    fwrite(&val,sizeof(val),1,stream);

                }
            }
        } else {
            for (y=0;y<55;y++) {
                door=ch=val=0;
                if(y!=0 && y!=5 && y!=10 && y!=22 && y!=32 && y!=44 && y!=49 &&
                   y!=54) {
                    if(rand()%10==1) {
                        if(w<5 && w>1) ch=NUM_BLOCKAGE+1;
                        else if(w==1 || w==5) ch=NUM_BLOCKAGE+5;
                        else if(w<8) ch=NUM_BLOCKAGE;
                        else if(w==8 || w==9) ch=NUM_MONSTER+14+rand()%8; }
                } else door=1;
                if(rand()%20==1 && !ch) {
                    ch=(rand()%58)+1;
                    if(ch==NUM_BLOCKAGE) {
                        if(w<5 && w>1) ch=NUM_BLOCKAGE+1;
                        else if(w==1 || w==5) ch=NUM_BLOCKAGE+5;
                        else if(w<8) ch=NUM_BLOCKAGE;
                        else if(w==8 || w==9) ch=NUM_MONSTER+14+rand()%8; } }
                if(object[ch].type!=STAIRS && object[ch].type!=BLOCKAGE &&
                   object[ch].type!=GOLD && ch!=0) {
                    if(object[ch].type==WEAPON) ch=NUM_WEAPON;
                    else if(w<5 && w>1) ch=NUM_MONSTER+rand()%7;
                    else if(w==1 || w==5) ch=NUM_MONSTER+7+rand()%7;
                    else ch=NUM_MONSTER+14+rand()%8; }
                if(object[ch].type==TRADINGPOST && !door) {
                    if(w<5 && w>1) ch=NUM_BLOCKAGE+1;
                    else if(w==1 || w==5) ch=NUM_BLOCKAGE+5;
                    else if(w<8) ch=NUM_BLOCKAGE;
                    else if(w==8 || w==9) ch=NUM_MONSTER+14+rand()%8; }
                if(object[ch].type==TRADINGPOST) {
                    if(w<5 && w>1) ch=NUM_MONSTER+rand()%7;
                    else if(w==1 || w==5) ch=NUM_MONSTER+7+rand()%7;
                    else ch=NUM_MONSTER+14+rand()%8; }
                if(object[ch].type==STAIRS) {
                    if(w<5 && w>1) ch=NUM_MONSTER+rand()%7;
                    else if(w==1 || w==5) ch=NUM_MONSTER+7+rand()%7;
                    else ch=NUM_MONSTER+14+rand()%8; 
                }
                if(object[ch].type==GOLD) {
                    ++num_gold;
                    if(num_gold>1) { ch=0; val=0; }
                    else val=((rand()%4)+1); }
                if(object[ch].type==MONSTER) {
                    ++num_monster;
                    if(num_monster>2 && w<8) { ch=0; val=0; }
                    if(num_monster>3 && w==8) { ch=0; val=0; }
                    if(num_monster>4 && w==9) { ch=0; val=0; }
                    else val=(object[ch].misc*10)+10; }
                if(object[ch].type==BLOCKAGE) {
                    if(w<8 && door) { ch=0; val=0; }
                    if(w<8 && !door) {
                        if(w<5 && w>1) ch=NUM_BLOCKAGE+1;
                        else if(w==1 || w==5) ch=NUM_BLOCKAGE+5;
                        else if(w<8) ch=NUM_BLOCKAGE; } }
                if(y==14 || y==18 || y==36 || y==40) {
                    if(w==8) { ch=NUM_BLOCKAGE+2; val=0; }
                    if(w==9) { ch=NUM_BLOCKAGE+3; val=0; } }
                if(w==9 && staff==x && y==28) { ch=NUM_MONSTER+22;
                    val=(object[ch].misc*10)+10; }
                if(w==8 && stairs==x && y==28) ch=NUM_STAIRS+1;
                if(w==9 && stairs==x && y==28) ch=NUM_STAIRS;
                if(object[ch].type==SPECIAL) {
                    ch=0; val=0; }
/*                write(file,&ch,1); write(file,&val,1); */
                fwrite(&ch,sizeof(ch),1,stream);
                fwrite(&val,sizeof(val),1,stream);

            }
        }
        z++; if(z>SQUARE) { z=1; }
    }
}
/* close(file); */
fclose(stream);
if((file=open("tbd.mnt",O_WRONLY|O_BINARY|O_CREAT,S_IWRITE|S_IREAD))!=-1) {
#if 0
    struct time t;
    struct date d;
    unixtodos(lastrun,&d,&t);
    t.ti_hour=t.ti_min=t.ti_sec=t.ti_hund=0;
    lastrun=dostounix(&d,&t);
#endif
    lastrun=time(NULL);
    lastrun -= lastrun % 86400;
    write(file,&lastrun,4); close(file); }
printf("\r\n\r\nProgram Complete!  The Beast's Domain is ready to go!");
return(0);
}
