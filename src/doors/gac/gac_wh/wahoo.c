#include "gac_wh.h"
/* WaHoo

	This game uses the GAME SDK in gac_cs\gamesdk

  Play a game...
	- Setup the board
	- restore the positions if saved...

	- loop to play the game
	{
	- Allow the player to roll and pick who to move
	- move the player's piece
		- allow them to place a piece on the board if a 1 or 6.
		- check if they are going to jump their own man (not alloweD)
		- check if they land on another player  (+5 points)
		- check if they make it into a home cell
		- check if they won or lost
	- move all computer opponents (identical to above, but try to land on
	  pieces more than anything other than going home)
	}
	
	- Score 5 points/knock off and 100 points for a win.


	WAHOTITL
	WAHOINST

*/



void WaHoo( void )
{
	char gameOver=FALSE, response;

   #ifdef GAC_DEBUG
   gac_debug = myopen(gac_debugfile, "a", SH_DENYRW);
   fprintf(gac_debug, "  WaHoo()\n");
   fclose(gac_debug);
   #endif

  g_clr_scr();
  g_send_file("wahotitl");
  if (WaHooInit() == TRUE) return;   // Check if user chose quit
  
  InitWaHooBoard();   // Initialize the x and y positions of the board

//PromptBox("After InitWaHooBoard", "", "ANY", TRUE);

  response = 'Y';
  while (response != 'N')
  {              
	// see if user can play some more today...    
	if (player.gamesToday > maxGames && maxGames != -1)
	{
		PromptBox("Sorry, you can not play again today.", "Come back tomorrow.  Press any key to continue.", "ANY", FALSE);
		return;
	}
	else
		player.gamesToday++;

	if (player.savedGame == TRUE)
	{
		if (PromptBox("Do you wish to continue your saved game (`bright cyan`Y`cyan`/`bright cyan`n`cyan`)?","","YN\n\r", FALSE) == 'N')
			player.savedGame = FALSE;
	}
	g_clr_scr();
	SetupWaHooBoard();   // start the game out
//PromptBox("After SetupWaHooBoard", "", "ANY", TRUE);
	
	// let the user play the game
	gameOver = PlayWaHoo();
//PromptBox("After PlayWaHoo", "", "ANY", TRUE);

	if( gameOver == WON)
	{
		sprintf(prompt, "Congratulations!  You won the game.");
		player.gamesWon++;
		player.money += 200;
	}
	else if (gameOver == LOST)
	{
		sprintf(prompt, "To bad, you lost that game.");
		if (player.money >= 100)
			player.money -= 100;
	}

	if (gameOver == SAVED)
	{
		player.savedGame = TRUE;
		PromptBox("Come back when you want to continue.", "Press any key", "ANY", FALSE);
		response = 'N';
	}
	else
	{
		player.savedGame = FALSE;
		response = PromptBox(prompt, "Do you wish to play another game (`bright cyan`Y`cyan`/`bright cyan`n`cyan`)?", "YN\n\r", FALSE);
	}

	// need to write the players information...
//    WritePlayerInfo(&player, player.account);
  }
	return;
}


// This function allows the user to play the game...
// returns LOST, WON or SAVED
/*
	- role for the player
	- let user choose which piece to move.
		- if a 1 or 6 and they have pieces at start let one out
	- check if we jump our player (not allowed)
	- check if we land on a player (+5 points)
	- check if the game is won
	- let user go again if it was a six...

	- do computer players...


*/
INT16 PlayWaHoo( void )
{
	char gameOver=FALSE, finished=FALSE, moved=FALSE;
	char curColor, i,y;
	INT16 dieroll, dieX=0, dieY=0;
	char choice;

	while(gameOver==FALSE)
	{
	  for (curColor = 0; curColor< MAX_PLAYERS; curColor++)  
	  {
   // process the user's moves...
		if (curColor == player.playerColor)
		{
			finished = FALSE;
			while (finished == FALSE)
			{
			   moved = FALSE;
			// role for the user
			dieroll = g_rand(5) + 1;
			  // print the die roll 
			  //od_set_cursor(2,1);
			  //od_printf("`bright white on blue`Die Roll:`bright red` %7d ``", dieroll);
            if (dieX != 0)
               DeleteDieRoll( dieX, dieY);
            dieX = 2+g_rand(12);
            dieY = 3+g_rand(12);
            SmallDieRoll( dieroll, dieX, dieY);

   			od_set_cursor(1,1);
				od_printf("`bright white on blue`Score   :`bright red` %7ld ``", player.money);

				// if die roll is a 1 or six, let user pull a start piece out
				if (dieroll == 1 || dieroll == 6)
				{
					for (i=0;i<MAX_PIECES;i++)
					{
						if (player.locations[player.playerColor][i] == EMPTY)
						{
							choice = PromptBox("Do you want to take a piece out of the start (`bright cyan`Y`cyan`/`bright cyan`n`cyan`)?", "", "YN\n\r", TRUE);
							if (choice != 'N')
							{
								moved = MoveWaHoo(player.playerColor,i, START);
							}
							i = MAX_PIECES; // quit early
						}
					}
				}           

			// check to see if the player can make a move or not.
			// see if we took one out of start
			if (moved == TRUE)
			{
				if (dieroll != 6) 
					finished = TRUE;             
			   continue;
			}
			else 
			   moved = TRUE;
				for (i=0;i<MAX_PIECES;i++)
				{
					if (player.locations[curColor][i] != EMPTY)
					{
						moved = FALSE;
					}
				}
			// tell user they can not move
			if (moved == TRUE)
         {
			   choice = PromptBox("You need a 1 or 6 to take a piece out of start.","Press any key to continue or `bright cyan`q`cyan`)uit.",
               "ANY",TRUE);
				if (choice == 'Q')
				{
					choice = PromptBox("`bright cyan`G`cyan`)o back to the game, `bright cyan`s`cyan`)ave or `bright cyan`f`cyan`)orfiet?", "", 
                  "SGF\n\r", TRUE);
					if (choice == 'F')
					{
						gameOver = LOST;
						finished = TRUE;
					}
					else if (choice == 'S')
					{
						gameOver = SAVED;
						finished = TRUE;
					}
				}
	     }


				// if they did not pull one out of start, let them move one (if
				// they have any on the board)
				while (moved == FALSE)
				{
					choice = PromptBox("Move which piece (`bright cyan`1`cyan`-`bright cyan`4`cyan`), `bright cyan`s`cyan`)kip this turn,"
						"`bright cyan`r`cyan`)edraw the screen", "or `bright cyan`q`cyan`)uit)?", "1234SRQ", TRUE);

					if (choice == 'Q')
					{
						choice = PromptBox("`bright cyan`G`cyan`)o back to the game, `bright cyan`s`cyan`)ave or `bright cyan`f`cyan`)orfiet?", "", "SGF\n\r", TRUE);
						if (choice == 'F')
						{
							gameOver = LOST;
							moved=TRUE;
							finished = TRUE;
						}
						else if (choice == 'S')
						{
							gameOver = SAVED;
							moved=TRUE;
							finished = TRUE;
						}
					}
					else if (choice == 'S')
						moved = TRUE;
			   else if (choice == 'R')
			   {
				  SetupWaHooBoard();
					od_set_cursor(1,1);
					od_printf("`bright white on blue`Score   :`bright red` %7ld ``", player.money);
					//od_set_cursor(2,1);
					//od_printf("`bright white on blue`Die Roll:`bright red` %7d ``", dieroll);
               SmallDieRoll( dieroll, dieX, dieY);
				}
					else
						moved = MoveWaHoo(player.playerColor,choice-'1', dieroll);
				}

				if (dieroll != 6) 
					finished = TRUE;
			
			}
		}        
		// process each of the computer opponents...    
		else
		{
// get the player moving around the board first
// continue;
		 od_sleep(500);
			finished = FALSE;
			while (finished == FALSE)
			{
			   moved = FALSE;
	   		// role for the user
   			dieroll = g_rand(5) + 1;
		   	// print the die roll 
			   //od_set_cursor(2,1);
			   //od_printf("`bright white on blue`Die Roll:`bright red` %7d ``", dieroll);
            if (dieX != 0)
               DeleteDieRoll( dieX, dieY);
            dieX = 2+g_rand(12);
            dieY = 3+g_rand(12);
            SmallDieRoll( dieroll, dieX, dieY);

					// if die roll is a 1 or six, let the computer pull a piece out
				if (dieroll == 1 || dieroll == 6)
				{
					for (i=0;i<MAX_PIECES;i++)
					{
						if (player.locations[curColor][i] == EMPTY)
						{
							moved = MoveWaHoo(curColor,i, START);
							i = MAX_PIECES; // quit early
						}
					}
				}           
			// see if we took one out of start
			if (moved == TRUE)
			{
				if (dieroll != 6) 
					finished = TRUE;             
			   continue;
			}
				// Check if the computer player can move
			else
				moved = TRUE;

				for (i=0;i<MAX_PIECES;i++)
				{
					if (player.locations[curColor][i] != EMPTY)
					{
						moved = FALSE;
					}
				}


				// if they did not pull one out of start, let them move one (if
				// they have any on the board)
				// don't loop for the computer
			//while (moved == FALSE)
				//{
			if (moved == FALSE)
			{
					// give priority to landing on someone...
					for (choice = 0; choice < MAX_PIECES; choice++)
					{
						for (y=0;y<MAX_PLAYERS; y++)
						{
							for (i=0;i<MAX_PIECES;i++)
							{
								// only check other players and on the board (not in "home")
								if (player.locations[y][i] == 
									player.locations[curColor][choice] + dieroll && y != curColor
									&& player.locations[y][i] < TOTAL_SPACES)
								{
									moved = MoveWaHoo(curColor,choice, dieroll);
						   // exit early
						   y = MAX_PLAYERS;
						   i = MAX_PIECES;
						   choice = MAX_PIECES;
								}
							}
						}
					}
					
					// if we could not get someone, then just move any
					// piece have four different AI's for playing
					if (moved == FALSE)
					{
   				  if (g_rand(3) < 2)
	   			  {
		   			 // try to move one randomly 
                   choice = g_rand(3);
			   		 moved = MoveWaHoo(curColor,choice, dieroll);
				     }
					 
   				  // default method
	   			  if (moved == FALSE)
		   		  {
						   for (y = 0; y < MAX_PIECES; y++)
   						{
					         if (y == choice) continue;		
                        moved = MoveWaHoo(curColor,y, dieroll);
		   					if (moved == TRUE)
			   					y = MAX_PIECES; // quit early
				   		}
			   	  }
					}
				}

				if (dieroll != 6) 
					finished = TRUE;
			}
		}
	  
		
		// see if anyone won the game by counting up the number of pieces
		// in the home...if they are all there...there was a winner.
		for (y=0;y<MAX_PLAYERS; y++)
		{
			choice = 0;
			for (i=0;i<MAX_PIECES;i++)
			{
				// check for players in home
				if (player.locations[y][i] >= HOME_1 && player.locations[y][i] <= HOME_1+3) 
					choice++;
			}
			
			if (choice >= MAX_PIECES)
			{
				// we have a winner!!!
				if (y == player.playerColor)
					gameOver = WON;
				else
					gameOver = LOST;
				y = MAX_PLAYERS; // exit loop early
			}
		}
		
		
		// check to see if the game was won, if it was...make the for exit
		if (gameOver != FALSE)
			curColor = MAX_PLAYERS+1;
	  
	  }  // end of for
	} // end of while
	return(gameOver);
}

/////////////////////  these are done

/* This function will attempt to move the piece around the board if it can
   be done.  If it can not be done, it will tell the user why and return 
   FALSE

   if player.playerColor is equal to the playerNumber, we will prompt
   the user.
*/
INT16 MoveWaHoo( char playerNumber, char pieceNumber, INT16 dieroll)
{
	char start,home; // where the start location is...
	char i,y,step; // loop counters
	char oldLocation;
   char choice;

	start = START_1 + DISTANCE*playerNumber;
	home = ENTER_HOME_1 + DISTANCE*playerNumber;

	// check if we can pull a player out...and then do it...
	if (dieroll == START)
	{
		// if we already have a piece in the start position, we can not pull
		// one out.  If someone else is there, we can nail them and gain
		// 25 points!!!
		for (i=0;i<MAX_PLAYERS;i++)
		{
			for (y=0;y<MAX_PIECES;y++)
			{
				// don't allow them to move onto themself
				if (player.locations[i][y] == start && i == playerNumber)
				{
					if (player.playerColor == playerNumber)
						PromptBox("You can not move a piece onto your other piece!", 
							"Press any key to continue", "ANY", TRUE);
					return(FALSE);
				}
				// the player got someone
				else if (player.locations[i][y] == start)
				{
					if (player.playerColor == playerNumber)
					{
						PromptBox("EXCELLENT!  You landed on another piece and gained 25 points.",
							"Press any key to continue", "ANY", TRUE);
						player.money+=25;
					od_set_cursor(1,1);
					od_printf("`bright white on blue`Score   :`bright red` %7ld ``", player.money);

					}
					else if (player.playerColor == i)
					{
						PromptBox("BUMMER!  You were sent back to start.",
							"Press any key to continue", "ANY", TRUE);
						if (player.money >= 12)
							player.money -= 12;
					}
					// delete old player
					DeleteWaHoo(i, y);
					// move old player back to start
					player.locations[i][y] = EMPTY;
					// show old player
					ShowWaHoo(i,y);
				}
			}
		}
		// move the piece out to the start
		// delete top player
		DeleteWaHoo(playerNumber, pieceNumber);
		// move top player to start
		player.locations[playerNumber][pieceNumber] = start;
		ShowWaHoo(playerNumber, pieceNumber);

		return(TRUE);
	}
	else if (player.locations[playerNumber][pieceNumber] == EMPTY)
	{
		if (player.playerColor == playerNumber)
			PromptBox("You can not move a piece out of start unless you roll a 1 or 6!",
				"Press any key to continue", "ANY", TRUE);

	  return(FALSE);
	}
	else if (player.locations[playerNumber][pieceNumber] == CENTER && dieroll != 1)
	{
		if (player.playerColor == playerNumber)
			PromptBox("You can not move a piece out of the center unless you roll a 1!",
				"Press any key to continue", "ANY", TRUE);

	  return(FALSE);
	}


	/*
		record the old position in case we land on ourself or try to go past
		home
	*/
	oldLocation = player.locations[playerNumber][pieceNumber];


	// see if the player is moving out of the center or possibly to the center
	if (dieroll == 1 )
	{
		if (player.locations[playerNumber][pieceNumber] == CENTER)
		{
		if (player.playerColor == playerNumber)
			choice = PromptBox("Move to which corner (`bright cyan`A`cyan`-`bright cyan`D`cyan`)?", "", "ABCD", TRUE);
		 else
		 {
			// computer logic
			// computer chooses to stay in the center, unless it can enter
			// the corner closest to its home
			for (y=0;y<MAX_PIECES;y++)
				if (player.locations[playerNumber][y] == CORNER_1+DISTANCE*playerNumber)
				  return(FALSE);

			switch(playerNumber)
			{
				case 0:
				  choice = 'D';
				  break;
				case 1:
				  choice = 'A';
				  break;
				case 2:
				  choice = 'B';
				  break;
				case 3:
				  choice = 'C';
				  break;
			 }
		 }
		 // delete the old position
		DeleteWaHoo(playerNumber, pieceNumber);

		 // move the piece
			player.locations[playerNumber][pieceNumber] = CORNER_1+DISTANCE*(choice-'A');

		// check if we are going to a space occupied by one of our tokens...
		 // this can not happen
		for (i=0;i<MAX_PIECES;i++)
		{
			if (player.locations[playerNumber][i] ==
				player.locations[playerNumber][pieceNumber] && pieceNumber != i)
			{
				if (playerNumber == player.playerColor)
					PromptBox("You can not land on one of your own pieces!",
						"Press any key to continue.", "ANY", TRUE);
				player.locations[playerNumber][pieceNumber] = oldLocation;

				// show the player at the new position
					ShowWaHoo(playerNumber, pieceNumber);

				return(FALSE);
			}
			}

		// show the player at the new position
		ShowWaHoo(playerNumber, pieceNumber);

			// check if we landed on someone
		for (y=0;y<MAX_PLAYERS; y++)
			{
				for (i=0;i<MAX_PIECES;i++)
				{
					// only check other players and on the board (not in "home")
					if (player.locations[y][i] ==
						player.locations[playerNumber][pieceNumber] && y != playerNumber
						&& player.locations[y][i] < TOTAL_SPACES)
					{
						if (playerNumber == player.playerColor)
						{
							PromptBox("EXCELLENT! You landed on a piece and gained 25 points.",
								"Press any key to continue.", "ANY", TRUE);
							player.money+=25;
					od_set_cursor(1,1);
					od_printf("`bright white on blue`Score   :`bright red` %7ld ``", player.money);


						}
					else if (player.playerColor == y)
					{
						PromptBox("BUMMER!  You were sent back to start.",
							"Press any key to continue", "ANY", TRUE);
						}

						if (player.money >= 12)
							player.money -= 12;
						// move the other piece back to start
						player.locations[y][i] = EMPTY;
						// show the squashed player at the new position
						ShowWaHoo(y,i);
					}
				}
			}

		 return(TRUE);
	  }
		else if (player.locations[playerNumber][pieceNumber] == CORNER_1 ||
		 player.locations[playerNumber][pieceNumber] == CORNER_1+DISTANCE ||
		 player.locations[playerNumber][pieceNumber] == CORNER_1+DISTANCE*2 ||
		 player.locations[playerNumber][pieceNumber] == CORNER_1+DISTANCE*3
		 )
		{
		 // see if they want to move to the center, otherwise move normally
			if (player.playerColor == playerNumber)
				choice = PromptBox("Do you want to move your piece to the center (`bright cyan`Y`cyan`/`bright cyan`n`cyan`)?", "", "YN\n\r", TRUE);
		 else
		 {
			// computer logic only go in if we are not at the last corner
			choice = 'N';
			switch(playerNumber)
			{
				case 0:
				  if (player.locations[playerNumber][pieceNumber] != CORNER_1+DISTANCE*3)
					 choice = 'Y';
				  break;
				case 1:
				  if (player.locations[playerNumber][pieceNumber] != CORNER_1+DISTANCE*0)
					 choice = 'Y';
				  break;

				case 2:
				  if (player.locations[playerNumber][pieceNumber] != CORNER_1+DISTANCE*1)
					 choice = 'Y';
				  break;

				case 3:
				  if (player.locations[playerNumber][pieceNumber] != CORNER_1+DISTANCE*2)
					 choice = 'Y';
				  break;

			}
		 }
		 if (choice != 'N')
		 {
			// move the piece (check for squashing)
			// delete the old position
			DeleteWaHoo(playerNumber, pieceNumber);

			// move the piece
				player.locations[playerNumber][pieceNumber] = CENTER;

			// check if we are going to a space occupied by one of our tokens...
			// this can not happen
		for (i=0;i<MAX_PIECES;i++)
			{
				if (player.locations[playerNumber][i] ==
					player.locations[playerNumber][pieceNumber] && pieceNumber != i)
				{
					if (playerNumber == player.playerColor)
					PromptBox("You can not land on one of your own pieces!",
						"Press any key to continue.", "ANY", TRUE);
				player.locations[playerNumber][pieceNumber] = oldLocation;

					// show the player at the new position
					ShowWaHoo(playerNumber, pieceNumber);

					return(FALSE);
				}
			  }

			// show the player at the new position
			ShowWaHoo(playerNumber, pieceNumber);

			  // check if we landed on someone
			for (y=0;y<MAX_PLAYERS; y++)
			{
				for (i=0;i<MAX_PIECES;i++)
				{
					// only check other players and on the board (not in "home")
						if (player.locations[y][i] ==
							player.locations[playerNumber][pieceNumber] && y != playerNumber )

// 4/97 CENTER is larger than TOTAL_SPACES
//						&& player.locations[y][i] < TOTAL_SPACES)
					{
						if (playerNumber == player.playerColor)
						{
							PromptBox("EXCELLENT! You landed on a piece and gained 25 points.",
								"Press any key to continue.", "ANY", TRUE);
							player.money+=25;
						od_set_cursor(1,1);
							od_printf("`bright white on blue`Score   :`bright red` %7ld ``", player.money);


						}
						else if (player.playerColor == y)
						{
							if (player.money >= 12)
								player.money -= 12;
							PromptBox("BUMMER!  You were sent back to start.",
								  "Press any key to continue", "ANY", TRUE);
						}

						// move the other piece back to start
						player.locations[y][i] = EMPTY;
						// show the squashed player at the new position
						ShowWaHoo(y,i);
						}
				}
			}

			return(TRUE);
		 }
		}
	}

	/*
		move the player normally!

		move the piece one step at a time and check for special things like
		jumping ourself, going into home, or landing on another player (if step
		equals dieroll)
	*/
	for (step=1;step<=dieroll;step++)
	{
		// pause slightly
		od_sleep(300);
		// delete the piece from the screen
		DeleteWaHoo(playerNumber, pieceNumber);

		// increment this piece by one...
		player.locations[playerNumber][pieceNumber] += 1;

		// check if we are at TOTAL_SPACES, if so set to 0 so we can go around
		// the board.
		if (player.locations[playerNumber][pieceNumber] == TOTAL_SPACES)
			player.locations[playerNumber][pieceNumber] = 0;

		// check if we should enter our home
		if (player.locations[playerNumber][pieceNumber] == home+1)
		{
			// since we are one past home, put the piece in home...
			player.locations[playerNumber][pieceNumber] = HOME_1;
			player.money += 10;
		}

		// check if we tried to walk out the end of our home...// 1/97 home+1 to HOME_1
		if (player.locations[playerNumber][pieceNumber] > HOME_1 + 3)
		{
			// since we walked out the end, reset the piece and return
			player.locations[playerNumber][pieceNumber] = oldLocation;
			// show the player at the new position
			ShowWaHoo(playerNumber, pieceNumber);
			if (playerNumber == player.playerColor)
			{
				PromptBox("You must have an exact number to enter home.", 
					"Press any key to continue.", "ANY", TRUE);
			}
			
			return(FALSE);

		}
		
		// check if we are on a space occupied by one of our tokens...this
		// can not happen
		for (i=0;i<MAX_PIECES;i++)
		{
			if (player.locations[playerNumber][i] == 
				player.locations[playerNumber][pieceNumber] && pieceNumber != i)
			{
				if (playerNumber == player.playerColor)
					PromptBox("You can not pass one of your own pieces!",
						"Press any key to continue.", "ANY", TRUE);
				player.locations[playerNumber][pieceNumber] = oldLocation;

				// show the player at the new position
				ShowWaHoo(playerNumber, pieceNumber);
				
				return(FALSE);
			}
		}
		

		// show the player at the new position
		ShowWaHoo(playerNumber, pieceNumber);

		// check if we are at the end of our roll and landed on someone
		if (step == dieroll)
		{
			for (y=0;y<MAX_PLAYERS; y++)
			{
				for (i=0;i<MAX_PIECES;i++)
				{
					// only check other players and on the board (not in "home")
					if (player.locations[y][i] ==
						player.locations[playerNumber][pieceNumber] && y != playerNumber
						&& player.locations[y][i] < TOTAL_SPACES)
					{
						if (playerNumber == player.playerColor)
						{
							PromptBox("EXCELLENT! You landed on a piece and gained 25 points.",
								"Press any key to continue.", "ANY", TRUE);
							player.money+=25;
							od_set_cursor(1,1);
							od_printf("`bright white on blue`Score   :`bright red` %7ld ``", player.money);
						}
						else if (player.playerColor == y)
						{
							PromptBox("BUMMER!  You were sent back to start.",
								"Press any key to continue", "ANY", TRUE);
							if (player.money >= 12)
								player.money -= 12;
						}

						// move the other piece back to start
						player.locations[y][i] = EMPTY;
						// show the squashed player at the new position
						ShowWaHoo(y,i);
					}
				}
			}
		}

	}


	// redraw all of the pieces in case they were erased by our walking...
/*
	for (y=0;y<MAX_PLAYERS; y++)
	{
		for (i=0;i<MAX_PIECES;i++)
		{
			ShowWaHoo(y,i);
		}
	}
*/

	return(TRUE);
}


/* 
   This function will remove the playing piece from the screen and redraw
   the piece under it (if necessary)

   // 1/97 looks correct now
*/
void DeleteWaHoo( char playerNumber, char pieceNumber)
{
   char i, y;
		
   // set colors to be used in home or start
		// set the correct color and display that player
			switch (playerNumber)
			{
				case 0:
				  od_printf("`bright magenta`");
					break;
				 case 1:
					od_printf("`bright green`");
				  break;
				 case 2:
					od_printf("`bright red`");
				  break;
				case 3:
					 od_printf("`bright yellow`");
				  break;
			}

	////// set the cursor to the correct position
	// player is at start
	if (player.locations[playerNumber][pieceNumber] == EMPTY)
	{
		// check which player it is and assign the X and Y to that cell
		od_set_cursor(startLocations[playerNumber][pieceNumber].y, 
					  startLocations[playerNumber][pieceNumber].x); 
	  od_printf("O");
	  return;
	}
   // player is in center
	else if (player.locations[playerNumber][pieceNumber] == CENTER)
	{
		// check which player it is and assign the X and Y to that cell
		od_set_cursor(centerLocation.y, centerLocation.x); 
   	// print the board character
	   od_printf("`blue on cyan`O");
     	return;
   }
	// player is in one of the home spots
	else if (player.locations[playerNumber][pieceNumber] > TOTAL_SPACES)
	{
	  // 1/97 added HOME_1 the -2 is a kludge fix
		od_set_cursor(homeLocations[playerNumber][(player.locations[playerNumber][pieceNumber])-HOME_1-2].y, 
					  homeLocations[playerNumber][(player.locations[playerNumber][pieceNumber])-HOME_1-2].x); 
	  od_printf("O");
      return;
	}
   // added 1/97
   // else a normal spot
   else
   {
	od_set_cursor(board[player.locations[playerNumber][pieceNumber]].y, 
					  board[player.locations[playerNumber][pieceNumber]].x); 
   }

   // see what we need to print, another player, the board or start
	for (y=0;y<MAX_PLAYERS; y++)
	{
		for (i=0;i<MAX_PIECES;i++)
		{
			// only check other players and on the board (not in "home")
			if (player.locations[y][i] == 
					player.locations[playerNumber][pieceNumber] && y != playerNumber
						&& player.locations[y][i] < TOTAL_SPACES)
			{
			// set the correct color and display that player
			switch (y)
			{
				case 0:
				  od_printf("`bright magenta on cyan`");
					break;
				 case 1:
					od_printf("`bright green on cyan`");
				  break;
				 case 2:
					od_printf("`bright red on cyan`");
				  break;
				case 3:
					 od_printf("`bright yellow on cyan`");
				  break;
			}
			od_printf("%d", i+1);
             
			return;
		}
	
		}
	}

	// print the board character
	//od_printf("`blue on cyan`O");
	od_printf("`bright white on black`O");

	return;
}


/* 
   This function will display the piece in the appropriate color and at the
   correct position.  Used for setting up the game and for moving pieces...
   // 1/97 looks correct now

*/
void ShowWaHoo( char playerNumber, char pieceNumber)
{
		
	// set the correct color to display this user in
	switch (playerNumber)
	{
		case 0:
			od_printf("`bright magenta on cyan`");
			break;
		case 1:
			od_printf("`bright green on cyan`");
		 break;
		case 2:
			od_printf("`bright red on cyan`");
		 break;
		case 3:
			od_printf("`bright yellow on cyan`");
		 break;
	}

	////// set the cursor to the correct position
	// player is at start
	if (player.locations[playerNumber][pieceNumber] == EMPTY)
	{
		// check which player it is and assign the X and Y to that cell
		od_set_cursor(startLocations[playerNumber][pieceNumber].y, 
					  startLocations[playerNumber][pieceNumber].x); 
   	switch (playerNumber)
	   {
		   case 0:
				od_printf("`bright magenta`");
   			break;
	   	case 1:
		   	od_printf("`bright green`");
   		 break;
	   	case 2:
		   	od_printf("`bright red`");
   		 break;
	   	case 3:
		   	od_printf("`bright yellow`");
   		 break;
	   }

	}
   // player is in center
	else if (player.locations[playerNumber][pieceNumber] == CENTER)
	{
		// check which player it is and assign the X and Y to that cell
		od_set_cursor(centerLocation.y, centerLocation.x); 
	}
	// player is in one of the home spots
	else if (player.locations[playerNumber][pieceNumber] > TOTAL_SPACES)
	{  
	  // 1/97 added the - HOME_1 the -2 is a klduge fix
		od_set_cursor(homeLocations[playerNumber][(player.locations[playerNumber][pieceNumber])-HOME_1-2].y, 
					  homeLocations[playerNumber][(player.locations[playerNumber][pieceNumber])-HOME_1-2].x); 
   	switch (playerNumber)
	   {
		   case 0:
				od_printf("`bright magenta`");
   			break;
	   	case 1:
		   	od_printf("`bright green`");
   		 break;
	   	case 2:
		   	od_printf("`bright red`");
   		 break;
	   	case 3:
		   	od_printf("`bright yellow`");
   		 break;
	   }

	}
   // added 1/97
   // else a normal spot
   else
   {
	od_set_cursor(board[player.locations[playerNumber][pieceNumber]].y, 
					  board[player.locations[playerNumber][pieceNumber]].x); 
   }

   

	// print the player's character
	od_printf("%d", pieceNumber+1);
/*
od_set_cursor(1,1);
od_printf("Player: %d Piece: %d X: %d y: %d loc: %d Num: %d", playerNumber, 
   pieceNumber, homeLocations[playerNumber][(player.locations[playerNumber][pieceNumber])-HOME_1].x, 
   homeLocations[playerNumber][(player.locations[playerNumber][pieceNumber])-HOME_1].y,
   player.locations[playerNumber][pieceNumber],
   player.locations[playerNumber][pieceNumber]-HOME_1);
if (od_get_key(FALSE))
   od_get_key(TRUE);
*/

	return;
}

/*
	This function will either initialize a board if it is a new game, or
	restore the saved information.
   checked 1/97 looks good
*/
void SetupWaHooBoard( void )
{
	char i, y;
	
	g_clr_scr();
	g_send_file("wahoobd"); // send the game board to the screen

	// if not a saved game, init the memory to empty positions (are at start)
	if ( player.savedGame != TRUE)
	{
		for (i=0;i<MAX_PLAYERS;i++)
			for (y=0;y<MAX_PIECES;y++)
				player.locations[i][y] = EMPTY;
	
		player.playerColor = g_rand(MAX_PLAYERS-1);
	}

	// loop to display the pieces on the board...
	for (i=0;i<MAX_PLAYERS;i++)
		for (y=0;y<MAX_PIECES;y++)
			ShowWaHoo(i, y);

	// let the user know what color they are...
	switch(player.playerColor)
	{
		case 0:
		 od_set_cursor(2,50);
			od_printf("`bright magenta` You ");
			break;
		case 1:
		 od_set_cursor(16,50);
			od_printf("`bright green` You ");
			break;
		case 2:
		 od_set_cursor(16,25);
			od_printf("`bright red` You ");
			break;
		case 3:
		 od_set_cursor(2,25);
			od_printf("`bright yellow` You ");
			break;
	}

	player.savedGame = TRUE; // default to saving positions

	return;
}


/* This function initializes the x and y coordinates of the board into memory
   It also inits the startLocations and homeLocations for the playing pieces

   // double checked 1/97 (looks right)
*/

void InitWaHooBoard( void )
{
	char i;

   // init the center location
   centerLocation.x = 39;
   centerLocation.y = 10;
	///// init the starting locations and home locations

	// player 1
	for (i=0;i<MAX_PIECES;i++)
	{
		startLocations[0][i].x = 50 + i;
		startLocations[0][i].y = 3;
		homeLocations[0][i].x = 39;
		homeLocations[0][i].y = 3 + i;
	}

	// player 2
	for (i=0;i<MAX_PIECES;i++)
	{
		startLocations[1][i].x = 50 + i;
		startLocations[1][i].y = 17;
		homeLocations[1][i].x = 55 - i;
		homeLocations[1][i].y = 10;
	}

	// player 3
	for (i=0;i<MAX_PIECES;i++)
	{
		startLocations[2][i].x = 25 + i;
		startLocations[2][i].y = 17;
		homeLocations[2][i].x = 39;
		homeLocations[2][i].y = 17-i;
	}

	// player 4
	for (i=0;i<MAX_PIECES;i++)
	{
		startLocations[3][i].x = 25 + i;
		startLocations[3][i].y = 3;
		homeLocations[3][i].x = 23 + i;
		homeLocations[3][i].y = 10;
	}

	///// init the board into memory

	for (i=0;i<=4;i++)                      //x  x  x  x  x
	{
		board[i].y = 1;
		board[i].x = 33+i*3;
	}

	for (i=5;i<=7;i++)                                    //x
	{                                                     //x
		board[i].y = 3+(i-5);                              //x
		board[i].x = 45;
	}

	for (i=8;i<=12;i++)                                   //x  x  x  x  x
	{
		board[i].y = 7;
		board[i].x = 45+(i-8)*3;
	}

	for (i=13;i<=15;i++)                                               //x
	{                                                                  //x
		board[i].y = 9+(i-13);                                         //x
		board[i].x = 57;
	}

	for (i=16;i<=20;i++)                                  //x  x  x  x  x
	{
		board[i].y = 13;
		board[i].x = 57-(i-16)*3;
	}
	
	for (i=21;i<=23;i++)                                  //x
	{                                                     //x
		board[i].y = 15+(i-21);                            //x
		board[i].x = 45;
	}

	for (i=24;i<=28;i++)                      //x  x  x  x  x
	{
		board[i].y = 19;
		board[i].x = 45-(i-24)*3;
	}

	for (i=29;i<=31;i++)
	{
		board[i].y = 17-(i-29);                //x
		board[i].x = 33;                       //x
	}                                         //x

	for (i=32;i<=36;i++)
	{
		board[i].y = 13;           //x  x  x  x  x
		board[i].x = 33-(i-32)*3;
	}
	
	for (i=37;i<=39;i++)
	{                             //x
		board[i].y = 11-(i-37);    //x
		board[i].x = 21;           //x
	}

	for (i=40;i<=44;i++)
	{
		board[i].y = 7;            //x  x  x  x  x
		board[i].x = 21+(i-40)*3;
	}

	for (i=45;i<=47;i++)                      //x
	{                                         //x
		board[i].y = 5-(i-45);                 //x
		board[i].x = 33;
	}

	return;
}

INT16 WaHooInit( void )
{
	char choice;

	choice = PromptBox("Do you need instructions (`bright cyan`y`cyan`/`bright cyan`N`cyan`) or `bright red`Q`cyan`)uit?", "", "YN\n\rQ", FALSE);

	if (choice == 'Q')
	   return(TRUE);
	else if (choice == 'Y')
	{
		// display the instructions
		g_send_file(ibbsgametitle);
		gac_pause();
	}

	return(FALSE);

}

// Replace with char that is dot in center...

/* Small dice
|   |
| ` |

|.  |
|  .|

|.  |
| `.|

|. .|
|. .|

|' '|
|,',|

-----
|...|
|...|
-----

 .

 :

...

: :

:.:

-----
|:::|
-----
*/

// This function produces dice on the screen...
void SmallDieRoll( INT16 die, INT16 x, INT16 y)
{

    char row1[4], row2[4];
        
        if (od_control.user_rip || od_control.user_ansi)
        {
            // Set the coordinates, so it looks like the dice were thrown...they are randomized
                
            if (die == 1)
            {                          
                sprintf(row1, "   ");
                sprintf(row2, " %c ", '\'');
            }
            else if (die == 2)
            {
                sprintf(row1, "%c  ", '\'');
                sprintf(row2, "  %c", ',');
            }
            else if (die == 3)
            {
                sprintf(row1, "%c  ", '\'');
                sprintf(row2, " %c%c", '\'',',');
            }
            else if (die == 4)
            {
                sprintf(row1, "%c %c", '\'','\'');
                sprintf(row2, "%c %c", ',',',');
            }
            else if (die == 5)
            {
                sprintf(row1, "%c %c", '\'','\'');
                sprintf(row2, "%c%c%c", ',','\'', ',');
            }
            else if (die == 6)
            {
                sprintf(row1, "%c%c%c", '\'','\'','\'');
                sprintf(row2, "%c%c%c", ',', ',', ',');
            }
                    
            // Create the boxes that serves as the outside of the dice...
            // od_set_attrib(0x40);
            od_set_attrib(0x04);
            od_draw_box((char) x, (char) y, (char) (x + 4), (char) (y + 3));
            od_set_attrib(0x4f);
            od_set_cursor((y+1), (x+1));
            od_printf("%s", row1);
            od_set_cursor((y+2), (x+1));
            od_printf("%s``", row2);
        }
        else
        {
            od_printf("`Bright cyan`Die Roll: `Bright white`%d\n\r\n\r", die);
        }

    return;
}

void DeleteDieRoll( INT16 x, INT16 y)
{
                    
   // Create the boxes that serves as the outside of the dice...
   // od_set_attrib(0x40);
   od_set_attrib(0x00);
   od_draw_box((char) x, (char) y, (char) (x + 4), (char) (y + 3));

   return;
}





