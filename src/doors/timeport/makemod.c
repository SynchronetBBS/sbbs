#include <string.h>
#include "doors.h"
#include "tplib.h"

# // ' ***** MakeMod Module *****
int main(int argc, char **argv)
{
	FILE *file;

	Ocls();
	SetColor(13, 0, 0);
	Oprint("     ------------------\r\n");
	Oprint("     MAKEMOD.EXE Active\r\n");
	Oprint("     Building DataFiles\r\n");
	Oprint("     ------------------\r\n");

	file=SharedOpen("items.dat","w",LOCK_EX);
	
/*
	'***************************** DATA DEFINITIONS **********************
    	 'Description(20)
    	 'Buy Cost(8)
    	 'Num(3)
    	 '.2345678901234567890.2345678.01.345.7890123456789
*/
	fputs("Healing Crystal     5       1                                                   ",file);
	fputs("Sturdy Stick        100     2                                                   ",file);
	fputs("Nylon String        100     3                                                   ",file);
	fputs("Sharp, Bent Nail    100     4                                                   ",file);
	fputs("Fishing Pole        0       5                                                   ",file);
	fputs("Fresh Fish          0       6                                                   ",file);
	fputs("Shovel              5000    7                                                   ",file);
	fputs("Lock Picks          2000    8                                                   ",file);
	fputs("Diamond             250000  9                                                   ",file);
	fputs("Huge Board          0       10                                                  ",file);
	fputs("Long Feather        0       11                                                  ",file);
	fputs("Light Fabric        0       12                                                  ",file);
	fputs("Huge Wings          0       13                                                  ",file);
	SetColor(12, 0,0);
	fclose(file);
	Oprint("     -- ITEMS.DAT    --\r\n");


	file=SharedOpen("deaths.txt","w",LOCK_EX);
    	fputs("22\r\n",file);
    	fputs("`$*1 yells out in anguish!\r\n",file);
    	fputs("`$*2 is very pleased with the outcome.\r\n",file);
    	fputs("`$*1 dies without uttering a word.\r\n",file);
    	fputs("`@'I bet you want to be just like me,' says *2 to *1.\r\n",file);
    	fputs("`@'It wasn't even a challenge,' comments *2.\r\n",file);
    	fputs("`#'*1 is so weak,' taunts *2.\r\n",file);
    	fputs("`#'Let's fight again when you aren't such a wimp!' states *2.\r\n",file);
    	fputs("`#*1 tries to trip *2 on the way to the ground.\r\n",file);
    	fputs("`#'Yes, I'm a sore loser,' states *1.  'I'll have my revenge!'\r\n",file);
    	fputs("`@*2 almost feels guilty... but gets over it quickly.\r\n",file);
    	fputs("`@'It was my pleasure, *1,' says *2 smugly.\r\n",file);
    	fputs("`4'You'll pay for that, *2,' grunts *1.\r\n",file);
    	fputs("`4'You cheated somehow, *2,' says *1 in denial.\r\n",file);
    	fputs("`4'Life isn't very fair,' *1 comments.\r\n",file);
    	fputs("`@*2 kicks *1 again for good measure\r\n",file);
    	fputs("`2*2 laughs at *1's unfortunate demise.\r\n",file);
    	fputs("`6*1 is in shock;  'I thought I was the best!'\r\n",file);
    	fputs("`4'I almost had you,' says *1 to *2\r\n",file);
    	fputs("`5*1 is humiliated;  `#'Don't ever come near me again, *2!'\r\n",file);
    	fputs("`5'I feel pretty worthless right now,' *1 comments.\r\n",file);
    	fputs("`2'I was having such a great day before this!' grunts *1.\r\n",file);
    	fputs("`2'You should be ashamed for that, *2,' *1 whines.\r\n",file);
	fclose(file);
	Oprint("     -- DEATHS.TXT   --\r\n");

	file=SharedOpen("daily.txt","w",LOCK_EX);
    	fputs("9\r\n",file);
    	fputs("`@A temporal shockwave came from a 1943 Bermuda Triangle accident.\r\n",file);
    	fputs("`@The autograph of Christopher Columbus was obtained today.\r\n",file);
    	fputs("`@A Time Traveler introduced Small Pox to the Aztecs, but was killed by Cortez.\r\n",file);
    	fputs("`@A Traveler went to Ancient Greece last week but has yet to return.\r\n",file);
    	fputs("`@Two Time Travelers were killed by dinosaurs eons ago, today.\r\n",file);
    	fputs("`@Airplane designs were smuggled to the Wright Brothers today.\r\n",file);
    	fputs("`@Zagala Benvora killed a Time Traver during WW4 today.\r\n",file);
    	fputs("`@A Time Traveler inspired King Henry to start a school for sailors today.\r\n",file);
    	fputs("`0Evidence that aliens built the Egyptian Pyramids was uncovered today.\r\n",file);
	fclose(file);
	Oprint("     -- DAILY.TXT    --\r\n");

	file=SharedOpen("mutants.1","w",LOCK_EX);
    	fputs("3\r\n",file);
    	fputs("Brown-Eyed Blob\r\n",file);
    	fputs("Jhengo Warrior\r\n",file);
    	fputs("Mutant Bird\r\n",file);
	fclose(file);
	Oprint("     -- MUTANTS.1    --\r\n");

	file=SharedOpen("mutants.2","w",LOCK_EX);
    	fputs("3\r\n",file);
    	fputs("Green-Eyed Blob\r\n",file);
    	fputs("Mutant Rat\r\n",file);
    	fputs("Toy Cyborg\r\n",file);
	fclose(file);
	Oprint("     -- MUTANTS.2    --\r\n");

	file=SharedOpen("mutants.3","w",LOCK_EX);
    	fputs("3\r\n",file);
    	fputs("Blue-Eyed Blob\r\n",file);
    	fputs("Six-Toed Blob\r\n",file);
    	fputs("Deformo\r\n",file);
	fclose(file);
	Oprint("     -- MUTANTS.3    --\r\n");

	file=SharedOpen("mutants.4","w",LOCK_EX);
    	fputs("3\r\n",file);
    	fputs("Flying Phantom\r\n",file);
    	fputs("Golden Robot\r\n",file);
    	fputs("Deformo's Twin\r\n",file);
	fclose(file);
	Oprint("     -- MUTANTS.4    --\r\n");

	file=SharedOpen("mutants.5","w",LOCK_EX);
    	fputs("3\r\n",file);
    	fputs("Two-Headed Rat\r\n",file);
    	fputs("Slobbering Fool\r\n",file);
    	fputs("White Specter\r\n",file);
	fclose(file);
	Oprint("     -- MUTANTS.5    --\r\n");

	file=SharedOpen("mutants.6","w",LOCK_EX);
    	fputs("3\r\n",file);
    	fputs("Twisted Tarzan\r\n",file);
    	fputs("Radioactive Child\r\n",file);
    	fputs("Two-Headed Dog\r\n",file);
	fclose(file);
	Oprint("     -- MUTANTS.6    --\r\n");

	file=SharedOpen("mutants.7","w",LOCK_EX);
    	fputs("3\r\n",file);
    	fputs("Radioactive Mutant\r\n",file);
    	fputs("Two-Headed Chimp\r\n",file);
    	fputs("Power Thrower\r\n",file);
	fclose(file);
	Oprint("     -- MUTANTS.7    --\r\n");

	file=SharedOpen("mutants.8","w",LOCK_EX);
    	fputs("3\r\n",file);
    	fputs("Mutated Maggot\r\n",file);
    	fputs("Mutated Worm\r\n",file);
    	fputs("Two-Headed Beast\r\n",file);
	fclose(file);
	Oprint("     -- MUTANTS.8    --\r\n");

	file=SharedOpen("mutants.9","w",LOCK_EX);
    	fputs("3\r\n",file);
    	fputs("Evil-Eyed Cyborg\r\n",file);
    	fputs("Cyclops Cyborg\r\n",file);
    	fputs("Radioactive Blog\r\n",file);
	fclose(file);
	Oprint("     -- MUTANTS.9    --\r\n");

	file=SharedOpen("mutants.10","w",LOCK_EX);
    	fputs("3\r\n",file);
    	fputs("Mutant Roach\r\n",file);
    	fputs("Mutant Punk Rocker\r\n",file);
    	fputs("Mutant Repairman\r\n",file);
	fclose(file);
	Oprint("     -- MUTANTS.10   --\r\n");

	file=SharedOpen("mutants.11","w",LOCK_EX);
    	fputs("3\r\n",file);
    	fputs("Mutated Disease\r\n",file);
    	fputs("Mutated Cyborg\r\n",file);
    	fputs("Siamese Mutants\r\n",file);
	fclose(file);
	Oprint("     -- MUTANTS.11   --\r\n");

	file=SharedOpen("mutants.12","w",LOCK_EX);
    	fputs("3\r\n",file);
    	fputs("Ultimate Demon\r\n",file);
    	fputs("Super Mutant\r\n",file);
    	fputs("Living Paradox\r\n",file);
	fclose(file);
	Oprint("     -- MUTANTS.12   --\r\n");
	return(0);
}

void UseItem(long c,short b) {}
void SaveGame(void) {}
