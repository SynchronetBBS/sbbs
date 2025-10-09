#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <OpenDoor.h>	// WORD, DWORD, INT16, and INT32

#include "filename.h"
#include "structs.h"

#define LEVELS 21

void dump(void);

int
main(void) {
	FILE *justfile;
	enemy erec;
	enemy_idx eidx;
	char key;
	INT16 num=0;
	INT16 lvl=0;
	INT16 cnt;
	system("mv " ENEMY_FILENAME " nyenmb.dat");
	system("mv " ENEMY_INDEX " nyenmb.idx");
	system("rm " ENEMY_FILENAME);
	system("rm " ENEMY_INDEX);

	eidx.first_enemy[0]=0;
	do {
		printf("\nA-Add N-Next level Q-Quit>");
		scanf("%c",&key);
		dump();

		if (key=='A' || key=='a') {
			int32_t n;
			printf("Name of the sucker:");
			cnt=0;
			do {
				scanf("%c",&key);
				erec.name[cnt]=key;
				cnt++;
			} while (key!='\n' && key!='\r' && cnt<36);
			erec.name[cnt-1]=0;
			if (key!='\n' && key!='\r')
				dump();

			printf("Hitpoints:");
			scanf("%" SCNd32,&n);
			erec.hitpoints = n;
			dump();

			printf("Strength:");
			scanf("%" SCNd32,&n);
			erec.strength = n;
			dump();

			printf("Defense:");
			scanf("%" SCNd32,&n);
			erec.defense = n;
			dump();

			printf("Arm:");
			scanf("%hhd",&erec.arm);

			dump();

			justfile=fopen(ENEMY_FILENAME,"a+b");
			printf("Atpos: %lu\n\n",ftell(justfile)/sizeof(enemy));
			fwrite(&erec,sizeof(enemy),1,justfile);
			fclose(justfile);

			num++;
		} else if (key=='N' || key=='n') {
			eidx.last_enemy[lvl]=num;
			lvl++;
			eidx.first_enemy[lvl]=num;
		}
	} while (key!='q' && key!='Q');
	eidx.last_enemy[lvl]=(--num);
	justfile=fopen(ENEMY_INDEX,"wb");
	fwrite(&eidx,sizeof(enemy_idx),1,justfile);
	fclose(justfile);
	printf("\nDone!");
	return 0;
}

void
dump(void) {
	char key;
	do {
		scanf("%c",&key);
	} while (key!='\n');
}


