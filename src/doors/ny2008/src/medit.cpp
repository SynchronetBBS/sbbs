#include <stdio.h>
#include <string.h>

#include "const.h"
#include "structs.h"
#include "filename.h"

void dump(void);

int
main(void) {
	FILE *justfile;
	enemy erec;
	enemy_idx eidx;
	char key;
	INT16 num,cnt;
	char nnm[36];

	do {
		printf("\n\nA-Add V-View E-Edit Q-Quit >");
		scanf("%c",&key);
		dump();

		if (key=='A' || key=='a') {
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
			scanf("%d",&erec.hitpoints);
			dump();

			printf("Strength:");
			scanf("%d",&erec.strength);
			dump();

			printf("Defense:");
			scanf("%d",&erec.defense);
			dump();

			printf("Arm:");
			scanf("%d",&erec.arm);

			dump();

			justfile=fopen(ENEMY_FILENAME,"a+b");
			printf("Atpos: %d\n\n",ftell(justfile)/sizeof(enemy));
			fwrite(&erec,sizeof(enemy),1,justfile);
			fclose(justfile);
		} else if (key=='V' || key=='v') {
			printf("\nView which record:");
			scanf("%d",&num);
			dump();

			justfile=fopen(ENEMY_FILENAME,"rb");
			fseek(justfile,num*sizeof(enemy),SEEK_SET);
			fread(&erec,sizeof(enemy),1,justfile);
			fclose(justfile);

			printf("Name of the sucker: %s\n",erec.name);
			printf("Hitpoints: %d\n",erec.hitpoints);
			printf("Strength: %d\n",erec.strength);
			printf("Defense: %d\n",erec.defense);
			printf("Arm: %d\n",erec.arm);

		} else if (key=='E' || key=='e') {

			printf("\nEdit which record:");
			scanf("%d",&num);

			dump();

			justfile=fopen(ENEMY_FILENAME,"rb");
			fseek(justfile,num*sizeof(enemy),SEEK_SET);
			fread(&erec,sizeof(enemy),1,justfile);
			fclose(justfile);

			printf("Name of the sucker: %s\n",erec.name);
			printf("Hitpoints: %d\n",erec.hitpoints);
			printf("Strength: %d\n",erec.strength);
			printf("Defense: %d\n",erec.defense);
			printf("Arm: %d\n",erec.arm);

			printf("\nName of the sucker:");
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
			scanf("%d",&erec.hitpoints);
			dump();

			printf("Strength:");
			scanf("%d",&erec.strength);
			dump();

			printf("Defense:");
			scanf("%d",&erec.defense);
			dump();

			printf("Arm:");
			scanf("%d",&erec.arm);

			dump();

			justfile=fopen(ENEMY_FILENAME,"r+b");
			fseek(justfile,num*sizeof(enemy),SEEK_SET);
			fwrite(&erec,sizeof(enemy),1,justfile);
			fclose(justfile);
		}

	} while (key!='q' && key!='Q');

	printf("\n\nRebuild the idx file? yn >");
	scanf("%c",&key);
	dump();

	if (key=='y' || key=='Y') {
		justfile=fopen(ENEMY_INDEX,"rb");
		fread(&eidx,sizeof(enemy_idx),1,justfile);
		fclose(justfile);
		for (num=0;num<=20;num++) {
			printf("input first enemy for level %d, was %d :",num,eidx.first_enemy[num]);
			scanf("%d",&eidx.first_enemy[num]);
			printf("input last enemy for level %d, was %d :",num,eidx.last_enemy[num]);
			scanf("%d",&eidx.last_enemy[num]);
			dump();
		}
		justfile=fopen(ENEMY_INDEX,"wb");
		fwrite(&eidx,sizeof(enemy_idx),1,justfile);
		fclose(justfile);
	}
	printf("\nDone!");
}

void
dump(void) {
	char key;
	do {
		scanf("%c",&key);
	} while (key!='\n');
}


