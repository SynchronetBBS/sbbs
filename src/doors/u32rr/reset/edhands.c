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

void Reset_Hands(void)
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

 Add_Object("Grass Gloves",   /*name*/
            Hands,     /*typ*/
            250,       /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            1,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            true,      /*finnas i shoppen*/
            true,     /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            1,         /*min level att hittas i dngs*/
            5,         /*max level att hittas i dngs*/
            "A pair of ordinary Grass Gloves.",
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

 Add_Object("Wool Gloves",   /*name*/
            Hands,     /*typ*/
            750,       /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            2,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            true,      /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            2,         /*min level att hittas i dngs*/
            10,        /*max level att hittas i dngs*/
            "A pair of ordinary Wool Gloves.",
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

 Add_Object("Leather Gloves",   /*name*/
            Hands,     /*typ*/
            1500,      /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            3,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            5,         /*min level att hittas i dngs*/
            15,        /*max level att hittas i dngs*/
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

 Add_Object("Navy Gloves",   /*name*/
            Hands,     /*typ*/
            2700,      /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
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
            "White & Heavy ; Navy Model Gloves.",
            "",        /*normal beskrivning 2*/
            "",        /*normal beskrivning 3*/
            "",        /*normal beskrivning 4*/
            "",        /*normal beskrivning 5*/
            "",        /*detaljerad beskrivning 1*/
            "",        /*detaljerad beskrivning 2*/
            "",        /*detaljerad beskrivning 3*/
            "",        /*detaljerad beskrivning 4*/
            "",        /*detaljerad beskrivning 5*/
            3,         /*”ka strength*/
            3,         /*”ka defence*/
            10);       /*strength demanded to use object*/

 Add_Object("Heavy Leather Gloves",   /*name*/
            Hands,     /*typ*/
            3500,      /*v„rde i gold*/
            5,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
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
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            10,        /*min level att hittas i dngs*/
            25,        /*max level att hittas i dngs*/
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

 Add_Object("Golden Gauntlets",   /*name*/
            Hands,     /*typ*/
            17000,     /*v„rde i gold*/
            15,        /*”ka/minska hps*/
            5,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            7,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            25,         /*min level att hittas i dngs*/
            30,        /*max level att hittas i dngs*/
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
            5);        /*strength demanded to use object*/

 Add_Object("Dwarven Battle Gauntlets",   /*name*/
            Hands,     /*typ*/
            27000,     /*v„rde i gold*/
            30,        /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            12,        /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            50,         /*min level att hittas i dngs*/
            60,        /*max level att hittas i dngs*/
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
            25);        /*strength demanded to use object*/


}
