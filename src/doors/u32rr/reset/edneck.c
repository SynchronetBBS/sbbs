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

void Reset_Neck(void)
{
              /*typ av objekt :
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
                              14. vapen */

 Add_Object("Green Necklace",   /*name*/
            Neck,      /*typ*/
            1500,      /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            25,        /*”ka mana*/
            0,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            true,      /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            1,         /*min level att hittas i dngs*/
            99,        /*max level att hittas i dngs*/
            "You see nothing special.",
            "",        /*normal beskrivning 2*/
            "",        /*normal beskrivning 3*/
            "",        /*normal beskrivning 4*/
            "",        /*normal beskrivning 5*/
            "",        /*detaljerad beskrivning 1*/
            "",        /*detaljerad beskrivning 2*/
            "",        /*detaljerad beskrivning 3*/
            "",        /*detaljerad beskrivning 4*/
            "",        /*detaljerad beskrivning 5*/
            0,         /*”ka strength*/
            0,         /*”ka defence*/
            0);        /*strength demanded to use object*/

 Add_Object("Madmans Necklace",   /*name*/
            Neck,      /*typ*/
            500,       /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            20,        /*”ka mana*/
            1,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            true,      /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            1,         /*min level att hittas i dngs*/
            99,        /*max level att hittas i dngs*/
            "",
            "",
            "",
            "",        /*normal beskrivning 4*/
            "",        /*normal beskrivning 5*/
            "",        /*detaljerad beskrivning 1*/
            "",        /*detaljerad beskrivning 2*/
            "",        /*detaljerad beskrivning 3*/
            "",        /*detaljerad beskrivning 4*/
            "",        /*detaljerad beskrivning 5*/
            0,         /*”ka strength*/
            0,         /*”ka defence*/
            0);        /*strength demanded to use object*/

 Add_Object("NeckTreck",
            Neck,      /*typ*/
            5,         /*v„rde i gold*/
            -2,        /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            -2,        /*”ka armor v„rde*/
            -2,        /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            true,      /*finnas i shoppen*/
            false,     /*kunna hittas i dungeons?*/
            true,      /*cursed item?*/
            1,         /*min level att hittas i dngs*/
            99,        /*max level att hittas i dngs*/
            "",
            "",
            "",
            "",        /*normal beskrivning 4*/
            "",        /*normal beskrivning 5*/
            "",        /*detaljerad beskrivning 1*/
            "",        /*detaljerad beskrivning 2*/
            "",        /*detaljerad beskrivning 3*/
            "",        /*detaljerad beskrivning 4*/
            "",        /*detaljerad beskrivning 5*/
            -2,         /*”ka strength*/
            -2,         /*”ka defence*/
            2);        /*strength demanded to use object*/

 Add_Object("Brass Necklace",   /*name*/
            Neck,      /*typ*/
            25,        /*v„rde i gold*/
            -5,        /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            -2,        /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            true,      /*cursed item?*/
            1,         /*min level att hittas i dngs*/
            99,        /*max level att hittas i dngs*/
            "You see nothing special.",
            "",        /*normal beskrivning 2*/
            "",        /*normal beskrivning 3*/
            "",        /*normal beskrivning 4*/
            "",        /*normal beskrivning 5*/
            "",        /*detaljerad beskrivning 1*/
            "",        /*detaljerad beskrivning 2*/
            "",        /*detaljerad beskrivning 3*/
            "",        /*detaljerad beskrivning 4*/
            "",        /*detaljerad beskrivning 5*/
            0,         /*”ka strength*/
            0,         /*”ka defence*/
            0);        /*strength demanded to use object*/

 Add_Object("Iron Necklace",   /*name*/
            Neck,      /*typ*/
            7500,      /*v„rde i gold*/
            -5,        /*”ka/minska hps*/
            -5,        /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            4,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            true,      /*finnas i shoppen*/
            false,     /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            1,         /*min level att hittas i dngs*/
            99,        /*max level att hittas i dngs*/
            "You see nothing special.",
            "",        /*normal beskrivning 2*/
            "",        /*normal beskrivning 3*/
            "",        /*normal beskrivning 4*/
            "",        /*normal beskrivning 5*/
            "",        /*detaljerad beskrivning 1*/
            "",        /*detaljerad beskrivning 2*/
            "",        /*detaljerad beskrivning 3*/
            "",        /*detaljerad beskrivning 4*/
            "",        /*detaljerad beskrivning 5*/
            0,         /*”ka strength*/
            0,         /*”ka defence*/
            0);        /*strength demanded to use object*/


 Add_Object("Ivory Necklace",   /*name*/
            Neck,      /*typ*/
            55000,     /*v„rde i gold*/
            25,        /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            8,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            30,        /*min level att hittas i dngs*/
            99,        /*max level att hittas i dngs*/
            "You see nothing special.",
            "",        /*normal beskrivning 2*/
            "",        /*normal beskrivning 3*/
            "",        /*normal beskrivning 4*/
            "",        /*normal beskrivning 5*/
            "",        /*detaljerad beskrivning 1*/
            "",        /*detaljerad beskrivning 2*/
            "",        /*detaljerad beskrivning 3*/
            "",        /*detaljerad beskrivning 4*/
            "",        /*detaljerad beskrivning 5*/
            0,         /*”ka strength*/
            0,         /*”ka defence*/
            0);        /*strength demanded to use object*/

 Add_Object("Crab Claw Necklace",   /*name*/
            Neck,      /*typ*/
            15000,     /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            13,        /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            40,        /*min level att hittas i dngs*/
            55,        /*max level att hittas i dngs*/
            "You see nothing special.",
            "",        /*normal beskrivning 2*/
            "",        /*normal beskrivning 3*/
            "",        /*normal beskrivning 4*/
            "",        /*normal beskrivning 5*/
            "",        /*detaljerad beskrivning 1*/
            "",        /*detaljerad beskrivning 2*/
            "",        /*detaljerad beskrivning 3*/
            "",        /*detaljerad beskrivning 4*/
            "",        /*detaljerad beskrivning 5*/
            0,         /*”ka strength*/
            0,         /*”ka defence*/
            0);        /*strength demanded to use object*/

    /*  1: armor_name:="Red Necklace";
      2: armor_name:="Yellow Necklace";
      3: armor_name:="Black Necklace";
      4: armor_name:="Green Necklace";
      6: armor_name:="Steel Necklace";
      8: armor_name:="Silver Necklace";
      9: armor_name:="Gold Necklace";
      10: armor_name:="Brown Necklace";
      11: armor_name:="White Necklace";
      12: armor_name:="Heros Necklace";
      13: armor_name:="Tiger Tooth Necklace";
      */

}
