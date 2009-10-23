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

void Reset_Body(void)
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
                              14. vapen
                              15. around body */

 Add_Object("Grass Coat",   /*name*/
            Body,      /*typ*/
            50,        /*v„rde i gold*/
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
            10,        /*max level att hittas i dngs*/
            "You see nothing special.",
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

 Add_Object("Wool Coat",   /*name*/
            Body,      /*typ*/
            300,       /*v„rde i gold*/
            1,         /*”ka/minska hps*/
            1,         /*”ka stamina*/
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
            6,         /*min level att hittas i dngs*/
            15,        /*max level att hittas i dngs*/
            "An ugly wool coat ...",
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

 Add_Object("Cloth",   /*name*/
            Body,      /*typ*/
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

 Add_Object("Leather Vest",   /*name*/
            Body,      /*typ*/
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

  Add_Object("Heavy Leather Vest",
             Body,      /*typ*/
             1300,      /*v„rde i gold*/
             5,         /*”ka/minska hps*/
             0,         /*”ka stamina*/
             0,         /*”ka agility*/
             0,         /*”ka charisma*/
             8,         /*”ka dexterity*/
             0,         /*”ka wisdom*/
             0,         /*”ka mana*/
             2,         /*”ka armor v„rde*/
             0,         /*”ka attack v„rde*/
             "",        /*„gd av?*/
             false,     /*bara EN i spelet*/
             Blindness, /*kurerar*/
             true,      /*finnas i shoppen*/
             true,      /*kunna hittas i dungeons?*/
             false,     /*cursed item?*/
             5,         /*min level att hittas i dngs*/
             7,         /*max level att hittas i dngs*/
             "Reinforced leather vest. Can stop arrows.",
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
             3);        /*strength demanded to use object*/

  Add_Object("Ma's Protection",
             Body,      /*typ*/
             1200,      /*v„rde i gold*/
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
             false,     /*kunna hittas i dungeons?*/
             false,     /*cursed item?*/
             1,         /*min level att hittas i dngs*/
             99,        /*max level att hittas i dngs*/
             "Enchanted coat of unknown origin. Will last a lifetime",
             "",        /*normal beskrivning 2*/
             "",        /*normal beskrivning 3*/
             "",        /*normal beskrivning 4*/
             "",        /*normal beskrivning 5*/
             "",        /*detaljerad beskrivning 1*/
             "",        /*detaljerad beskrivning 2*/
             "",        /*detaljerad beskrivning 3*/
             "",        /*detaljerad beskrivning 4*/
             "",        /*detaljerad beskrivning 5*/
             10,        /*”ka strength*/
             0,         /*”ka defence*/
             2);        /*strength demanded to use object*/

  Add_Object("Plate Mail",   /*name*/
            Body,      /*typ*/
            1500,      /*v„rde i gold*/
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

  Add_Object("Smears Plate",   /*name*/
            Body,      /*typ*/
            2500,      /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            5,         /*”ka stamina*/
            0,         /*”ka agility*/
            5,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            5,         /*”ka wisdom*/
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
            3,         /*max level att hittas i dngs*/
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

  Add_Object("Hedgehog Defence",   /*name*/
            Body,      /*typ*/
            5500,      /*v„rde i gold*/
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
            "Can sting opponents.",
            "",        /*normal beskrivning 2*/
            "",        /*normal beskrivning 3*/
            "",        /*normal beskrivning 4*/
            "",        /*normal beskrivning 5*/
            "",        /*detaljerad beskrivning 1*/
            "",        /*detaljerad beskrivning 2*/
            "",        /*detaljerad beskrivning 3*/
            "",        /*detaljerad beskrivning 4*/
            "",        /*detaljerad beskrivning 5*/
            5,         /*”ka strength*/
            5,         /*”ka defence*/
            3);        /*strength demanded to use object*/

  Add_Object("Asylum Coat",   /*name*/
             Body,      /*typ*/
             19800,     /*v„rde i gold*/
             35,        /*”ka/minska hps*/
             0,         /*”ka stamina*/
             0,         /*”ka agility*/
             0,         /*”ka charisma*/
             0,         /*”ka dexterity*/
             0,         /*”ka wisdom*/
             0,         /*”ka mana*/
             14,        /*”ka armor v„rde*/
             0,         /*”ka attack v„rde*/
             "",        /*„gd av?*/
             false,     /*bara EN i spelet*/
             Nothing,   /*kurerar*/
             true,      /*finnas i shoppen*/
             false,     /*kunna hittas i dungeons?*/
             false,     /*cursed item?*/
             1,         /*min level att hittas i dngs*/
             99,        /*max level att hittas i dngs*/
             "Nothing special.",
             "",        /*normal beskrivning 2*/
             "",        /*normal beskrivning 3*/
             "",        /*normal beskrivning 4*/
             "",        /*normal beskrivning 5*/
             "",        /*detaljerad beskrivning 1*/
             "",        /*detaljerad beskrivning 2*/
             "",        /*detaljerad beskrivning 3*/
             "",        /*detaljerad beskrivning 4*/
             "",        /*detaljerad beskrivning 5*/
             15,        /*”ka strength*/
             15,        /*”ka defence*/
             45);       /*strength demanded to use object*/

  Add_Object("Sable Coat",   /*name*/
            Body,      /*typ*/
            3700,      /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
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
            false,     /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            1,         /*min level att hittas i dngs*/
            99,        /*max level att hittas i dngs*/
            "A very elegant coat.",
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

  Add_Object("Razor shirt", /*name*/
            Body,      /*typ*/
            150,       /*v„rde i gold*/
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
            false,     /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            1,         /*min level att hittas i dngs*/
            10,        /*max level att hittas i dngs*/
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

  Add_Object("Heavy Leather Vest",   /*name*/
            Body,      /*typ*/
            3000,      /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
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

  Add_Object("Banded Mail",   /*name*/
            Body,      /*typ*/
            4500,      /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            6,         /*”ka armor v„rde*/
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

  Add_Object("SnakeSkin Jacket",   /*name*/
            Body,      /*typ*/
            10500,     /*v„rde i gold*/
            5,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            5,         /*”ka dexterity*/
            5,         /*”ka wisdom*/
            15,        /*”ka mana*/
            7,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Measles,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            25,        /*min level att hittas i dngs*/
            45,        /*max level att hittas i dngs*/
            "A typical Ranger Jacket. Popular among hunters.",
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

  Add_Object("Chain Shirt",   /*name*/
            Body,      /*typ*/
            22000,     /*v„rde i gold*/
            10,        /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            -10,       /*”ka mana*/
            8,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            15,        /*min level att hittas i dngs*/
            55,        /*max level att hittas i dngs*/
            "This shirt looks as if it could stop a few blows ...",
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

  Add_Object("Heavy Chain Shirt",   /*name*/
            Body,      /*typ*/
            45000,     /*v„rde i gold*/
            20,        /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            -20,       /*”ka mana*/
            10,        /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            35,        /*min level att hittas i dngs*/
            65,        /*max level att hittas i dngs*/
            "This shirt looks as if it could stop quite a few blows ...",
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

  Add_Object("Coat of Mail",   /*name*/
             Body,      /*typ*/
             250000,    /*v„rde i gold*/
             75,        /*”ka/minska hps*/
             20,        /*”ka stamina*/
             0,         /*”ka agility*/
             5,         /*”ka charisma*/
             0,         /*”ka dexterity*/
             0,         /*”ka wisdom*/
             0,         /*”ka mana*/
             25,        /*”ka armor v„rde*/
             0,         /*”ka attack v„rde*/
             "",        /*„gd av?*/
             false,     /*bara EN i spelet*/
             Nothing,   /*kurerar*/
             false,     /*finnas i shoppen*/
             true,      /*kunna hittas i dungeons?*/
             false,     /*cursed item?*/
             90,        /*min level att hittas i dngs*/
             95,        /*max level att hittas i dngs*/
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
             15,        /*”ka strength*/
             15,        /*”ka defence*/
             100);      /*strength demanded to use object*/

  Add_Object("Unfinished Breast Plate",   /*name*/
             Body,      /*typ*/
             30000,     /*v„rde i gold*/
             5,         /*”ka/minska hps*/
             5,         /*”ka stamina*/
             5,         /*”ka agility*/
             5,         /*”ka charisma*/
             0,         /*”ka dexterity*/
             5,         /*”ka wisdom*/
             5,         /*”ka mana*/
             9,         /*”ka armor v„rde*/
             0,         /*”ka attack v„rde*/
             "",        /*„gd av?*/
             false,     /*bara EN i spelet*/
             Nothing,   /*kurerar*/
             false,     /*finnas i shoppen*/
             true,      /*kunna hittas i dungeons?*/
             false,     /*cursed item?*/
             20,        /*min level att hittas i dngs*/
             25,        /*max level att hittas i dngs*/
             "The plate looks as if it could protect you well.",
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

   Add_Object("Air Elite",   /*name*/
              Body,      /*typ*/
              16000,     /*v„rde i gold*/
              0,         /*”ka/minska hps*/
              0,         /*”ka stamina*/
              0,         /*”ka agility*/
              25,        /*”ka charisma*/
              0,         /*”ka dexterity*/
              0,         /*”ka wisdom*/
              0,         /*”ka mana*/
              7,         /*”ka armor v„rde*/
              0,         /*”ka attack v„rde*/
              "",        /*„gd av?*/
              false,     /*bara EN i spelet*/
              Nothing,   /*kurerar*/
              true,      /*finnas i shoppen*/
              false,     /*kunna hittas i dungeons?*/
              false,     /*cursed item?*/
              1,         /*min level att hittas i dngs*/
              99,        /*max level att hittas i dngs*/
              "Defence of the unborn...",
              "",        /*normal beskrivning 2*/
              "",        /*normal beskrivning 3*/
              "",        /*normal beskrivning 4*/
              "",        /*normal beskrivning 5*/
              "",        /*detaljerad beskrivning 1*/
              "",        /*detaljerad beskrivning 2*/
              "",        /*detaljerad beskrivning 3*/
              "",        /*detaljerad beskrivning 4*/
              "",        /*detaljerad beskrivning 5*/
              15,        /*”ka strength*/
              15,        /*”ka defence*/
              5);        /*strength demanded to use object*/

  Add_Object("Breast Plate",   /*name*/
            Body,      /*typ*/
            40000,     /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            -15,       /*”ka mana*/
            13,        /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            50,        /*min level att hittas i dngs*/
            70,        /*max level att hittas i dngs*/
            "The standard \"Warrior Plate\" model.",
            "Heavy, but with very good absorbation capability.",
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

  Add_Object("Heavy Breast Plate",   /*name*/
            Body,      /*typ*/
            90000,     /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            -55,       /*”ka mana*/
            16,        /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            40,        /*min level att hittas i dngs*/
            99,        /*max level att hittas i dngs*/
            "The model above the \"Warrior Plate\" model.",
            "Very heavy, but with extreme absorbation capability.",
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

  Add_Object("Dwarven Body Armor",   /*name*/
            Body,      /*typ*/
            190000,    /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            35,        /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            50,        /*min level att hittas i dngs*/
            70,        /*max level att hittas i dngs*/
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
            0,         /*”ka defence*/
            0);        /*strength demanded to use object*/

  Add_Object("White Body Armor",   /*name*/
            Body,      /*typ*/
            50000,     /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            25,        /*”ka stamina*/
            25,        /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            0,         /*”ka mana*/
            15,        /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            75,        /*min level att hittas i dngs*/
            80,        /*max level att hittas i dngs*/
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
            0,         /*”ka defence*/
            0);        /*strength demanded to use object*/


    /* 4: armor_name:="Dusty Coat";
      5: armor_name:="Heavy Coat";
      7: armor_name:="Leather Jacket";
      8: armor_name:="Reinforced Leather Jacket";

 addrec2("Alfheims Protection",35000,18);
 addrec2("Angel Wings",40000,19);
 addrec2("Gods Helper",50000,20);
 addrec2("Elaines Kiss",70000,22);
 addrec2("King Arthurs Armor",80000,24);
 addrec2("Gawaines Angel",100000,26);
 addrec2("Carados Cross",110000,27);
 addrec2("Stones of Camelot",160000,32);
 addrec2("Nimues Rescue",190000,36);
 addrec2("Hand of Galahad",200000,37);
 addrec2("Reynauds Stones",240000,39);
 addrec2("Gareths Spell",260000,41);
 addrec2("Menelaos Protection",280000,43);
 addrec2("Gunnars Firewall",290000,44);
 addrec2("Grendels Sorrow",310000,46);
 addrec2("Hand of Brahma",320000,47);
 addrec2("Boaz Whisper",340000,49);
 addrec2("Arts Experience",360000,51);
 addrec2("Mauis Trick",400000,55);
 addrec2("Mati-Syras Prophecy",410000,56);
 addrec2("Mawus Boll",420000,57);
 addrec2("Hands of Midas",440000,59);
 addrec2("Forcefield of Mesak",450000,60);
 addrec2("Modrons Stallion",460000,61);
 addrec2("Morgan Le Fays Word",470000,62);
 addrec2("Munins Rescue",490000,64);
 addrec2("Morpheus Sleep",510000,66);
 addrec2("Morrigans Fear",520000,67);
 addrec2("Montezumas Healing",540000,69);
 addrec2("Montezumas Whisper",550000,70);
 addrec2("Zarathustras Faith",570000,72);
 addrec2("Zeus Love",590000,74);
 addrec2("Balders Protection",610000,76);
 addrec2("Balors Eye",620000,77);
 addrec2("Hand of Baal",630000,78);
 addrec2("Belis Freedom",640000,79);
 addrec2("Bedeveres Choice",650000,80);
 addrec2("Beelzebubs Spear",660000,81);
 addrec2("Bellefrons Stallion",670000,82);
 addrec2("Flight of Pegasus",680000,83);
 addrec2("Bladuds Flight",690000,84);
 addrec2("Blodeuedds Treason",700000,85);
 addrec2("Stallion of God",710000,86);
 addrec2("Branwens Heart",720000,87);
 addrec2("Monks Pray",730000,88);
 addrec2("Berzerks Protection",740000,89);
 addrec2("Buddhas Light",750000,90);
 addrec2("Spear of Bron",760000,91);
 addrec2("Buchius Bulldefence",770000,92);
 addrec2("Buris Birthskin",780000,93);
 addrec2("Darths Reflexion",800000,95);
 addrec2("Cloth of God",820000,97);
 addrec2("Ceridwens Potion",840000,99);
 addrec2("Pot of Protection",850000,100);
 addrec2("Cerunnos Horn",860000,101);
 addrec2("Sea of Power",880000,103);
 addrec2("Chimaeras Defence",890000,104);
 addrec2("Chi Songzis Water",900000,105);
 addrec2("Lamb of Protection",910000,106);
 addrec2("Deaths Deflector",940000,109);
 addrec2("Scipios Defence",950000,110);
 addrec2("Simsons Hair",970000,112);
 addrec2("Danus Demondefence",1000000,115);
 addrec2("Bastards Guard",1100000,116);
 addrec2("Paladins Dream",1200000,117);
 addrec2("Floating Mystery",1400000,119);
 addrec2("Pearl of Protection",1500000,120);
 addrec2("Spear of Healing",1600000,121);
 addrec2("Satans Armour",1700000,122);
 addrec2("Lions Defence",1800000,123);
 addrec2("Elronds Protection",1900000,124);
 addrec2("Delgeths Protection",2200000,127);
 addrec2("Diomedes Defence",2400000,129);
 addrec2("Gandalfs Steel",2500000,130);
 addrec2("Wizards Vest",2600000,131);
 addrec2("Whisper of Sauron",2800000,133);
 addrec2("Warlords Defence",2900000,134);
 addrec2("Coat of Whispers",3200000,137);
 addrec2("Dengs Vest",3300000,138);
 addrec2("Galtungs Vest",3400000,139);
 addrec2("Ballad of Freedom",3500000,140);
 addrec2("Merlins Icewall",3600000,141);
 addrec2("Merlins Stonewall",3700000,142);
 addrec2("Dragons Nightmare",3800000,143);
 addrec2("Merlins Steelwall",3900000,144);
 addrec2("Didos Resurrection",4000000,145);
 addrec2("Djabos Armor",4100000,146);
 addrec2("EAs Wand",4300000,148);
 addrec2("Ekkekkos Thought",4400000,149);
 addrec2("Chant of Elaine",4500000,150);
 addrec2("Elffins Salvation",4700000,152);
 addrec2("Sphere of Spectrum",4900000,154);
 addrec2("Flight of Mamba",5000000,155);
 addrec2("Wall of Stone",5200000,157);
 addrec2("Hair of Embla",5300000,158);
 addrec2("Endymions Sleep",5400000,159);
 addrec2("Wall of Diamonds",5500000,160);
 addrec2("Enkidus Health",5600000,161);
 addrec2("Air of Enlil",5700000,162);
 addrec2("Eshus Spirit",5900000,164);
 addrec2("Carpet of Steel",6000000,165);
 addrec2("Erectheus Power",6100000,166);
 addrec2("Esaus Mantel",6300000,168);
 addrec2("Guardian of Europa",6400000,169);
 addrec2("Light of healing",6500000,170);
 addrec2("Evas Defence",6600000,171);
 addrec2("Deflector of Hope",6900000,174);
 addrec2("Deflector of Fafne",7000000,175);
 addrec2("Feng Bos Wisdom",7100000,176);
 addrec2("Fountain of Force",7200000,177);
 addrec2("Dantes Defence",7300000,178);
 addrec2("Gollums Disguise",8100000,186);
 addrec2("Ceasars Protector",8500000,188);
 addrec2("Forcefield of Stone",8900000,189);
 addrec2("Forcefield of Glass",9400000,190);
 addrec2("Protector of Faith",13000000,193);
 addrec2("Demons Armour",15000000,194);
 addrec2("Queen Dragons Skin",17500000,195);
 addrec2("Troll Wizards Skin",22500000,196);
 addrec2("Elf Emperors  Skin",25000000,197);
 addrec2("Dwarf Lords Skin",27500000,198);
 addrec2("Nocturnes Rescue",29000000,199);
 addrec2("Moons Defence",30000000,200);
 addrec2("Hair of Gaia",50000000,202);
 addrec2("Armour of Gaia",60000000,203);
 addrec2("The Foregivness",70000000,204);
 addrec2("Wisdom of Baal",80000000,205);
 addrec2("Millenniums Will",90000000,206);
 addrec2("First Spell of God",100000000,207);
 addrec2("Freyas Seed",200000000,208);
 addrec2("Dust of Hope",400000000,210);
 addrec2("Supreme Protector",900000000,918); */

}
