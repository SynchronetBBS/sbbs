#include "gac_fc.h"

/* FREECELL


Add the ability to Take back a move...
Add the ability to move entire columns...

	This game uses the GAME SDK in gac_cs\gamesdk

  Play a game...
	- Shuffle the cards
	- Deal the cards faceup into the columns
	-  8 Columns, 4 Freecells (top left), 4 home cells (top right)

	- loop to play the game
	{
	- Allow the player to move cards
	- Check if legal move
	- Check if any legal moves remain
	}
	

	FREETITL - Displayed when asked if they need instructions
	FREEINST - Instructions for playing FREECELL


	need to add the ability to move an entire column after we get the game
	working right.  ONLY when moving a column to another column!

	check from top of column to find a card that will fit on the new column,
	then check if the card below that one in this column is in order, than the
	next card etc...

	If they can all be moved, then see if there are enough blank spots to move
	them, we need 1 less than the number to move. (e.g. to move 3 cards, you
	must have 2 free spaces).

*/



void FreeCell( void )
{
	char prompt[128], response;
	char gameOver=FALSE;
	INT16 ratio;

#ifdef GAC_DEBUG
gac_debug = myopen(gac_debugfile, "a", SH_DENYRW);
fprintf(gac_debug, "  FreeCell()\n");
fclose(gac_debug);
#endif

  g_clr_scr();
  g_send_file("freetitl");
  if (FreeCellInit() == TRUE) return;   // Check if user chose quit

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
	DealFreeCell();   // start the game out (shuffle and put cards on table)

	// let the user play the game
	gameOver = PlayFreeCell();
	ratio = (INT16) ( ((float)player.gamesWon/(float) player.games) * 100.);
	if( gameOver == WON)
	{
		sprintf(prompt, "Congratulations!  You won the game (`bright cyan`%2d%%`cyan` Win Ratio).", ratio);
		player.gamesWon++;
		player.money += WIN_BONUS;
		FreeShowScore();
	}
	else if (gameOver == LOST)
	{
		sprintf(prompt, "To bad, you lost that game (`bright cyan`%2d%%`cyan` Win Ratio).", ratio);
	}

	if (gameOver == SAVED)
	{
		player.savedGame = TRUE;
		sprintf(prompt, "You currently have a `bright cyan`%2d%%`cyan` Win Ratio and `bright cyan`$%lu`cyan`", ratio, player.money);
		PromptBox("Come back when you want to continue.", prompt, "ANY", FALSE);
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
INT16 PlayFreeCell( void )
{
	char gameOver=FALSE, moved=FALSE;
	char bottomCard, topCard, card1, card2;
	char choice;

	while(gameOver==FALSE)
	{
		bottomCard = EMPTY;
		topCard = EMPTY;
		card1=EMPTY;
		card2=EMPTY;

	moved = FALSE;
	while (moved == FALSE)
	{
	  // show the choice
	  od_set_cursor(22,60);
	  od_printf("`cyan` Move: `bright cyan`%c`cyan` to `bright cyan`%c ", (card1==EMPTY || topCard==EMPTY)?'?':card1, (card2==EMPTY)?'?':card2);
	  
	  // get the user's choice
	  choice=od_get_answer("12345678ASDFZXCV QRI");
	  choice = FreeKeys(choice);
	  // show the choice
	  od_set_cursor(22,60);
	  od_printf("`cyan` Move: `bright cyan`%c`cyan` to `bright cyan`%c ", (card1==EMPTY || topCard==EMPTY)?'?':card1, (card2==EMPTY)?'?':card2);

	  
	  // act on the user's choice
	  switch(toupper(choice))
	  {
		// user Cacelled the move
		case ' ':
			bottomCard = EMPTY;
			topCard = EMPTY;
			card1 = EMPTY;
			card2 = EMPTY;
			break;
		case 'R':
			// redraw the screen
			DealFreeCell();
			break;
		case 'I':
			// display the instructions
			od_save_screen(screen);
			g_clr_scr();
			g_send_file(ibbsgametitle);
			gac_pause();
			// DealFreeCell();
			od_restore_screen(screen);
			break;
		// the user picked a column
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		{
			// card picked up is Top Card
			if (topCard == EMPTY)
			{
				card1 = choice;
				topCard = player.column[choice - '1'][GetLastFreeCard(choice - '1')];
			}
			else
			{
				card2 = choice;
				bottomCard = player.column[choice - '1'][GetLastFreeCard(choice - '1')];
				moved = TRUE;
			}
		}
		break;

		// the user picked a HOME_CELL
		case 'M':
		case 'N':
		case 'O':
		case 'P':
		{
			if (topCard == EMPTY)
			{
				PromptBox("ILLEGAL MOVE: You cannot move cards out of the Home Cells.\n\r", "Press any key to continue", "ANY", FALSE);
			}
			else
			{
				card2 = choice;
				bottomCard = player.homeCell[choice - 'M'];
				moved = TRUE;
			}
		}
		break;


		// the user picked a FREE_CELL
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		{
			if (topCard == EMPTY)
			{
				card1 = choice;
				topCard = player.freeCell[choice - 'A'];
			}
			else
			{
				card2 = choice;
				bottomCard = player.freeCell[choice - 'A'];
				moved = TRUE;
			}
		}
		break;


		// if user hits QUIT, ask if they want to save...if they do
		// return SAVED else return LOST
		case 'Q':
		{
			choice = PromptBox("`bright cyan`G`cyan`)o back to the game, `bright cyan`s`cyan`)ave or `bright cyan`f`cyan`)orfiet?", "", "SGF\n\r", FALSE);
			if (choice == 'F')
			{
				gameOver = LOST;
				return(gameOver);
			}
			else if (choice == 'S')
			{
				gameOver = SAVED;
				return(gameOver);
			}
		}
	  } // end of Switch
	} // end of while

// od_set_cursor(22,1);
// od_printf("Top: %d   Bottom: %d  Card1: %c   Card2: %c", topCard%13, bottomCard%13, card1, card2);

	// see if the user's move is appropriate...
	switch(card2)
	{
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
			// check if we can move an entire column
			if (card1 >= '1' && card1 <= '8')
			{
				// see if we moved the column
				if (MoveFreeColumn( card1, card2) == TRUE)
					break;

			}
			// move a single card to a column
			if (CheckFreeCard( topCard, bottomCard, COLUMN) == TRUE)
			{
				// move the cards
				MoveFreeCard(card1, card2, topCard, bottomCard);
			}
			else
				PromptBox("ILLEGAL MOVE: You can only move a card into a column","if it is empty, or if it is the opposite color and one smaller.", "ANY", FALSE);
			break;
		case 'M':
		case 'N':
		case 'O':
		case 'P':
			// Move to a Home Cell
			if (CheckFreeCard( topCard, bottomCard, HOME_CELL) == TRUE)
			{
				// move the cards
				MoveFreeCard(card1, card2, topCard, bottomCard);
			}
			else
				PromptBox("ILLEGAL MOVE: All cards in a Home Cell must be the same","suit and in order starting with an Ace.", "ANY", FALSE);
			break;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
			// move to a FREE CELL
			if (CheckFreeCard( topCard, bottomCard, FREE_CELL) == TRUE)
			{
				// move the cards
				MoveFreeCard(card1, card2, topCard, bottomCard);
			}
			else
				PromptBox("ILLEGAL MOVE: You can only move a card into a Free Cell","if it is empty.  Press any key to continue", "ANY", FALSE);
			break;
	} // end of switch

	// reset the chosen cards...
	card1=card2=EMPTY;

	// check if we can move cards to the home_cells if they are no
	// longer needed.
// Test this after we get the player movements working
	CheckHomeCells();
	// check if the game has been won, lost or should continue
	if (gameOver == FALSE) 
		gameOver=CheckFreeCell();
	}

	return(gameOver);
}


// this function will attempt to move cards to the home cells
// automatically.  It will only move a card if all of the cards below
// it of the opposite colors have been moved already.
void CheckHomeCells( void )
{
	char more,i,y;



	more = TRUE;
	while (more == TRUE)
	{
	  more=FALSE;

		// check if we can move a card from a column to the home cells
		for (i=0;i<MAX_COLUMNS;i++)
		{
			if (player.column[i][GetLastFreeCard(i)] == EMPTY) 
				continue;
			for (y=0;y<4;y++)
			{
				if (CheckFreeCard( player.column[i][GetLastFreeCard(i)], player.homeCell[y], GOING_HOME) == TRUE)
				{
					// move the cards
					MoveFreeCard('1' + i, 'M' + y, player.column[i][GetLastFreeCard(i)], player.homeCell[y]);
					more = TRUE; // loop again if necessary
				}
			}
		}

		// check if we can move a card from free cell to the home cells
		for (i=0;i<4;i++)
		{
			for (y=0;y<4;y++)
			{
				if (player.freeCell[i] == EMPTY) 
					continue;
				if (CheckFreeCard( player.freeCell[i], player.homeCell[y], GOING_HOME) == TRUE)
				{
					// move the cards
					MoveFreeCard('A' + i, 'M' + y, player.freeCell[i], player.homeCell[y]);
					more = TRUE; // loop again if necessary
				}
			}
		}



	} // end of while loop

	return;
}

// This function moves a card from location 1 to location 2 and redraws
// the appropriate graphics
void MoveFreeCard( char location1, char location2, char topCard, char bottomCard)
{
	char i;

	// must delete the old card, redraw the top cards of the columns
	// if it was a column,
	// move the card and then redraw the card there...

	// delete the old card if coming from column or free cell
	switch(location1)
	{
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		// delete the card
		DeleteSmallCard(COLUMN_X + X_OFFSET*(location1-'1'),
				ROW_Y + Y_OFFSET*GetLastFreeCard(location1-'1') );
		player.column[location1-'1'][GetLastFreeCard(location1-'1')] = EMPTY;
		// redraw the cards in the columns
		// 12/96 With smaller cards, we only need to draw the one column...
		if (player.column[location1-'1'][GetLastFreeCard(location1-'1')] != EMPTY)
			DisplaySmallCard( COLUMN_X + (location1-'1')*X_OFFSET, ROW_Y + GetLastFreeCard((location1-'1'))*Y_OFFSET,
				player.column[(location1-'1')][GetLastFreeCard((location1-'1'))]);
		
		/*
		for (i=0;i<8;i++)
			{
				DisplaySmallCard( COLUMN_X + i*X_OFFSET, ROW_Y + GetLastFreeCard(i)*Y_OFFSET,
					player.column[i][GetLastFreeCard(i)]);
			}
		*/
		break;
		// FREE_CELLS
		case 'A':
		case 'B':
		case 'C':
		case 'D':
			player.freeCell[location1-'A'] = EMPTY;
			DeleteSmallCard(FREE_X, FREE_OFFSET*(location1-'A') + FREE_Y);
			// 12/96 redisplay all four cards
			for (location1 = 'A'; location1 <= 'D'; location1++)
			{
				if (player.freeCell[location1-'A'] != EMPTY)
					DisplaySmallCard( FREE_X, (location1-'A')*FREE_OFFSET + FREE_Y,
						player.freeCell[location1-'A'] );
			}

			break;
		// skip deleting HOME_CELLS

	} // end of switch



	// copy the card to location 2 and draw it
	switch(location2)
	{
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
			// put the card in the new position
			if (player.column[location2-'1'][GetLastFreeCard(location2-'1')] == EMPTY)
				player.column[location2-'1'][GetLastFreeCard(location2-'1')] = topCard;
			else
				player.column[location2-'1'][GetLastFreeCard(location2-'1') + 1] = topCard;
			// draw teh card in the new position
			DisplaySmallCard( COLUMN_X + (location2-'1')*X_OFFSET, ROW_Y + GetLastFreeCard(location2-'1')*Y_OFFSET,
			player.column[location2-'1'][GetLastFreeCard(location2-'1')]);
			break;
		// FREE_CELLS
		case 'A':
		case 'B':
		case 'C':
		case 'D':
			// copy the card to the new location
			player.freeCell[location2-'A'] = topCard;
			// display the card in teh new location
			// 12/96 redisplay all the free cells if needed
			for (location2 = location2; location2 <= 'D'; location2++)
			{
				if (player.freeCell[location2-'A'] != EMPTY)
					DisplaySmallCard( FREE_X, (location2-'A')*FREE_OFFSET + FREE_Y,
						player.freeCell[location2-'A'] );
			}
		break;
		// HOME_CELLS
		case 'M':
		case 'N':
		case 'O':
		case 'P':
			// copy the card to the new location
			player.homeCell[location2-'M'] = topCard;
			// display the card in teh new location
			// 12/96 Draw all four cards again (if needed)
			for (location2 = location2; location2 <= 'P'; location2++)
			{
				if (player.homeCell[location2-'M'] != EMPTY)
					DisplaySmallCard( HOME_X, (location2-'M')*FREE_OFFSET + HOME_Y,
						player.homeCell[location2-'M'] );
			}
			// Give the player their bonus
			player.money += HOME_BONUS;
			FreeShowScore();

		break;

	} // end of switch


	return;
}

// This function checks if a game has been lost, won or should continue
// returns FALSE, LOST or WON
INT16 CheckFreeCell( void )
{
	char more=FALSE;
	INT16 i,y; //loop counters
	char movesLeft;


	// check if a game has been won
	for (i=0;i<8;i++)
		if (player.column[i][0] != EMPTY) 
			more = TRUE;

	for (i=0;i<4;i++)
		if (player.freeCell[i] != EMPTY) 
			more = TRUE;

	if (more == FALSE) 
		return(WON);

	// reset more to FALSE so we can check
	movesLeft = 0;

	/////// check if the game has been lost (no free moves remaining)
	// check each free cell, if one is open allow them to play
	for (i=0;i<4;i++)
		if (player.freeCell[i] == EMPTY) 
			movesLeft+=11;

	// if a column is open, allow them to keep playing
	for (i=0;i<8;i++)
		if (player.column[i][0] == EMPTY) 
			movesLeft+=11;

	// no open cells, so now we need to check the cards and see if they
	// can still play one.
	// Start by checking if a card from a column can go onto a home cell
	for (i=0;i<4;i++)
	{
		for (y=0;y<8;y++)
		{
			if ( CheckFreeCard(player.column[y][GetLastFreeCard(y)], player.homeCell[i], HOME_CELL) == TRUE)
			movesLeft++;
		}
	}

	// check if a card from the free cells can go onto a home cell
	for (i=0;i<4;i++)
	{
		for (y=0;y<4;y++)
		{
			if ( CheckFreeCard(player.homeCell[y], player.freeCell[i], HOME_CELL) == TRUE)
			movesLeft++;
		}
	}


	// check if a card from the free cells can go onto a column
	for (i=0;i<4;i++)
	{
		for (y=0;y<8;y++)
		{
			if ( CheckFreeCard(player.freeCell[i], player.column[y][GetLastFreeCard(y)], COLUMN) == TRUE)
			movesLeft++;
		}
	}

	// check if a card from a column can go onto another column
	for (i=0;i<8;i++)
	{
		for (y=0;y<8;y++)
		{
			if ( CheckFreeCard(player.column[y][GetLastFreeCard(y)], player.column[i][GetLastFreeCard(i)], COLUMN) == TRUE)
			movesLeft++;
		}
	}

	// print the number of moves left to the screen!
//od_set_cursor(23,2);
//od_printf("`bright white` Moves Left: `bright cyan`%d  ", movesLeft);
	
	// see if the user can keep playing
	if (movesLeft == 1)
	{
		// warn user they only have one move left
		PromptBox("Caution, there is only one legal move left!", "Press any key to continue", "ANY", TRUE);
		return(FALSE);
	}
	else if (movesLeft > 0)
		return(FALSE);
	else
		return(LOST);
}


// this function returns an index to the last card in the column
INT16 GetLastFreeCard( INT16 column )
{
	char i;

	for (i=MAX_COL_ROWS-1; i>=0;i--)
	if (player.column[column][i] != EMPTY) 
		return(i);

	// didn't find a card, so return the index to the first one
	return(0);

}
// this function tests whether the card passed for topCard can go onto
// the card passed for bottomCard.  movingTo can be COLUMN, HOME_CELL, FREE_CELL

INT16 CheckFreeCard( char topCard, char bottomCard, char movingTo)
{
	char topSuit, bottomSuit;
	char topNumber, bottomNumber;
	char i, tempNumber, tempSuit, found;

	//since the cards are 0-12 for testing
	topNumber = topCard%13;
	bottomNumber = bottomCard%13;

	// The cards are 1-52, so we must subtract 1 from the card before dividing, otherwise
	// an ace will not be in the correct suit
	// suites are H=0, C=1, D=2, S=3
	topSuit = (topCard - 1)/13;
	bottomSuit = (bottomCard - 1)/13;

	// check FREE_CELL
	if (movingTo == FREE_CELL)
	{
		// can only move to a free cell if it is empty
		if (bottomCard == EMPTY) 
			return(TRUE);
	}

	// check GOING_HOME this part will only return true if all cards of
	// the opposite color and lower numbers have been placed in the HOME_CELLs
	else if (movingTo == GOING_HOME)
	{
		// CAUTION, an ace is 0...always allow aces to auto home...
		// allow aces to go automatically
		if (bottomCard == EMPTY && topNumber == 0)
			return(TRUE);
		// return FALSE if the cell is empty and the card is not an Ace
		else if (bottomCard == EMPTY)
			return(FALSE);
		// allow 2's to go automatically
		else if (topSuit == bottomSuit && topNumber == 1 && bottomNumber == 0)
			return(TRUE);
		else if (topSuit != bottomSuit || topNumber != bottomNumber+1)
			return(FALSE);
		else
		{
			// We only want to auto home if all cards of the opposite suits
			// and lower numbers are in the home cells...

			// check for cards smaller than our current number
			found = 0;
			for (i=0;i<4;i++)
			{
				// keep checking if empty
				if (player.homeCell[i] == EMPTY) 
					continue;

				tempNumber = player.homeCell[i]%13;
				// suites are H=0, C=1, D=2, S=3
				tempSuit = (player.homeCell[i] - 1)/13;
				
				// keep checking if this is our suit stack
				if (tempSuit == topSuit)
				// if (player.homeCell[i] == bottomCard) 
					continue;


				// check for opposite suit cards that are greater or
				// equal to our card number minus one (must find two)...
				switch(topSuit)
				{
					case HEARTS:
					case DIAMONDS:
						if ( (tempSuit == CLUBS || tempSuit == SPADES) &&
							tempNumber >= topNumber-1 )
							found++;
						break;

					case CLUBS:
					case SPADES:
						if ( (tempSuit == HEARTS || tempSuit == DIAMONDS) &&
							tempNumber >= topNumber-1 )
							found++;
						break;
				}

			}
			// if we found a card smaller than the topcard minus one, return
			// false
			if (found != 2)
				return(FALSE);
			else
				return(TRUE);
		}
	}
	// check HOME_CELL
	else if (movingTo == HOME_CELL)
	{
		// can only go to a home cell if one larger and same suit
		// CAUTION, an ace is 0...
		if (topNumber == 0 && bottomCard == EMPTY)
			return(TRUE);
		else if (topSuit == bottomSuit && topNumber == bottomNumber+1)
			return(TRUE);
	}

	// check COLUMN
	else if (movingTo == COLUMN)
	{
		// can only go to a column if the column is empty OR the card
		// is a different color suit and one smaller.
		// CAUTION, an ace is 0...

		// check if empty column
		if (bottomCard == EMPTY)
			return(TRUE);
		// check if card will go onto the bottom of the column
		switch (bottomSuit)
		{
			case HEARTS:
				if ( (topSuit == CLUBS || topSuit == SPADES) &&
					topNumber == bottomNumber-1)
					return(TRUE);
				break;
			case CLUBS:
				if ( (topSuit == HEARTS || topSuit == DIAMONDS) &&
					topNumber == bottomNumber-1)
					return(TRUE);
				break;
			case DIAMONDS:
				if ( (topSuit == CLUBS || topSuit == SPADES) &&
					topNumber == bottomNumber-1)
					return(TRUE);
				break;
			case SPADES:
				if ( (topSuit == HEARTS || topSuit == DIAMONDS) &&
					topNumber == bottomNumber-1)
					return(TRUE);
				break;
			default:
				break;
		}
	}

	return(FALSE);
}


INT16 FreeCellInit( void )
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



// *********************** deal ************************************
void DealFreeCell( void )
{
	INT16 i,        // loop counter
	   column,    // what column are we putting the card into
	   row,       // row to put the card in...
	   card_avail,    // boolean, has that card been dealt?
	   crd;       // random number between 1 and 52
	char deck[53];


	#ifdef GAC_DEBUG
	gac_debug = myopen(gac_debugfile, "a", SH_DENYRW);
	fprintf(gac_debug, "  Deal()\n");
	fclose(gac_debug);
	#endif

	g_clr_scr();
  // init the deck of cards
  for (i=0; i<=52; i++)
  { // initialize deck of cards
	deck[i] = i;
  }

  // setup the screen (draw a box around it, Title at top, labels...etc)
  od_printf("`cyan`");
  od_draw_box( 1, 1, 75, 23);
  od_set_cursor(1,(75-strlen(od_control.od_prog_name))/2);
  od_printf("`bright white`%s", od_control.od_prog_name);
  od_set_cursor(HOME_Y-1, HOME_X);
  od_printf("`bright white` Home ");
  od_set_cursor(FREE_Y-1, FREE_X);
  od_printf("`bright white` Free ");
  od_set_cursor(22,3);
  od_printf("`bright white`KEYS: `bright cyan`Q`cyan`)uit, `bright cyan`R`cyan`)edraw, `bright cyan`I`cyan`)nstructions or choose a card.");
  // show the user's current winnings
  FreeShowScore();

  // draw the keys for the cards
  for (i='1';i<='8';i++)
  {
	od_set_cursor(ROW_Y -1, COLUMN_X+X_OFFSET*(i-'1') +3);
	od_printf("`bright white`%c", i);
  }
  for (i='A';i<='D';i++)
  {
	od_set_cursor(FREE_Y+FREE_OFFSET*(i-'A'), FREE_X-1);
	od_printf("`bright white`%c", FreeDefinedKeys(i));
  }
  for (i='M';i<='P';i++)
  {
	od_set_cursor(HOME_Y+FREE_OFFSET*(i-'M'), HOME_X-1);
	od_printf("`bright white`%c", FreeDefinedKeys(i));
  }

  // redisplay all of the cards saved in memory
  if (player.savedGame == TRUE)
  {
	  // PromptBox("Continuing Saved Game!", "Press any key to continue", "ANY", FALSE);
	  
	  // display the HOME_CELLS
	  for (i=0;i<4;i++)
		if (player.homeCell[i] != EMPTY)
			DisplaySmallCard(HOME_X, FREE_OFFSET*i + HOME_Y, player.homeCell[i]);

	  // display the FREE_CELLS
	  for (i=0;i<4;i++)
		if (player.freeCell[i] != EMPTY)
			DisplaySmallCard(FREE_X, FREE_OFFSET*i + FREE_Y, player.freeCell[i]);

	  // display the cards in the columns
	  for (i=0;i<8;i++)
		{
			row=0;
			while (player.column[i][row] != EMPTY)
			{
				DisplaySmallCard(COLUMN_X+X_OFFSET*i, ROW_Y+Y_OFFSET*row,
				player.column[i][row]);
				row++;
			}
		}
  }

  // randomly place the cards into columns 1-8 if a game is not already
  // being played
  else
  {
	player.games++;
	// loop to init all variables to empty
	for ( column=0; column<MAX_COLUMNS; column++ )
	{
		for( row=0; row<MAX_COL_ROWS; row++ )
		{
			player.column[column][row]=EMPTY;
		}
	}

	for ( i=0; i<4; i++ )
		player.freeCell[i] = EMPTY;

	for ( i=0; i<4; i++ )
		player.homeCell[i] = EMPTY;

	column = 0;
	row = 0;
	// loop to place all cards in columns
	for ( i=0; i<52; i++ )
	{
	  
	  card_avail = FALSE;   // card not available, keep looking
	  crd = (char) g_rand(51) + 1;
	  while (!card_avail)    // pick a random card
	  {
		if (crd>52)
			crd = (char) g_rand(51) + 1;
		if (deck[crd] == EMPTY)
		{
		  card_avail = FALSE;
			crd++; // speeds up shuffle
		}
		else
		{
		  card_avail = TRUE;
		  deck[crd] = EMPTY;
		}

	  } // end of while
	  
	  // assign the card to the next column
	  player.column[column][row] = (char) crd;
	  
	  // display the cards on the screen in the appropriate spots
	  DisplaySmallCard( COLUMN_X + column*X_OFFSET, ROW_Y + row*Y_OFFSET, (char) crd);

	  column++;
	  if (column > MAX_COLUMNS - 1 )
	  {
		column=0;
		row++;
	  }

	} // end of for loop
  } // end of if


  player.savedGame = TRUE; // default to TRUE;
  return;
}


/* This function deletes the card from the screen (leaves a black box)
*/
void DeleteSmallCard( INT16 left, INT16 top)
{
	// Set the color of the box to be created.
	od_set_color(D_BLACK, D_BLACK);
	// Create a box for the card to go in.
	od_draw_box( (char) left, (char) top, (char) (left + CARD_X), (char) (top + CARD_Y));

	return;
}

// This function displays a card in ANSI format at the specified
// top-left coordinates.  The cards are numbered 1-52.  Corresponding
// to the cards 2-A in the suit order Hearts, Clubs, Diamonds, Spades.
// e.g. # - 1 2 3 4 5 6 7 8  9 10 11 12 13   
//   card - 2 3 4 5 6 7 8 9 10 J  Q  K  A    (Hearts)
// Need to use smaller cards than other games...
// 12/96 Small cards do not have a border...thus they are 4x2 smaller.
void DisplaySmallCard( INT16 left, INT16 top, INT16 card)
{
	// the lines that will ultimately be printed.
	char line1[125], line2[125], line3[125], line4[125], line5[125];
	// some color configs...
	char suitcolor[20], cardchar, barcolor[25];
	// storage for the current border types
	// char box_chars[8];
	// loop
	INT16 i;
	
	// 12/96    
	/*
	// Store the current box characters
	for (i=0;i<8;i++)
	{
		box_chars[i] = od_control.od_box_chars[i];
	}
	// Set the border I want for the cards
	od_control.od_box_chars[BOX_UPPERLEFT] = (unsigned char) 201;
	od_control.od_box_chars[BOX_TOP] = (unsigned char) 205;
	od_control.od_box_chars[BOX_UPPERRIGHT] = (unsigned char) 187;
	od_control.od_box_chars[BOX_LEFT] = (unsigned char) 186;
	od_control.od_box_chars[BOX_LOWERLEFT] = (unsigned char) 200;
	od_control.od_box_chars[BOX_LOWERRIGHT] = (unsigned char) 188;
	od_control.od_box_chars[BOX_BOTTOM] = (unsigned char) 205;
	od_control.od_box_chars[BOX_RIGHT] = (unsigned char) 186;
	*/


	// Set the color of the bar in the center of the card...
	sprintf(barcolor, "`bright black on white`%c", 179);
	
	// This area sets the strings to be used for printing the cards.
	switch((card-1)/13)
	{              
	
	//     

		// The card is a heart
		case 0:
		{
			cardchar = '';
			strcpy(suitcolor, "`red on white`");
			break;
		}
		// The card is a club
		case 1:
		{
			cardchar = '';
			strcpy(suitcolor, "`black on white`");
			break;
		}
		// The card is a diamond
		case 2:
		{
			cardchar = '';
			strcpy(suitcolor, "`red on white`");
			break;
		}
		// The card is a spade
		case 3:
		{
			cardchar = '';
			strcpy(suitcolor, "`black on white`");
			break;
		}
	}

	// Determine which card it is and set the strings...
	switch(card%13)
	{
		// an ace (a multiple of 13)
		case 0:                    
		{
			sprintf(line1, "%sA%s%s   %s%s ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s   %s%s ", suitcolor, cardchar, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line3, "%s %s%s %c %s%s ", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s   %s%sA", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			break;
		}
		// a 2
		case 1:
		{
			sprintf(line1, "%s2%s%s %c %s%s ", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s   %s%s ", suitcolor, cardchar, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line3, "%s %s%s   %s%s ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s %c %s%s2", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			break;
		}
		// a 3
		case 2:
		{
			sprintf(line1, "%s3%s%s %c %s%s ", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s   %s%s ", suitcolor, cardchar, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line3, "%s %s%s %c %s%s ", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s %c %s%s3", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			break;
		}
		// a 4
		case 3:
		{
			sprintf(line1, "%s4%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s   %s%s ", suitcolor, cardchar, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line3, "%s %s%s   %s%s ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s%c %c%s%s4", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a 5
		case 4:
		{                          
			sprintf(line1, "%s5%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s   %s%s ", suitcolor, cardchar, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line3, "%s %s%s %c %s%s ", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s%c %c%s%s5", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a 6
		case 5:
		{
			sprintf(line1, "%s6%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s   %s%s ", suitcolor, cardchar, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line3, "%s %s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s%c %c%s%s6", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a 7
		case 6:
		{
			sprintf(line1, "%s7%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s %c %s%s ", suitcolor, cardchar, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line3, "%s %s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s%c %c%s%s7", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a 8
		case 7:
		{
			sprintf(line1, "%s8%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s %c %s%s ", suitcolor, cardchar, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line3, "%s %s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s %c %s%s%c", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s%c %c%s%s8", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a 9
		case 8:
		{
			sprintf(line1, "%s9%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s%c %c%s%s ", suitcolor, cardchar, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line3, "%s %s%s %c %s%s ", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s%c %c%s%s%c", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s%c %c%s%s9", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a 10
		case 9:
		{
			sprintf(line1, "%s1%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s0%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line3, "%s%c%s%s%c %c%s%s%c", suitcolor, cardchar, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor, cardchar);
			sprintf(line4, "%s %s%s%c %c%s%s1", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line5, "%s %s%s%c %c%s%s0", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a Jack
		case 10:
		{
			sprintf(line1, "%sJ%s%s   %s%s ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s  %c%s%s ", suitcolor, cardchar, barcolor, suitcolor, 209, barcolor, suitcolor);
			sprintf(line3, "%s %s%s  %c%s%s ", suitcolor, barcolor, suitcolor, 179, barcolor, suitcolor);
			sprintf(line4, "%s %s%s%c%c%c%s%s%c", suitcolor, barcolor, suitcolor, 212, 205, 190, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s   %s%sJ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			break;
		}
		// a Queen
		case 11:
		{
			sprintf(line1, "%sQ%s%s   %s%s ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s%c%c%c%s%s ", suitcolor, cardchar, barcolor, suitcolor, 213, 205,184 , barcolor, suitcolor);
			sprintf(line3, "%s %s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, 179,179 , barcolor, suitcolor);
			sprintf(line4, "%s %s%s%c%c%c%s%s%c", suitcolor, barcolor, suitcolor,212,205 ,181 , barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s   %s%sQ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			break;
		}
		// a King
		case 12:
		{
			sprintf(line1, "%sK%s%s   %s%s ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s%c%c%c%s%s ", suitcolor, cardchar, barcolor, suitcolor,179 ,213 ,190 , barcolor, suitcolor);
			sprintf(line3, "%s %s%s%c%c %s%s ", suitcolor, barcolor, suitcolor,198 , 181, barcolor, suitcolor);
			sprintf(line4, "%s %s%s%c%c%c%s%s%c", suitcolor, barcolor, suitcolor,179 ,212 ,184 , barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s   %s%sK", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			break;
		}
	}

	
	
	// Set the color of the box to be created.
	/* 12/96
	od_set_color(D_CYAN, D_BLACK);
	// Create a box for the card to go in.
	od_draw_box( (char) left, (char) top, (char) (left + 10), (char) (top + 6));
	*/
	
	// Print the guts of the card
	od_set_cursor(top, left);
	od_printf("%s", line1);
	od_set_cursor(top+1, left);
	od_printf("%s", line2);
	od_set_cursor(top+2, left);
	od_printf("%s", line3);
	od_set_cursor(top+3, left);
	od_printf("%s", line4);
	od_set_cursor(top+4, left);
	od_printf("%s", line5);
	
	/*
	// Restore the current box characters
	for (i=0;i<8;i++)
	{
		od_control.od_box_chars[i] = box_chars[i];
	}
	*/

	return;
}

void FreeShowScore( void )
{

	od_set_cursor(2, 67);
	od_printf("`bright white`$%5d", player.money);
}

/* This function will calculate how many cards in a column may be moved at 
   one time...
		   base
	total = (1) + (columnsFree + emptyFreeCells) + 
			(columnsFree-1 + emptyFreeCells) ... (0 + emptyFreeCells)
*/

INT16 FreeTotalMoves(char toColumn)
{
	INT16 total=0;
	INT16 emptyFreeCells=0;
	INT16 columnsFree=0;
	INT16 i;

	// count empty free cells
	for ( i=0; i<4; i++ )
		if (player.freeCell[i] == EMPTY)
			emptyFreeCells++;

	// count empty columns
	// loop to place all cards in columns
	for ( i=0; i<MAX_COLUMNS; i++ )
		if (player.column[i][0] == EMPTY && i != toColumn-'1')
			columnsFree++;

	// calculate total
	total = 1;
	for (i=columnsFree;i>=0;i--)
	{
		total += i + emptyFreeCells;
	}

//od_set_cursor(23, 60);
//od_printf("`bright white`Max Column Move %3d", total);

	return(total);
}

/* This function will attempt to move a column of cards from one column
	to another.

	Count number of cards to be moved.
	Check if we can move that many cards
	Move the cards

	return TRUE if we moved the column, otherwise return FALSE
*/

INT16 MoveFreeColumn( char col1, char col2)
{
	char needToMove=0;
	char curCard;
	INT16 i;

	// check if what we are moving to is an empty column.  If it is, ask
	// the user if they want to move the entire column or just a single card
	if (player.column[col2 - '1'][GetLastFreeCard(col2 - '1')] == EMPTY
		&& GetLastFreeCard(col1 - '1') != 0 &&
		// there are more than one cards available to move
		CheckFreeCard( player.column[col1 - '1'][GetLastFreeCard(col1 - '1')], 
			player.column[col1 - '1'][GetLastFreeCard(col1 - '1')-1], COLUMN) 
			== TRUE)
	{
		if (PromptBox("Do you want to move the `bright cyan`C`cyan`)olumn or a `bright cyan`s`cyan`)ingle card?", "", "CS\n\r", FALSE) == 'S')
			return(FALSE);
	}

	// count the total number of cards that are in order in column one that
	// will/can be placed on the card in column 2
	curCard = GetLastFreeCard(col1 - '1');
	while(1)
	{
		// check if the current card in col 1 will go on col 2
		if (CheckFreeCard( player.column[col1 - '1'][curCard], 
			player.column[col2 - '1'][GetLastFreeCard(col2 - '1')], COLUMN) 
			== TRUE &&
			// let more than one card go to an empty column...
			player.column[col2 - '1'][GetLastFreeCard(col2 - '1')] != EMPTY
			)
			break;
		// make sure we do not go below 0 for the current card
		else if (curCard-1 < 0 && 
			player.column[col2 - '1'][GetLastFreeCard(col2 - '1')] != EMPTY)
			return(FALSE);
		else if (curCard-1 < 0)
			break;
		// check if the current card is in order on the card above it.
		else if (CheckFreeCard( player.column[col1 - '1'][curCard], 
			player.column[col1 - '1'][curCard-1], COLUMN) 
			== TRUE)
			{
				curCard--;
				continue;
			}
		// see if the going to column is empty...if so move the cards we can...
		else if (player.column[col2 - '1'][GetLastFreeCard(col2 - '1')] == EMPTY)
			break;
		// the curCard will not go onto the next column, and it is not in
		// order with the card above it, so return FALSE on move column
		else
			return(FALSE);
		
	}
		
	// calculate total cards we need to move for this to work...
	needToMove = (char) (GetLastFreeCard(col1 - '1') - curCard + 1);
	
	// if second column is empty, move as many as we can...
	if (player.column[col2 - '1'][GetLastFreeCard(col2 - '1')] == EMPTY)
	{
		if (needToMove > FreeTotalMoves(col2))
		{
			curCard += needToMove-FreeTotalMoves(col2); // was a + 1

			needToMove = FreeTotalMoves(col2);
		}
	}
	
	// see if we can move this many cards...
	if (FreeTotalMoves(col2) < needToMove)
	{
		sprintf(prompt, "That requires moving `bright cyan`%d`cyan` cards.  You could only move `bright cyan`%d`cyan`.", needToMove, FreeTotalMoves(col2));
		PromptBox(prompt, "", "ANY", FALSE);
		return(FALSE);
	}

	// move the cards (directly to the column to save time over the modem)

	// delete the cards in the moving from column from the screen
	for (i = GetLastFreeCard(col1 - '1'); i >= curCard; i--)
	{
//sprintf(prompt, "CurCard %d i %d", curCard, i);
//PromptBox(prompt,"","ANY",FALSE);
		// delete the card from the screen...
		DeleteSmallCard(COLUMN_X + X_OFFSET*(col1-'1'),
				ROW_Y + Y_OFFSET*i );
		// display the card under it...(Don't do this if we took the last card)
		if (i > 0)
			DisplaySmallCard( COLUMN_X + (col1-'1')*X_OFFSET, ROW_Y + (i-1)*Y_OFFSET,
				player.column[(col1-'1')][i-1]);
	}

	// copy the cards to the new column and display them there...
	for (i = curCard; i <= curCard + needToMove; i++)
	{
		if (player.column[col2-'1'][GetLastFreeCard(col2-'1')] == EMPTY)
			player.column[col2-'1'][GetLastFreeCard(col2-'1')] = 
				player.column[col1-'1'][i];
		else
			player.column[col2-'1'][GetLastFreeCard(col2-'1') + 1] = 
				player.column[col1-'1'][i];

		player.column[col1-'1'][i] = EMPTY;

		// display the card in column 2
		if (player.column[col2-'1'][GetLastFreeCard(col2-'1')] != EMPTY)
			DisplaySmallCard( COLUMN_X + (col2-'1')*X_OFFSET, ROW_Y + GetLastFreeCard((col2-'1'))*Y_OFFSET,
				player.column[(col2-'1')][GetLastFreeCard((col2-'1'))]);
	}
	// we moved the column...
	return(TRUE);
}

// This function translates the user's input to my predefined keys (in order)
char FreeKeys( char input)
{

	switch (input)
	{
		case 'A':
			return('A');
		case 'S':
			return('B');
		case 'D':
			return('C');
		case 'F':
			return('D');
		case 'Z':
			return('M');
		case 'X':
			return('N');
		case 'C':
			return('O');
		case 'V':
			return('P');
		default:
			return(input);
	}

}

char FreeDefinedKeys( char input)
{

	switch (input)
	{
		case 'A':
			return('A');
		case 'B':
			return('S');
		case 'C':
			return('D');
		case 'D':
			return('F');
		case 'M':
			return('Z');
		case 'N':
			return('X');
		case 'O':
			return('C');
		case 'P':
			return('V');
		default:
			return(input);
	}

}

