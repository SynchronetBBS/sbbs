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

void Reset_ABody(void)
{
  Add_Object("White Robe",   /*name*/
            ABody,      /*typ*/
            500,       /*v„rde i gold*/
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
            "It looks like a very nice robe.",      /*normal beskrivning 1*/
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

  Add_Object("Brown Robe",   /*name*/
            ABody,     /*typ*/
            650,       /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            5,         /*”ka wisdom*/
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
            50,        /*max level att hittas i dngs*/
            "",
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

  Add_Object("Kimono",   /*name*/
             ABody,     /*typ*/
            1600,      /*v„rde i gold*/
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
             Smallpox,  /*kurerar*/
             true,      /*finnas i shoppen*/
             true,      /*kunna hittas i dungeons?*/
             false,     /*cursed item?*/
             15,        /*min level att hittas i dngs*/
             17,        /*max level att hittas i dngs*/
             "The Kimono is made in silk. It looks very comfortable.",
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
             15);        /*strength demanded to use object*/

  Add_Object("Toga",   /*name*/
            ABody,      /*typ*/
            2600,       /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            55,        /*”ka mana*/
            3,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Measles,   /*kurerar*/
            true,      /*finnas i shoppen*/
            false,     /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            1,         /*min level att hittas i dngs*/
            99,        /*max level att hittas i dngs*/
            "The toga is white.",
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

  Add_Object("Black Robe",   /*name*/
            ABody,     /*typ*/
            7500,      /*v„rde i gold*/
            0,         /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            15,        /*”ka mana*/
            3,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Nothing,   /*kurerar*/
            true,      /*finnas i shoppen*/
            false,     /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            1,         /*min level att hittas i dngs*/
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
            0,         /*”ka defence*/
            0);        /*strength demanded to use object*/

  Add_Object("Dusty Robe",   /*name*/
            ABody,      /*typ*/
            10500,     /*v„rde i gold*/
            10,        /*”ka/minska hps*/
            10,        /*”ka stamina*/
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
            1,         /*min level att hittas i dngs*/
            50,        /*max level att hittas i dngs*/
            "A dirty old robe.",        /*normal beskrivning 1*/
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

   Add_Object("Black Robe",   /*name*/
            ABody,      /*typ*/
            15000,     /*v„rde i gold*/
            -5,        /*”ka/minska hps*/
            -2,        /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            0,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            40,        /*”ka mana*/
            3,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Blindness, /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            false,     /*cursed item?*/
            10,        /*min level att hittas i dngs*/
            80,        /*max level att hittas i dngs*/
            "You see nothing special.",        /*normal beskrivning 1*/
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

   Add_Object("Shabby Robe",   /*name*/
            ABody,      /*typ*/
            15,        /*v„rde i gold*/
            -15,       /*”ka/minska hps*/
            0,         /*”ka stamina*/
            0,         /*”ka agility*/
            0,         /*”ka charisma*/
            5,         /*”ka dexterity*/
            0,         /*”ka wisdom*/
            5,         /*”ka mana*/
            0,         /*”ka armor v„rde*/
            0,         /*”ka attack v„rde*/
            "",        /*„gd av?*/
            false,     /*bara EN i spelet*/
            Blindness, /*kurerar*/
            false,     /*finnas i shoppen*/
            true,      /*kunna hittas i dungeons?*/
            true,      /*cursed item?*/
            1,         /*min level att hittas i dngs*/
            99,        /*max level att hittas i dngs*/
            "You see nothing special.",        /*normal beskrivning 1*/
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

   Add_Object("Green Robe",   /*name*/
            ABody,      /*typ*/
            25000,     /*v„rde i gold*/
            15,       /*”ka/minska hps*/
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
            77,        /*max level att hittas i dngs*/
            "You see nothing special.",        /*normal beskrivning 1*/
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

  Add_Object("Beggars Robe",   /*name*/
             ABody,      /*typ*/
             500,       /*v„rde i gold*/
             0,         /*”ka/minska hps*/
             0,         /*”ka stamina*/
             0,         /*”ka agility*/
             0,         /*”ka charisma*/
             0,         /*”ka dexterity*/
             0,         /*”ka wisdom*/
             35,        /*”ka mana*/
             3,         /*”ka armor v„rde*/
             0,         /*”ka attack v„rde*/
             "",        /*„gd av?*/
             false,     /*bara EN i spelet*/
             Plague,    /*kurerar*/
             false,     /*finnas i shoppen*/
             true,      /*kunna hittas i dungeons?*/
             false,     /*cursed item?*/
             5,         /*min level att hittas i dngs*/
             25,        /*max level att hittas i dngs*/
             "Standard cotton model used by beggars.",        /*normal beskrivning 1*/
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

   Add_Object("Judge-Gown",   /*name*/
              ABody,      /*typ*/
              17000,     /*v„rde i gold*/
              0,         /*”ka/minska hps*/
              0,         /*”ka stamina*/
              0,         /*”ka agility*/
              15,        /*”ka charisma*/
              0,         /*”ka dexterity*/
              0,         /*”ka wisdom*/
              0,         /*”ka mana*/
              8,         /*”ka armor v„rde*/
              0,         /*”ka attack v„rde*/
              "",        /*„gd av?*/
              false,     /*bara EN i spelet*/
              Nothing,   /*kurerar*/
              true,      /*finnas i shoppen*/
              false,     /*kunna hittas i dungeons?*/
              false,     /*cursed item?*/
              1,         /*min level att hittas i dngs*/
              99,        /*max level att hittas i dngs*/
              "The gown is black of course...",
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
              15);        /*strength demanded to use object*/

   Add_Object("Snakeskin Robe",   /*name*/
              ABody,      /*typ*/
              27000,     /*v„rde i gold*/
              15,        /*”ka/minska hps*/
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
              42,        /*min level att hittas i dngs*/
              47,        /*max level att hittas i dngs*/
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
              15);        /*strength demanded to use object*/

  Add_Object("Monks Robe",   /*name*/
             ABody,     /*typ*/
             20500,     /*v„rde i gold*/
             -3,        /*”ka/minska hps*/
             0,         /*”ka stamina*/
             0,         /*”ka agility*/
             0,         /*”ka charisma*/
             0,         /*”ka dexterity*/
             55,        /*”ka wisdom*/
             55,        /*”ka mana*/
             -3,        /*”ka armor v„rde*/
             -3,         /*”ka attack v„rde*/
             "",        /*„gd av?*/
             false,     /*bara EN i spelet*/
             Nothing,   /*kurerar*/
             false,     /*finnas i shoppen*/
             true,      /*kunna hittas i dungeons?*/
             false,     /*cursed item?*/
             60,        /*min level att hittas i dngs*/
             75,        /*max level att hittas i dngs*/
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

  Add_Object("Tiger Skin",   /*name*/
             ABody,     /*typ*/
             175000,    /*v„rde i gold*/
             0,         /*”ka/minska hps*/
             0,         /*”ka stamina*/
             0,         /*”ka agility*/
             0,         /*”ka charisma*/
             0,         /*”ka dexterity*/
             0,         /*”ka wisdom*/
             0,         /*”ka mana*/
             30,        /*”ka armor v„rde*/
             0,         /*”ka attack v„rde*/
             "",        /*„gd av?*/
             false,     /*bara EN i spelet*/
             Blindness, /*kurerar*/
             false,     /*finnas i shoppen*/
             true,      /*kunna hittas i dungeons?*/
             false,     /*cursed item?*/
             75,        /*min level att hittas i dngs*/
             80,        /*max level att hittas i dngs*/
             "Grrr...",
             "",        /*normal beskrivning 2*/
             "",        /*normal beskrivning 3*/
             "",        /*normal beskrivning 4*/
             "",        /*normal beskrivning 5*/
             "Jakob Dangarden wrote this nice game.",
             "Greetings from Sweden!",
             "This Tiger Skin is great!",
             "",        /*detaljerad beskrivning 4*/
             "",        /*detaljerad beskrivning 5*/
             25,         /*”ka strength*/
             25,         /*”ka defence*/
             65);        /*strength demanded to use object*/

  Add_Object("Ophranas",   /*name*/
             ABody,     /*typ*/
             36000,     /*v„rde i gold*/
             0,         /*”ka/minska hps*/
             0,         /*”ka stamina*/
             0,         /*”ka agility*/
             0,         /*”ka charisma*/
             0,         /*”ka dexterity*/
             0,         /*”ka wisdom*/
             0,         /*”ka mana*/
             15,        /*”ka armor v„rde*/
             5,         /*”ka attack v„rde*/
             "",        /*„gd av?*/
             false,     /*bara EN i spelet*/
             Nothing,   /*kurerar*/
             false,     /*finnas i shoppen*/
             true,      /*kunna hittas i dungeons?*/
             false,     /*cursed item?*/
             65,        /*min level att hittas i dngs*/
             70,        /*max level att hittas i dngs*/
             "It's glimmering of gold.",
             "",        /*normal beskrivning 2*/
             "",        /*normal beskrivning 3*/
             "",        /*normal beskrivning 4*/
             "",        /*normal beskrivning 5*/
             "",
             "",
             "",
             "",        /*detaljerad beskrivning 4*/
             "",        /*detaljerad beskrivning 5*/
             9,         /*”ka strength*/
             9,         /*”ka defence*/
             45);       /*strength demanded to use object*/


}
