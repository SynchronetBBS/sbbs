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

void Reset_Feets(void)
{
              /*typ av objekt :
                               1. head
                               2. body
                               3. arms
                               4. hands
                               5. fingers
                               6. legs
                               7. feet
                               8. waist
                               9. neck
                              10. face
                              11. shield
                              12. mat
                              13. dryck
                              14. vapen */

 Add_Object("Grass Boots",   /*name*/
            Feet,     /*typ*/
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
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            1,         /*min level att hittas i dngs*/
            20,        /*max level att hittas i dngs*/
            "Ordinary Grass Boots. Often used by poor peasants.",
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

 Add_Object("Wool Boots",   /*name*/
            Feet,     /*typ*/
            500,       /*v„rde i gold*/
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
            10,        /*min level att hittas i dngs*/
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
            0);        /*strength demanded to use object*/

 Add_Object("Rubber Boots",   /*name*/
            Feet,     /*typ*/
            4500,      /*v„rde i gold*/
            15,        /*”ka/minska hps*/
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
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            15,        /*min level att hittas i dngs*/
            25,        /*max level att hittas i dngs*/
            "Is it raining?",
            "",        /*normal beskrivning 2*/
            "",        /*normal beskrivning 3*/
            "",        /*normal beskrivning 4*/
            "",        /*normal beskrivning 5*/
            "",        /*detaljerad beskrivning 1*/
            "",        /*detaljerad beskrivning 2*/
            "",        /*detaljerad beskrivning 3*/
            "",        /*detaljerad beskrivning 4*/
            "",        /*detaljerad beskrivning 5*/
            2,         /*”ka strength*/
            2,         /*”ka defence*/
            5);        /*strength demanded to use object*/

  Add_Object("Slippers",   /*name*/
             Feet,     /*typ*/
             800,       /*v„rde i gold*/
             0,         /*”ka/minska hps*/
             0,         /*”ka stamina*/
             0,         /*”ka agility*/
             0,         /*”ka charisma*/
             0,         /*”ka dexterity*/
             0,         /*”ka wisdom*/
             45,        /*”ka mana*/
             3,         /*”ka armor v„rde*/
             0,         /*”ka attack v„rde*/
             "",        /*„gd av?*/
             false,     /*bara EN i spelet*/
             Nothing,   /*kurerar*/
             true,      /*finnas i shoppen*/
             false,     /*kunna hittas i dungeons?*/
             false,     /*cursed item?*/
             1,         /*min level att hittas i dngs*/
             99,        /*max level att hittas i dngs*/
             "A pair of ugly slippers. Indoor use only.",
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

  Add_Object("Ball and Chain",   /*name*/
             Feet,     /*typ*/
             5,         /*v„rde i gold*/
             -10,       /*”ka/minska hps*/
             -10,       /*”ka stamina*/
             -10,       /*”ka agility*/
             -10,       /*”ka charisma*/
             -10,       /*”ka dexterity*/
             -10,       /*”ka wisdom*/
             0,         /*”ka mana*/
             -5,        /*”ka armor v„rde*/
             -5,        /*”ka attack v„rde*/
             "",        /*„gd av?*/
             false,     /*bara EN i spelet*/
             Nothing,   /*kurerar*/
             false,     /*finnas i shoppen*/
             true,      /*kunna hittas i dungeons?*/
             true,      /*cursed item?*/
             22,        /*min level att hittas i dngs*/
             45,        /*max level att hittas i dngs*/
             "Old time Prison model.",
             "",        /*normal beskrivning 2*/
             "",        /*normal beskrivning 3*/
             "",        /*normal beskrivning 4*/
             "",        /*normal beskrivning 5*/
             "",        /*detaljerad beskrivning 1*/
             "",        /*detaljerad beskrivning 2*/
             "",        /*detaljerad beskrivning 3*/
             "",        /*detaljerad beskrivning 4*/
             "",        /*detaljerad beskrivning 5*/
             -5,        /*”ka strength*/
             -5,        /*”ka defence*/
             15);       /*strength demanded to use object*/

 Add_Object("Leather Boots",   /*name*/
            Feet,     /*typ*/
            1000,      /*v„rde i gold*/
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
            true,      /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            15,        /*min level att hittas i dngs*/
            40,        /*max level att hittas i dngs*/
            "The boots are made of black leather.",
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

 Add_Object("Reinforced Leather Boots",   /*name*/
            Feet,     /*typ*/
            2000,      /*v„rde i gold*/
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
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            5,         /*min level att hittas i dngs*/
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

 Add_Object("Iron Boots",   /*name*/
            Feet,     /*typ*/
            4500,      /*v„rde i gold*/
            -5,        /*”ka/minska hps*/
            -2,        /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            5,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            true,      /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            7,         /*min level att hittas i dngs*/
            35,        /*max level att hittas i dngs*/
            "Phew, these boots look very heavy.",
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

 Add_Object("Roller-Skates",   /*name*/
            Feet,     /*typ*/
            11000,     /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            25,        /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            0,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            true,      /*finnas i shoppen*/
            false,     /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            1,         /*min level att hittas i dngs*/
            99,        /*max level att hittas i dngs*/
            "A pair of black \"Speedy Rollers\". With these beauties",
            "you can make some speed out there.",
            "",        /*normal beskrivning 3*/
            "",        /*normal beskrivning 4*/
            "",        /*normal beskrivning 5*/
            "",        /*detaljerad beskrivning 1*/
            "",        /*detaljerad beskrivning 2*/
            "",        /*detaljerad beskrivning 3*/
            "",        /*detaljerad beskrivning 4*/
            "",        /*detaljerad beskrivning 5*/
            0,         /*”ka strength*/
            15,        /*”ka defence*/
            4);        /*strength demanded to use object*/

  Add_Object("Riding-Boots",   /*name*/
            Feet,     /*typ*/
            15000,     /*v„rde i gold*/
            5,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            -10,       /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            5,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            45,        /*min level att hittas i dngs*/
            50,        /*max level att hittas i dngs*/
            "You see nothing special.",
            "",
            "",        /*normal beskrivning 3*/
            "",        /*normal beskrivning 4*/
            "",        /*normal beskrivning 5*/
            "",        /*detaljerad beskrivning 1*/
            "",        /*detaljerad beskrivning 2*/
            "",        /*detaljerad beskrivning 3*/
            "",        /*detaljerad beskrivning 4*/
            "",        /*detaljerad beskrivning 5*/
            0,         /*”ka strength*/
            10,        /*”ka defence*/
            10);        /*strength demanded to use object*/

 Add_Object("Ivory Sandals",   /*name*/
            Feet,     /*typ*/
            15000,     /*v„rde i gold*/
            15,        /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            -9,        /*”ka charisma*/
            -5,        /*”ka dexterity*/
            0,         /*”ka wisdom*/
            10,        /*”ka mana*/
            2,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Plague,    /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            15,        /*min level att hittas i dngs*/
            70,        /*max level att hittas i dngs*/
            "Look very comfortable.",
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

 Add_Object("Black Skulls",   /*name*/
            Feet,     /*typ*/
            7000,      /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            15,        /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            1,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            true,      /*finnas i shoppen*/
            false,     /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            1,         /*min level att hittas i dngs*/
            99,        /*max level att hittas i dngs*/
            "A pair of orc skulls.",
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

 Add_Object("Elven Hunting Boots",   /*name*/
            Feet,     /*typ*/
            27000,     /*v„rde i gold*/
            20,        /*”ka/minska hps*/
            10,        /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            30,        /*”ka mana*/
            7,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Blindness, /*kurerar*/
            true,      /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            30,        /*min level att hittas i dngs*/
            40,        /*max level att hittas i dngs*/
            "The boots are shimmering with a strange light.",
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



}
