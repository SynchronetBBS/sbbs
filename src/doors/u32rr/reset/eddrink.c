/*

Copyright 2007 Jakob Dangarden

 This file is part of Usurper.

    Usurper is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Usurper is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Usurper; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "files.h"

void Reset_Drink(void)
{

 /*typ av objekt (ttype) :
  1. head
  2. body
  3. arms
  4. hands
  5. fingers
  6. legs
  7. feets
  8. waist
  9. neck
  10. face
  11. shield
  12. mat
  13. dryck
  14. vapen*/

 /*** Dryck ***/
 Add_Object("Red Bottle",   /*name*/
         Drink,     /*typ*/
         50,        /*v„rde i gold*/
         0,         /*”ka/minska hps*/
         0,         /*”ka stamina*/
         0,         /*”ka agility*/
         0,         /*”ka charisma*/
         0,         /*”ka dexterity*/
         0,         /*”ka wisdom*/
         0,         /*”ka mana*/
         0,         /*”ka armor v„rde*/
         0,         /*”ka attack v„rde*/
         "",        /*„gd av?*/
         false,     /*bara EN i spelet*/
         Blindness, /*kurerar*/
         false,     /*finnas i shoppen*/
         true,      /*kunna hittas i dungeons?*/
         false,     /*cursed item?*/
         1,         /*min level att hittas i dngs*/
         99,        /*max level att hittas i dngs*/
         "It contains a strange yellow liquid. Looks disgusting!",
         "",        /*normal beskrivning 2*/
         "",        /*normal beskrivning 3*/
         "",        /*normal beskrivning 4*/
         "",        /*normal beskrivning 5*/
         "Cure for Blindness.",
         "",
         "",        /*detaljerad beskrivning 3*/
         "",        /*detaljerad beskrivning 4*/
         "",        /*detaljerad beskrivning 5*/
         0,         /*”ka strength*/
         0,         /*”ka defence*/
         0);        /*strength demanded to use object*/

 Add_Object("Bottle",   /*name*/
         Drink,     /*typ*/
         50,        /*v„rde i gold*/
         0,         /*”ka/minska hps*/
         0,         /*”ka stamina*/
         0,         /*”ka agility*/
         0,         /*”ka charisma*/
         0,         /*”ka dexterity*/
         0,         /*”ka wisdom*/
         0,         /*”ka mana*/
         0,         /*”ka armor v„rde*/
         0,         /*”ka attack v„rde*/
         "",        /*„gd av?*/
         false,     /*bara EN i spelet*/
         Plague,    /*kurerar*/
         false,     /*finnas i shoppen*/
         true,      /*kunna hittas i dungeons?*/
         false,     /*cursed item?*/
         1,         /*min level att hittas i dngs*/
         99,        /*max level att hittas i dngs*/
         "It is green and has a large \"X\" written on it.",
         "",        /*normal beskrivning 2*/
         "",        /*normal beskrivning 3*/
         "",        /*normal beskrivning 4*/
         "",        /*normal beskrivning 5*/
         "Cure for Plague.",
         "",
         "",        /*detaljerad beskrivning 3*/
         "",        /*detaljerad beskrivning 4*/
         "",        /*detaljerad beskrivning 5*/
         0,         /*”ka strength*/
         0,         /*”ka defence*/
         0);        /*strength demanded to use object*/

 Add_Object("Bottle",   /*name*/
         Drink,     /*typ*/
         50,        /*v„rde i gold*/
         0,         /*”ka/minska hps*/
         0,         /*”ka stamina*/
         0,         /*”ka agility*/
         0,         /*”ka charisma*/
         0,         /*”ka dexterity*/
         0,         /*”ka wisdom*/
         0,         /*”ka mana*/
         0,         /*”ka armor v„rde*/
         0,         /*”ka attack v„rde*/
         "",        /*„gd av?*/
         false,     /*bara EN i spelet*/
         Smallpox,  /*kurerar*/
         false,     /*finnas i shoppen*/
         true,      /*kunna hittas i dungeons?*/
         false,     /*cursed item?*/
         1,         /*min level att hittas i dngs*/
         99,        /*max level att hittas i dngs*/
         "Looks like water...",
         "",        /*normal beskrivning 2*/
         "",        /*normal beskrivning 3*/
         "",        /*normal beskrivning 4*/
         "",        /*normal beskrivning 5*/
         "Cure for Smallpox.",
         "",
         "",        /*detaljerad beskrivning 3*/
         "",        /*detaljerad beskrivning 4*/
         "",        /*detaljerad beskrivning 5*/
         0,         /*”ka strength*/
         0,         /*”ka defence*/
         0);        /*strength demanded to use object*/

 Add_Object("Bottle",   /*name*/
         Drink,     /*typ*/
         50,        /*v„rde i gold*/
         0,         /*”ka/minska hps*/
         0,         /*”ka stamina*/
         0,         /*”ka agility*/
         0,         /*”ka charisma*/
         0,         /*”ka dexterity*/
         0,         /*”ka wisdom*/
         0,         /*”ka mana*/
         0,         /*”ka armor v„rde*/
         0,         /*”ka attack v„rde*/
         "",        /*„gd av?*/
         false,     /*bara EN i spelet*/
         Measles,   /*kurerar*/
         false,     /*finnas i shoppen*/
         true,      /*kunna hittas i dungeons?*/
         false,     /*cursed item?*/
         1,         /*min level att hittas i dngs*/
         99,        /*max level att hittas i dngs*/
         "Its a round bottle with a tiny label stuck to the front.",
         "",        /*normal beskrivning 2*/
         "",        /*normal beskrivning 3*/
         "",        /*normal beskrivning 4*/
         "",        /*normal beskrivning 5*/
         "Cure for Measles.",
         "",
         "",        /*detaljerad beskrivning 3*/
         "",        /*detaljerad beskrivning 4*/
         "",        /*detaljerad beskrivning 5*/
         0,         /*”ka strength*/
         0,         /*”ka defence*/
         0);        /*strength demanded to use object*/

 Add_Object("Old Bottle",   /*name*/
         Drink,     /*typ*/
         50,        /*v„rde i gold*/
         0,         /*”ka/minska hps*/
         0,         /*”ka stamina*/
         0,         /*”ka agility*/
         0,         /*”ka charisma*/
         0,         /*”ka dexterity*/
         0,         /*”ka wisdom*/
         0,         /*”ka mana*/
         0,         /*”ka armor v„rde*/
         0,         /*”ka attack v„rde*/
         "",        /*„gd av?*/
         false,     /*bara EN i spelet*/
         Leprosy,   /*kurerar*/
         false,     /*finnas i shoppen*/
         true,      /*kunna hittas i dungeons?*/
         false,     /*cursed item?*/
         1,         /*min level att hittas i dngs*/
         99,        /*max level att hittas i dngs*/
         "Hmm... Could be wine, but it smells like rotten fish.",
         "",        /*normal beskrivning 2*/
         "",        /*normal beskrivning 3*/
         "",        /*normal beskrivning 4*/
         "",        /*normal beskrivning 5*/
         "Cure for Leprosy.",
         "",
         "",        /*detaljerad beskrivning 3*/
         "",        /*detaljerad beskrivning 4*/
         "",        /*detaljerad beskrivning 5*/
         0,         /*”ka strength*/
         0,         /*”ka defence*/
         0);        /*strength demanded to use object*/

 Add_Object("Dusty Bottle",   /*name*/
         Drink,     /*typ*/
         5,         /*v„rde i gold*/
         -25,       /*”ka/minska hps*/
         0,         /*”ka stamina*/
         0,         /*”ka agility*/
         0,         /*”ka charisma*/
         0,         /*”ka dexterity*/
         0,         /*”ka wisdom*/
         0,         /*”ka mana*/
         0,         /*”ka armor v„rde*/
         0,         /*”ka attack v„rde*/
         "",        /*„gd av?*/
         false,     /*bara EN i spelet*/
         Nothing,   /*kurerar*/
         false,     /*finnas i shoppen*/
         true,      /*kunna hittas i dungeons?*/
         false,     /*cursed item?*/
         1,         /*min level att hittas i dngs*/
         99,        /*max level att hittas i dngs*/
         "Its half full with a thick grey liquid.",
         "",        /*normal beskrivning 2*/
         "",        /*normal beskrivning 3*/
         "",        /*normal beskrivning 4*/
         "",        /*normal beskrivning 5*/
         "This is poison.",
         "",
         "",        /*detaljerad beskrivning 3*/
         "",        /*detaljerad beskrivning 4*/
         "",        /*detaljerad beskrivning 5*/
         0,         /*”ka strength*/
         0,         /*”ka defence*/
         0);        /*strength demanded to use object*/

 Add_Object("Blue Bottle",   /*name*/
         Drink,     /*typ*/
         15000,     /*v„rde i gold*/
         0,         /*”ka/minska hps*/
         0,         /*”ka stamina*/
         0,         /*”ka agility*/
         0,         /*”ka charisma*/
         0,         /*”ka dexterity*/
         0,         /*”ka wisdom*/
         1500,      /*”ka mana*/
         0,         /*”ka armor v„rde*/
         0,         /*”ka attack v„rde*/
         "",        /*„gd av?*/
         false,     /*bara EN i spelet*/
         Nothing,   /*kurerar*/
         true,     /*finnas i shoppen*/
         true,      /*kunna hittas i dungeons?*/
         false,     /*cursed item?*/
         15,         /*min level att hittas i dngs*/
         99,        /*max level att hittas i dngs*/
         "The bottle is only half-full with some liquid.",
         "",        /*normal beskrivning 2*/
         "",        /*normal beskrivning 3*/
         "",        /*normal beskrivning 4*/
         "",        /*normal beskrivning 5*/
         "This is a mana restorer.",
         "",
         "",        /*detaljerad beskrivning 3*/
         "",        /*detaljerad beskrivning 4*/
         "",        /*detaljerad beskrivning 5*/
         0,         /*”ka strength*/
         0,         /*”ka defence*/
         0);        /*strength demanded to use object*/


}
