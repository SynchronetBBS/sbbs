// ===========================================================================
// Tournament Blackjack
// ===========================================================================

/*
   This game uses the Game SDK found in GAC_BJ\GAMESDK\*.*

   This file ONLY contains the functions necessary to play the game, all
   other functions that are similiar in all games and MUST be modified are
   in GAME_S.C
*/

/* Future additions should include a split screen to seperate the game lines
	 from the chat lines.  We should also have a prompt box for entering chat lines
	 and maybe add support for this players cards displayed as ANSIs

	 Create an InProcess and OutProcess function to process incoming and
	 outgoing information for this game.  The files should be in the OUT and
	 IN directories (respectively) and must have the form
	 <ibbsgametitle>_x.DAT or <ibbsgametitle>_x.<bbsnumber>
	 NOTE: x can be any character other than the following:
	R - Reserved for ROUTE file information
	N - Reserved for New RESET file information

*/

/*
	HISTORY:
		12/96 - Modified to use Inter-LORD IBBS Routines
		12/96 - Modified to show full size cards for player and dealer
		May be able to show dealer's cards next to the user's?  Will have
		to see how the multiplayer works.
*/


// ===========================================================================
// * Multiuser, Inter-BBS Blackjack game for all BBS systems *
// ===========================================================================
// Copyright 1995-1997 G.A.C. Computer Services

// Maximum bet is limited to an integer (<34466)

// Only let this main .C file specify the variables
//#define EXTDRIVER
#include "gac_bj.h"
//#undef EXTDRIVER


char    *UserSays="`Bright Magenta`%s `Magenta`says \"`Bright Magenta`%s`Magenta`\"\r\n";
char    *UserWhispers="`Bright Magenta`%s `Magenta`whispers \"`Bright Cyan`%s`Magenta`\"\r\n";
char    *ShoeStatus="\r\n`Bright White`Shoe: %u/%u\r\n";
// uchar   symbols=1;
char    autoplay=0;

card_t newdeck[52]={
	 {2,H}, {2,D}, {2,C}, {2,S},
	 {3,H}, {3,D}, {3,C}, {3,S},
	 {4,H}, {4,D}, {4,C}, {4,S},
	 {5,H}, {5,D}, {5,C}, {5,S},
	 {6,H}, {6,D}, {6,C}, {6,S},
	 {7,H}, {7,D}, {7,C}, {7,S},
	 {8,H}, {8,D}, {8,C}, {8,S},
	 {9,H}, {9,D}, {9,C}, {9,S},
	{10,H},{10,D},{10,C},{10,S},
	 {J,H}, {J,D}, {J,C}, {J,S},
	 {Q,H}, {Q,D}, {Q,C}, {Q,S},
	 {K,H}, {K,D}, {K,C}, {K,S},
	 {A,H}, {A,D}, {A,C}, {A,S} };



// ===========================================================================
// * This function is the actual game playing loop.                                                     *
// ===========================================================================
void play()
{
	//char log[81];
	static char str[256],str2[256],done,doub,dh
		,*YouWereDealt="`Bright magenta`You `magenta` were dealt: %s\r\n"
		,*UserWasDealt="`Bright cyan`%s`cyan` was dealt: %s\r\n"
		,*YourHand="`Bright cyan`You `cyan`                      (%2d) %s"
		,*UserHand="`Bright cyan`%-25s `cyan`(%2d) %s"
		,*DealerHand="`Bright white`Dealer                    `Magenta`(%2d) "
		,*Bust="`Bright red flashing` Bust``\n\r"
		,*Natural="`Bright green flashing` Natural"
		,*Three7s="`Bright red flashing` Three 7's"
		,*Blackjack="`Bright red` Blackjack!``\n\r"
		,*TwentyOne="`bright blue` Twenty-one``\n\r";
	INT16 h,i,j;
	//,file;
	long val;
	time_t start,now;

	// 12/96 For full sized cards
	currentTop = 1; //12/96
	currentLine=currentTop;

	sprintf(str,"chatline.%d",node_num);         ///  remove CHATLINE if waiting
	if(access(str, 00)==0)
		remove(str);

	getgamedat(0);
	if(node[node_num-1]) 
	{
		getgamedat(1);
		node[node_num-1]=0;
		putgamedat();
		getgamedat(0); 
	}

	if(total_players && misc&INPLAY) 
	{
		od_printf("\r\n`Bright White`Waiting for end of hand so you can join the current game in");
		UpdateCursor(); // 12/96    
		od_printf("\r\n`Bright White`progress.  (Use Ctrl-A to stop waiting and return)\n\r");
		UpdateCursor(); // 12/96    
		start=now=time(NULL);
		getgamedat(0);
		while(total_players && misc&INPLAY) 
		{
			if((i=od_get_key(FALSE))!=0) 
			{   // if key was hit
				if(i==1) 
				{               // if ctrl-a
					od_printf("\r\n");
					UpdateCursor(); // 12/96                
					return; 
				} 
			}  //  return
			od_sleep(100);
			getgamedat(0);
			now=time(NULL);
			if(now-start>300) 
			{ // only wait up to 5 minutes
				od_printf("\r\nTimed out!\r\n");
				UpdateCursor(); // 12/96            
				return; 
			} 
		}
		od_printf("\r\n"); 
		UpdateCursor(); // 12/96    
	}

	getgamedat(1);
	node[node_num-1]=user_number;
	putgamedat();

	if(!total_players)
		shuffle();
	else
		list_bj_players();

	// sprintf(str,"`Bright Magenta`%s `Magenta`%s\r\n",od_control.user_name,joined());
	sprintf(str,"`Bright Magenta`%s `Magenta`%s\r\n",player.names,joined());
	putallnodemsg(str);

	while(1) 
	{
		// Update the player information
		WritePlayerInfo(&player, player.account, TRUE);
		if(autoplay)
			lncntr=0;
		od_printf(ShoeStatus,cur_card,total_decks*52);
		UpdateCursor(); // 12/96    
		if(cur_card>(total_decks*52)-(total_players*10)-10 && lastplayer())
			shuffle();
		getgamedat(1);
		misc&=~INPLAY;
		status[node_num-1]=BET;
		node[node_num-1]=user_number;
		putgamedat();

		od_printf("`Bright Cyan`You have $`Cyan`%lu`Bright Cyan`."
			,player.money);
		if(player.money <((ulong)min_bet)) 
		{
			od_printf("\n\r`Bright Cyan`Minimum bet: $`Cyan`%u\r\n",min_bet);
			UpdateCursor(); // 12/96
			od_printf("`Bright cyan`Come back tomorrow when you will be given $`cyan`%u`bright cyan`.\r\n", dailymoney);
			UpdateCursor(); // 12/96
			gac_pause();
			UpdateCursor(); // 12/96        
	
			break; 
		}
		sprintf(str,"  `Bright cyan`Bet amount or `bright red`Q`bright cyan`uit [`cyan`%lu`bright cyan`]: `cyan`"
			,(ulong) ibet < player.money ? (ulong) ibet : player.money);
		chat();
		od_printf(str);
		UpdateCursor(); // 12/96    
		if(autoplay && od_get_key(1))
			autoplay=0;
		if(autoplay)
		{
			i = ibet;
			// I added
			bet[0] = ibet;
		}
		else
		{
			od_input_str(temp, 10, 32, 127);
			if (stricmp(temp, "") == 0)             // if user hit enter
			{
				bet[0]=ibet<((uint)player.money) ? ibet : (uint)player.money;
			}
			else            // if user entered a value
			{
				// Test for over maximum bet...
				if (atol(temp) > ((long) max_bet))
				{
					od_printf("`Bright Cyan`Maximum bet: $`Cyan`%u\r\n",max_bet);
					UpdateCursor(); // 12/96                
					od_printf("`cyan`Your bet has been adjusted to `bright cyan`%u`cyan`.\r\n", max_bet<((uint)player.money) ? max_bet : (uint)player.money);
					UpdateCursor(); // 12/96                
					bet[0]=max_bet<((uint)player.money) ? max_bet : (uint)player.money;
					gac_pause();
					UpdateCursor(); // 12/96        
				}
				else
				{
					bet[0]=atoi(temp)<((uint)player.money) ? atoi(temp) : (uint)player.money;
				}
			}
		}

		if(stricmp(temp, "Q")==0)       // if user hit 'Q'
			break;

		if(bet[0]<min_bet) 
		{
			od_printf("`Bright Cyan`Minimum bet: $`Cyan`%u\r\n",min_bet);
			UpdateCursor(); // 12/96        
			od_printf("`cyan`Your bet has been adjusted to `bright cyan`%u`cyan`.\r\n", min_bet);
			UpdateCursor(); // 12/96        
			bet[0] = min_bet;
			gac_pause();
			UpdateCursor(); // 12/96        
		}

		od_printf("\r\n");
		g_clr_scr();
		// 12/96 Based on whether the user has ANSI or not
		if (!od_control.user_ansi)
			currentTop = 1;     // 12/96
		else
			currentTop = 1+2+7;    // 12/96

		currentLine = currentTop;     // 12/96
		// 12/96 Start the od_printf out at the currentLine
		od_set_cursor(currentLine,1); // 12/96
		// I Added these lines
		if (total_players >1)
		{
			od_printf("`Bright White`While waiting for your turn hit a key to chat or '`bright cyan`/?`bright white`' for the command menu.\n\r");
			UpdateCursor(); // 12/96
		}

		ibet=bet[0];
		getgamedat(0);  // to get all new arrivals
		//sprintf(str,"`Bright Magenta`%s`Magenta` bet $`Bright white`%u\r\n",od_control.user_name,bet[0]);
		sprintf(str,"`Bright Magenta`%s`Magenta` bet $`Bright white`%u\r\n",player.names,bet[0]);
		putallnodemsg(str);

		pc[0]=2;                                                // init player's 1st hand to 2 cards
		for(i=1;i<MAX_HANDS;i++)                // init player's other hands to 0 cards
			pc[i]=0;
		hands=1;                                                // init total player's hands to 1

		getgamedat(1);                                  // first come first serve to be the
		for(i=0;i<total_nodes;i++)              //  dealer in control of sync */
		if(node[i] && status[i]==SYNC_D)
			break;
	if(i==total_nodes) {
		syncdealer();  }                        //  all players meet here */
	else {                                                  //  first player is current after here */
		syncplayer(); }                         //  game is closed (INPLAY) at this point */

	waitturn();
	getnodemsg();
										//  Initial deal card #1 */
	getcarddat();
	bjplayer[0][0]=card[cur_card++];
	putcarddat();
	// 12/96 Show full sized cards if user has ANSI
	sprintf(str,YouWereDealt,cardstr(card[cur_card-1]));
	//12/96 Set up the screen (start of each game)
	if (od_control.user_ansi)
	{
		// draw a box around where the cards go...
		od_printf("`cyan`");
		od_draw_box(1, 1, 75, currentTop-1);
		// display the title
		od_set_cursor(1,3);
		od_printf("`bright cyan` Your Cards ");
		od_set_cursor(1,(75-strlen(od_control.od_prog_name)+2)/2);
		od_printf("`bright white` %s ", od_control.od_prog_name);
		od_set_cursor(1,DEALER_X+2);
		od_printf("`bright cyan` Dealer Card ");

		// show the first user card
		DisplayCard( 2 + 4*0, 2, card[cur_card-1]);
		od_set_cursor(currentLine,1); // 12/96
	}
	
	if(!symbols)
		strip_symbols(str);
	// 12/96 Only say this, if the user has ASCII
	if (!od_control.user_ansi)
	{
		od_printf(str);
		UpdateCursor(); // 12/96
	}


//  sprintf(str,UserWasDealt,od_control.user_name,cardstr(card[cur_card-1]));
	sprintf(str,UserWasDealt,player.names,cardstr(card[cur_card-1]));
		putallnodemsg(str);

	if(lastplayer()) 
	{
		getcarddat();
		dealer[0]=card[cur_card++];
		// 12/96
		dc=1;
		putcarddat(); 
	}
	nextplayer();
	waitturn();
	getnodemsg();

	getcarddat();                                      //  Initial deal card #2 */
	bjplayer[0][1]=card[cur_card++];
	
	putcarddat();

	sprintf(str,YouWereDealt,cardstr(card[cur_card-1]));
	//12/96
	// display user card 2
	if (od_control.user_ansi)
	{
		DisplayCard( 2 + 4*1, 2, card[cur_card-1]);
		od_set_cursor(currentLine,1); // 12/96
	}
	
	if(!symbols)
		strip_symbols(str);
	// 12/96 Only say this, if the user has ASCII
	if (!od_control.user_ansi)
	{
		od_printf(str);
		UpdateCursor(); // 12/96
	}


//  sprintf(str,UserWasDealt,od_control.user_name,cardstr(card[cur_card-1]));
	sprintf(str,UserWasDealt,player.names,cardstr(card[cur_card-1]));
		putallnodemsg(str);

	if(lastplayer()) 
	{
		getcarddat();
		dealer[1]=card[cur_card++];
		dc=2;
		putcarddat(); 
		if (od_control.user_ansi)
		{
			// SHOULD ONLY DISPLAY A BACKGROUND HERE!!!            
			DisplayCardBack( DEALER_X, 2);
			od_set_cursor(currentLine,1); // 12/96
			// 12/96 Display dealer's visible card
			DisplayCard( DEALER_X + 4*(dc-1), 2, dealer[dc-1]);
			od_set_cursor(currentLine,1); // 12/96
		}

	}
	nextplayer();
	waitturn();
	getnodemsg();
	getcarddat();

	// 12/96 Display to other people other than last player also
	// dealer's card 2
	if (!lastplayer())
	{
		if (od_control.user_ansi)
		{
			// SHOULD ONLY DISPLAY A BACKGROUND HERE!!!            
			DisplayCardBack( DEALER_X, 2);
			od_set_cursor(currentLine,1); // 12/96
			// display dealer's visible card
			DisplayCard( DEALER_X + 4*(dc-1), 2, dealer[dc-1]);
			od_set_cursor(currentLine,1); // 12/96
		}
	}
	for(i=0;i<hands;i++) {
		if(autoplay)
			lncntr=0;
		done=doub=0;
		while(!done && pc[i]<MAX_CARDS && cur_card<total_decks*52) {
			h=hand(bjplayer[i],pc[i]);
			str[0]=0;
			for(j=0;j<pc[i];j++) {
				strcat(str,cardstr(bjplayer[i][j]));
				strcat(str," "); }
			j=strlen(str);
			while(j++<19)
				strcat(str," ");
			if(h>21) {
				strcat(str,Bust);
				sprintf(str2,YourHand,h,str);
				if(!symbols)
					strip_symbols(str2);
				od_printf(str2);
UpdateCursor(); // 12/96

//              sprintf(str2,UserHand,od_control.user_name,h,str);
				sprintf(str2,UserHand,player.names,h,str);
				putallnodemsg(str2);
				break; }
			if(h==21) {
				if(pc[i]==2) {  //  blackjack */
					if(bjplayer[i][0].suit==bjplayer[i][1].suit)
						strcat(str,Natural);
					strcat(str,Blackjack); }
				else {
					if(bjplayer[i][0].value==7
						&& bjplayer[i][1].value==7
						&& bjplayer[i][2].value==7)
						strcat(str,Three7s);
					strcat(str,TwentyOne); }
				sprintf(str2,YourHand,h,str);
				if(!symbols)
					strip_symbols(str2);
				od_printf(str2);
UpdateCursor(); // 12/96

//              sprintf(str2,UserHand,od_control.user_name,h,str);
				sprintf(str2,UserHand,player.names,h,str);
		putallnodemsg(str2);
				// fdelay(500);
				break; }
			strcat(str,"\r\n");
			sprintf(str2,YourHand,h,str);
			if(!symbols)
				strip_symbols(str2);
			od_printf(str2);
			UpdateCursor(); // 12/96

//          sprintf(str2,UserHand,od_control.user_name,h,str);
			sprintf(str2,UserHand,player.names,h,str);
			putallnodemsg(str2);
			if(doub)
				break;
			sprintf(str,"`Bright white`Dealer`Magenta` card up: %s\r\n"
				,cardstr(dealer[1]));
			if(!symbols)
				strip_symbols(str);
			// 12/96
			/*
			if (od_control.user_ansi)
			{
				DisplayCard( DEALER_X + 4*(dc-1), 2, dealer[dc-1]);
				od_set_cursor(currentLine,1); // 12/96
			}
			*/
			// 12/96 display so blind people can still play
			//else
			//{
				od_printf(str);
				UpdateCursor(); // 12/96
			//}

			strcpy(str,"\r\n`Bright cyan`H`cyan`it");
			strcpy(tmp,"H\r");
//      if(bet[i]+ibet<= player.money && pc[i]==2) {
			if(bet[i]+ibet<= (uint) player.money && pc[i]==2) {
				strcat(str,", `bright cyan`D`cyan`ouble");
				strcat(tmp,"D"); }
//      if(bet[i]+ibet<=player.money && pc[i]==2 && hands<MAX_HANDS
			if(bet[i]+ibet<= (uint) player.money && pc[i]==2 && hands<MAX_HANDS
				&& bjplayer[i][0].value==bjplayer[i][1].value) {
				strcat(str,", `bright cyan`S`cyan`plit");
				strcat(tmp,"S"); }
			strcat(str,", or [`bright cyan`Stand`cyan`]: ");
			chat();
			od_printf(str);

			if(autoplay && od_get_key(1))
				autoplay=0;
			if(autoplay) {
				lncntr=0;
				od_printf("\r\n");
UpdateCursor(); // 12/96

				strcpy(str,stand());
				od_printf(str);
UpdateCursor(); // 12/96

				putallnodemsg(str);
				done=1; }
			else
			switch(od_get_answer(tmp)) 
			{
				case 'H':     //  hit
					od_printf("\r\n");
					UpdateCursor(); // 12/96
					strcpy(str,hit());
					od_printf(str);
					UpdateCursor(); // 12/96
					UpdateCursor();

					putallnodemsg(str);
					getcarddat();
					bjplayer[i][(int)pc[i]++]=card[cur_card++];
					putcarddat();
					//12/96
					if (od_control.user_ansi)
					{
						DisplayCard( 2 + 4*(pc[i]-1), 2 + 2*i, card[cur_card-1]);
						od_set_cursor(currentLine,1); // 12/96
					}

					break;
				case 'D':   //  double down 
					od_printf("\r\n");
					UpdateCursor(); // 12/96
					strcpy(str,doubit());
					od_printf(str);
					UpdateCursor(); // 12/96
					UpdateCursor();

					putallnodemsg(str);
					getcarddat();
					bjplayer[i][(int)pc[i]++]=card[cur_card++];
					putcarddat();
					//12/96
					if (od_control.user_ansi)
					{
						DisplayCard( 2 + 4*(pc[i]-1), 2 + 2*i, card[cur_card-1]);
						od_set_cursor(currentLine,1); // 12/96
					}
					doub=1;
					bet[i]+=ibet;
					break;
				case 'S':   //  split */
					od_printf("\r\n");
					UpdateCursor(); // 12/96
					strcpy(str,split());
					od_printf(str);
					UpdateCursor(); // 12/96
					
					// 12/96 lower the current chat ceiling each time the user
					// splits the cards
					if (od_control.user_ansi)
					{
					
						// get the text...
						od_gettext(1,1,75,currentTop-2, screen);
						// increment the top
						currentTop+=2;
						// draw a box
						od_printf("`cyan`");
						od_draw_box(1, 1, 75, currentTop-1);
						// put the text back...
						od_puttext(1,1,75,currentTop-4, screen);

						// erase the next three lines
//                        od_set_cursor(currentTop-1,1);
//                        od_clr_line();
///                        od_set_cursor(currentTop,1);
//                        od_clr_line();
//                        od_set_cursor(currentTop+1,1);
//                        od_clr_line();
						
						od_set_cursor(currentLine,1);
					}

					putallnodemsg(str);
					bjplayer[hands][0]=bjplayer[i][1];
					getcarddat();
					bjplayer[i][1]=card[cur_card++];
					//12/96
					if (od_control.user_ansi)
					{
						DisplayCard( 2 + 4*(pc[i]-1), 2+2*i, card[cur_card-1]);
						od_set_cursor(currentLine,1); // 12/96
					}
					pc[hands]=1;
					// 12/96 display split card as part of second hand
					if (od_control.user_ansi)
					{
						DisplayCard( 2 + 4*(pc[hands]-1), 2+2*hands, bjplayer[hands][0]);
						od_set_cursor(currentLine,1); // 12/96
					}
					
					bjplayer[hands][1]=card[cur_card++];
					pc[hands]=2;
					//12/96
					if (od_control.user_ansi)
					{
						DisplayCard( 2 + 4*(pc[hands]-1), 2+2*hands, card[cur_card-1]);
						od_set_cursor(currentLine,1); // 12/96
					}
					
					putcarddat();
					bet[hands]=ibet;
					hands++;
					break;
				case CR:
					od_printf("\r\n");
UpdateCursor(); // 12/96
					strcpy(str,stand());
					od_printf(str);
UpdateCursor(); // 12/96

					putallnodemsg(str);
					done=1;
					break; } } }

	if(lastplayer()) 
	{      //  last player plays the dealer's hand */
		getcarddat();
// Should check the dealers hands to those playing and see if he is higher...

		while(hand(dealer,dc)<17 && dc<MAX_CARDS && cur_card<total_decks*52)
			dealer[dc++]=card[cur_card++];
		putcarddat(); 
	}

	nextplayer();
	if (!lastplayer()) 
		waitturn();

	if(firstplayer()==node_num) 
	{
		// I moved this so I can clear the screen for the final standings correctly
		getnodemsg();
		gac_pause();
		UpdateCursor(); // 12/96        
		g_clr_scr();
		// 12/96 Based on whether the user has ANSI or not
		if (!od_control.user_ansi)
			currentTop = 1;     // 12/96
		else
			currentTop = FINAL_Y+7+2*(hands-1);    // 12/96
		currentLine=currentTop;     // 12/96
		od_set_cursor(currentLine,1); // 12/96
		// Place a cool ANSI next to the menu. if the user has ANSI
		if (od_control.user_ansi || od_control.user_rip) 
		{
			g_send_file("gac_2");
			od_printf("\r\n\r\n"); 
			od_set_cursor(currentLine,1); // 12/96

		}
		else
		{
			strcpy(str,"\r\n`cyan`          ***     `bright white`Final Standings`cyan`     ***\n\r\r\n");
			od_printf(str);
			UpdateCursor(); // 12/96
		}
	 }
	else 
	{
		gac_pause();
		UpdateCursor(); // 12/96        
		g_clr_scr();
		if (!od_control.user_ansi)
			currentTop = 1;     // 12/96
		else
			currentTop = FINAL_Y+7+2*(hands-1);    // 12/96
		
		currentLine=currentTop;     // 12/96
		od_set_cursor(currentLine,1); // 12/96
		// Place a cool ANSI next to the menu. if the user has ANSI
		if (od_control.user_ansi || od_control.user_rip) 
		{
			g_send_file("gac_2");
			od_printf("\n\r"); 
			od_set_cursor(currentLine,1); // 12/96
		}
		else
		{
			strcpy(str,"\n\r`cyan`          ***     `bright white`Final Standings`cyan`     ***\n\r\r\n");
			od_printf(str);
			UpdateCursor(); // 12/96
		}
		if (lastplayer()) waitturn();
		// I moved this so I can clear the screen for the final standings correctly
		getnodemsg();
	}
	getcarddat();
	dh=hand(dealer,dc);                                     //  display dealer's hand */
	sprintf(str,DealerHand,dh);
	if (od_control.user_ansi)
	{
		// display the title
		od_set_cursor(FINAL_Y,1);
		od_printf("`bright cyan`Y");
		od_set_cursor(FINAL_Y+1,1);
		od_printf("`bright cyan`O");
		od_set_cursor(FINAL_Y+2,1);
		od_printf("`bright cyan`U");
		
		od_set_cursor(FINAL_Y,DEALER_X2-1);
		od_printf("`bright cyan`D");
		od_set_cursor(FINAL_Y+1,DEALER_X2-1);
		od_printf("`bright cyan`E");
		od_set_cursor(FINAL_Y+2,DEALER_X2-1);
		od_printf("`bright cyan`A");
		od_set_cursor(FINAL_Y+3,DEALER_X2-1);
		od_printf("`bright cyan`L");
		od_set_cursor(FINAL_Y+4,DEALER_X2-1);
		od_printf("`bright cyan`E");
		od_set_cursor(FINAL_Y+5,DEALER_X2-1);
		od_printf("`bright cyan`R");


	}
	for(i=0;i<dc;i++) 
	{
		strcat(str,cardstr(dealer[i]));
		strcat(str," "); 
		if (od_control.user_ansi)
		{
			DisplayCard( DEALER_X2 + 3*i, FINAL_Y, dealer[i]);
			od_set_cursor(currentLine,1); // 12/96
		}
	}
	i=strlen(str);
	while(i++<50)
		strcat(str," ");
	if(dh>21) 
	{
		strcat(str,Bust);
		if(!symbols)
			strip_symbols(str);
		od_printf(str);
		UpdateCursor(); // 12/96

	}
	else if(dh==21) 
	{
		if(dc==2) 
		{     //  blackjack */
			if(dealer[0].suit==dealer[1].suit)
				strcat(str,Natural);
			strcat(str,Blackjack); 
		}
		else 
		{                  //  twenty-one */
			if(dc==3 && dealer[0].value==7 && dealer[1].value==7
				&& dealer[2].value==7)
				strcat(str,Three7s);
			strcat(str,TwentyOne); 
		}
		if(!symbols)
			strip_symbols(str);
		od_printf(str); 
		UpdateCursor(); // 12/96
	}
	
	else 
	{
		if(!symbols)
			strip_symbols(str);
		od_printf("%s\r\n",str); 
		UpdateCursor(); // 12/96        
	}


	// draw a box around where the cards go...
	
	for(i=0;i<hands;i++) {                                          //  display player's hand(s) */
		h=hand(bjplayer[i],pc[i]);
		str[0]=0;
		for(j=0;j<pc[i];j++) {
			strcat(str,cardstr(bjplayer[i][j]));
		//12/96
		if (od_control.user_ansi)
		{
			DisplayCard( 2 + 4*j, FINAL_Y+2*i, bjplayer[i][j]);
			od_set_cursor(currentLine,1); // 12/96
		}
			
			strcat(str," "); }
		j=strlen(str);
		while(j++<19)
			strcat(str," ");

		if(h<22 && (h>dh || dh>21       //  player won */
			|| (h==21 && pc[i]==2 && dh==21 && dh>2))) {    //  blackjack */
			j=bet[i];                                                                 //  and dealer got 21 */
			if(h==21 &&     //  natural blackjack or three 7's */
				((bjplayer[i][0].value==7 && bjplayer[i][1].value==7
				&& bjplayer[i][2].value==7)
				|| (pc[i]==2 && bjplayer[i][0].suit==bjplayer[i][1].suit)))
				j*=2;
			else if(h==21 && pc[i]==2)      //  regular blackjack */
// Possible problem here...
				j*=1.5; //  blackjack pays 1« to 1 */
			sprintf(tmp,"`Bright Magenta` Won! `bright yellow`$`Bright white`%u",j);
			strcat(str,tmp);
			player.money+=j;
			val-=j;
			//moduserdat();
			 }
		else if(h<22 && h==dh)
			strcat(str," `Bright white`Push");
		else 
		{
			strcat(str," `bright red`Lost");
			player.money-=bet[i];
			val+=bet[i];
		}
		strcat(str,"\r\n");
		sprintf(str2,YourHand,h,str);
		if(!symbols)
			strip_symbols(str2);
		od_printf(str2);
		UpdateCursor(); // 12/96

		// sprintf(str2,UserHand,od_control.user_name,h,str);
		sprintf(str2,UserHand,player.names,h,str);
		putallnodemsg(str2); }

		nextplayer();
		if(!lastplayer()) 
		{
			waitturn();
			nextplayer(); 
		}
		getnodemsg(); 
		
		gac_pause();        // 12/96
		UpdateCursor(); // 12/96
	}

	getgamedat(1);
	node[node_num-1]=0;
	putgamedat();
	//sprintf(str,"`Bright Magenta`%s `Magenta`%s\r\n",od_control.user_name,left());
	sprintf(str,"`Bright Magenta`%s `Magenta`%s\r\n",player.names,left());
	putallnodemsg(str);

}

// ===========================================================================
//  This function returns a static string that describes the status byte
// ===========================================================================
char *activity(char status_type)
{
	static char str[50];

switch(status_type) {
	case BET:
		strcpy(str,"betting");
		break;
	case WAIT:
		strcpy(str,"waiting for turn");
		break;
	case PLAY:
		strcpy(str,"playing");
		break;
	case SYNC_P:
		strcpy(str,"synchronizing");
		break;
	case SYNC_D:
		strcpy(str,"synchronizing (dealer)");
	break;
	default:
		strcat(str,"UNKNOWN");
		break; }
return(str);
}

// ===========================================================================
//  This function returns the string that represents a playing card.
// ===========================================================================
char *cardstr(card_t card)
{
	static char str[20];
	char tmp[20];

strcpy(str,""); //  card color - background always white */
if (card.value == 10)
{
	if(card.suit==H || card.suit==D)
		strcat(str,"`red on white`");  //  hearts and diamonds - foreground red */
	else
		strcat(str,"`black on white`");  //  spades and clubs - foreground black */
}
else
{
	if(card.suit==H || card.suit==D)
		strcat(str," `red on white`");  //  hearts and diamonds - foreground red */
	else
		strcat(str," `black on white`");  //  spades and clubs - foreground black */
}

if(card.value>10)       //  face card */
	switch(card.value) {
		case J:
			strcat(str,"J");
			break;
		case Q:
			strcat(str,"Q");
			break;
		case K:
			strcat(str,"K");
			break;
		case A:
			strcat(str,"A");
			break; }
else {

	sprintf(tmp,"%d",card.value);
	strcat(str,tmp); }

switch(card.suit) {  //  suit */
	case H:
		strcat(str,"\3");
		break;
	case D:
		strcat(str,"\4");
		break;
	case C:
		strcat(str,"\5");
		break;
	case S:
		strcat(str,"\6");
		break; }
strcat(str,"``");
return(str);
}


// ===========================================================================
//  This function returns the best value of a given hand.                                       */
// ===========================================================================
char hand(card_t card[MAX_CARDS],char count)
{
	int c;
	char total=0,ace=0;

for(c=0;c<count;c++) {
	if(card[c].value==A) {          //  Ace */
		if(total+11>21)
			total++;
		else {
			ace=1;
			total+=11; } }
	else if(card[c].value>=J)       //  Jack, Queen, King */
		total+=10;
	else                                            //  Number cards */
		total+=card[c].value; }
if(total>21 && ace)     //  ace is low if bust */
	total-=10;
return(total);
}


// ===========================================================================
//  This function shuffles the deck.                                                                            */
// ===========================================================================
void shuffle()
{
	char str[81];
	uint i,j;
	card_t shufdeck[52*MAX_DECKS];


getcarddat();

sprintf(str,"`Bright White`\r\nShuffling %d Deck Shoe...",total_decks);
od_printf(str);
UpdateCursor(); // 12/96
strcat(str,"\r\n");     //  add crlf for other nodes */
putallnodemsg(str);

for(i=0;i<total_decks;i++)
	memcpy(shufdeck+(i*52),newdeck,sizeof(newdeck));          //  fresh decks */

i=0;
// I added type declaration
while(i<(((int)total_decks)*52)-1) {
	 j = g_rand((total_decks*52) - 1);
	if(!shufdeck[j].value)  //  card already used */
		continue;
	card[i]=shufdeck[j];
	shufdeck[j].value=0;    //  mark card as used */
	i++; }

cur_card=0;
for(i=0;i<MAX_HANDS;i++)
	pc[i]=0;
hands=0;
dc=0;
putcarddat();
od_printf("\r\n");
UpdateCursor(); // 12/96

}

// ===========================================================================
//  This function reads and displays a message waiting for this node, if
//  there is one.                                                                                                                       */
// ===========================================================================
void getnodemsg()
{
	char str[81], *buf, *p;
	int file;
	ulong length;

sprintf(str,"chatline.%d",node_num);
//if(flength(str)<1L)                                   //  v1.02 fix */
file = nopen(str,O_RDONLY);
if(filelength(file)<1L)                                         //  v1.02 fix */
	{
		close(file);
		return;
	}
// I added this line
close(file);
if((file=nopen(str,O_RDWR))==-1) {
	od_printf("Couldn't open %s\r\n",str);
UpdateCursor(); // 12/96    
	return; }
length=filelength(file);
// possible problem
if((buf=malloc(length+1L))==NULL) {
	close(file);
	od_printf("`Bright red`\r\nError allocating %lu bytes of memory for %s\r\n"
		,length+1L,str);
UpdateCursor(); // 12/96    
od_log_write("Error allocating memory for chat lines");
	return; }
buf[read(file,buf,length)]=0;
chsize(file,0);
close(file);
if(!symbols)
	strip_symbols(buf);

// loop while we can read tokens from the buffer
p = strtok(buf, "~");
od_printf("%s", p);
UpdateCursor(); // 12/96    
while ( (p = strtok(NULL, "~")) != NULL)
{
	od_printf("%s", p);
UpdateCursor(); // 12/96    
}
// od_printf(buf);
free(buf);



}

// ===========================================================================
//  This function creates a message for a certain node.                                         */
// ===========================================================================
void putnodemsg(char *msg, char nodenumber)
{
	char str[81];
	int file;

sprintf(str,"chatline.%d",nodenumber);
if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
	od_printf("`Bright red`\r\nError opening/creating %s\r\n",str);
UpdateCursor(); // 12/96
	od_log_write("Error opening/creating chat file");
	return; }
write(file,msg,strlen(msg));
// Write a token to the file
str[0] = '~';
write(file,str,1);

close(file);
}

// ===========================================================================
//  This function creates a message for all nodes in the game.                          */
// ===========================================================================
void putallnodemsg(char *msg)
{
	INT16 i;

for(i=0;i<total_nodes;i++)
	if(node[i] && (i+1!=node_num))
		putnodemsg(msg,i+1);
}

// ===========================================================================
//  This function waits until it is the current player.                                         */
// ===========================================================================
void waitturn()
{
	time_t start,now;
start=now=time(NULL);
getgamedat(1);
status[node_num-1]=WAIT;
putgamedat();
while(curplayer!=node_num) {
	chat();
	od_sleep(100);
	getgamedat(0);
	if(curplayer && !node[curplayer-1] ) //   || status[curplayer-1]==BET */ )
		nextplayer();           //  current player is not playing? */

	if(!node[node_num-1]) { //  current node not in game? */
		getgamedat(1);
		node[node_num-1]=user_number;   //  fix it */
		putgamedat(); }

	now=time(NULL);
	if(now-start>300) { //  only wait upto 5 minutes */
		od_printf("\r\nwaitturn: timeout\r\n");
UpdateCursor(); // 12/96
		break; } }
getgamedat(1);
status[node_num-1]=PLAY;
putgamedat();
}

// ===========================================================================
//  This is the function that is called to see if the user has hit a key,
//  and if so, read in a line of chars and send them to the other nodes.
// ===========================================================================
void chat()
{
	char str1[150],str2[256],ch;
	INT16 i;
	char filename[128], temp[15];

if((ch=od_get_key(FALSE))!=0 ) {
	if(ch=='/') {
		//od_printf("`Bright Yellow`Command: ``");
		od_printf("`Bright cyan`W`cyan`)hisper, `bright cyan`L`cyan`)ist players, `bright cyan`?`cyan`) Help: ``");
UpdateCursor(); // 12/96
		switch(od_get_answer("?LS|%\r")) {
			case CR:
				od_printf("\n\r");
UpdateCursor(); // 12/96
				return;
			#if DEBUG
			case '|':
				debug();
				return;
			#endif
			case '%':
				if(!com_port)   //  only if local */
				{
					od_exit(0, FALSE);
				}
				break;
			case '?':
				od_printf("\r\n`bright cyan`?`cyan` This help screen");
UpdateCursor(); // 12/96
				od_printf("\r\n`bright cyan`L`cyan`ist players in current game");
UpdateCursor(); // 12/96
				od_printf("\r\n`bright cyan`W`cyan`hisper to another player");
UpdateCursor(); // 12/96
				od_printf("\r\n");
UpdateCursor(); // 12/96
				return;
			case 'L':
				list_bj_players();
				od_printf(ShoeStatus,cur_card,total_decks*52);
UpdateCursor(); // 12/96
				return;
			case 'S':
			case 'W':
// 12/96 May need to save and restore screen here
				list_bj_players();
				od_printf("``\r\n`Bright Yellow`Which node: `bright white`");
				od_input_str(temp, 10, '0', '9');
				if (stricmp(temp, "") == 0) i = sys_nodes;
				else i=atoi(temp);

				getgamedat(0);
				// set the filename to the ONLINE.[NODE]
				sprintf(filename, "%sONLINE%d", doorpath, i);
				if((i>0) && (i!=node_num) && (node[i-1]!=0)) {
					od_printf("\r\n`Bright Yellow`Message: `bright white`");
					od_input_str(str1, 50, 32, 127);
//                      sprintf(str2,UserWhispers,od_control.user_name
						sprintf(str2,UserWhispers,player.names
							,str1);
						putnodemsg(str2,i); } //}
				else
				{
					od_printf("``\r\n`Bright Red`Invalid node.``\r\n");
UpdateCursor(); // 12/96                
				}
				return; } }
	// Probably need this line since the first chat letter will disappear, use od_edit_str later
	// ungetkey(ch);

	// my equivalent to the ungetkey
	od_printf("`Bright Blue`Enter chat line: `bright cyan`");
	od_input_str(str1,50,32, 127);
	if(stricmp(str1, "") == 0)
		return;
//  sprintf(str2,UserSays,od_control.user_name,str1);
	sprintf(str2,UserSays,player.names,str1);
	putallnodemsg(str2); }
getnodemsg();
}

// ===========================================================================
//  This function returns 1 if the current node is the highest (or last)
//  node in the game, or 0 if there is another node with a higher number
//  in the game. Used to determine if this node is to perform the dealer
//  function                                                                                                                      */
// ===========================================================================
char lastplayer()
{
	INT16 i;

	getgamedat(0);
	if(total_players==1 && node[node_num-1]) //  if only player, definetly */
		return(1);                           //  the last */

	for(i=node_num;i<total_nodes;i++)        //  look for a higher number */
		if(node[i])
			break;
	if(i<total_nodes)                        //  if one found, return 0 */
		return(0);
	return(1);                               //  else return 1 */
}

// ===========================================================================
//  Returns the node number of the lower player in the game                                     */
// ===========================================================================
char firstplayer()
{
	INT16 i;

for(i=0;i<total_nodes;i++)
	if(node[i])
		break;
if(i==total_nodes)
	return(0);
return(i+1);
}

// ===========================================================================
//  This function is only run on the highest node number in the game. It
//  waits until all other nodes are waiting in their sync routines, and then
//  releases them by changing the status byte from SYNC_P to PLAY                       */
//  it is assumed that getgamedat(1) is called immediately prior.                       */
// ===========================================================================
void syncdealer()
{
	char *Dealing="`Bright white`Dealing...\r\n``";
	INT16 i;
	time_t start,now;

status[node_num-1]=SYNC_D;
putgamedat();
start=now=time(NULL);
//mswait(1000);                                 //  wait for stragglers to join game v1.02 */
getgamedat(0);
while(total_players) {
	for(i=0;i<total_nodes;i++)
		if(i!=node_num-1 && node[i] && status[i]!=SYNC_P)
			break;
	if(i==total_nodes)                //  all player nodes are waiting */
		break;
	chat();
	od_sleep(100);
	getgamedat(0);
	if(!node[node_num-1]) { //  current node not in game? */
		getgamedat(1);
		node[node_num-1]=user_number;   //  fix it */
		putgamedat(); }
	now=time(NULL);
	if(now-start>300) { //  only wait upto 5 minutes */
		od_printf("\r\nsyncdealer: timeout\r\n");
	break; } }

getgamedat(1);
misc|=INPLAY;
curplayer=firstplayer();
putgamedat();

getnodemsg();
od_printf(Dealing);
putallnodemsg(Dealing);

getgamedat(1);
for(i=0;i<total_nodes;i++)                //  release player nodes */
	if(node[i])
		status[i]=PLAY;
putgamedat();
}


// ===========================================================================
//  This function halts this node until the dealer releases it by changing
//  the status byte from SYNC_P to PLAY                                                                         */
//  it is assumed that getgamedat(1) is called immediately prior.                       */
// ===========================================================================
void syncplayer()
{
	time_t start,now;

status[node_num-1]=SYNC_P;
putgamedat();
start=now=time(NULL);
while(node[node_num-1] && status[node_num-1]==SYNC_P) {
	chat();
	od_sleep(100);
	getgamedat(0);
	if(!node[node_num-1]) { //  current node not in game? */
		getgamedat(1);
		node[node_num-1]=user_number;   //  fix it */
		putgamedat(); }
	now=time(NULL);
	if(now-start>300) { //  only wait upto 5 minutes */
		od_printf("\r\nsyncplayer: timeout\r\n");
	break; } }
}

// ===========================================================================
//  This function reads the entire shoe of cards and the dealer's hand from  */
//  the card database file (GAC_BJ.CRD)                                                                         */
// ===========================================================================
void getcarddat()
{
	int file;

	if((file=nopen("gac_bj.crd",O_RDONLY))==-1)
	{
		od_printf("\n\r`bright red`Error opening gac_bj.crd\r\n");
		od_log_write("Error opening gac_bj.crd");
		return;
	}
	read(file,&dc,1);
	read(file,dealer,sizeof(dealer));
	read(file,&total_decks,1);
	cur_card=LE_SHORT(cur_card);
	read(file,&cur_card,2);
	cur_card=LE_SHORT(cur_card);
	read(file,card,total_decks*52*sizeof(card_t));
	close(file);
}

// ===========================================================================
//  This function writes the entire shoe of cards and the dealer's hand to
//  the card database file (GAC_BJ.CRD)                                                                         */
// ===========================================================================
void putcarddat()
{
	int file;

	if((file=nopen("gac_bj.crd",O_WRONLY|O_CREAT))==-1)
	{
		od_printf("\n\r`bright red`Error opening gac_bj.crd\r\n");
		od_log_write("Error opening gac_bj.crd");
		return;
	}
	write(file,&dc,1);
	write(file,dealer,sizeof(dealer));
	write(file,&total_decks,1);
	cur_card=LE_SHORT(cur_card);
	write(file,&cur_card,2);
	cur_card=LE_SHORT(cur_card);
	write(file,card,total_decks*52*sizeof(card_t));
	close(file);
}

// ===========================================================================
//  This function creates random ways to say "hit"
// ===========================================================================
char *hit()
{
	static char str[81];

	strcpy(str,"`bright red`");
	switch(g_rand(10))
	{
		case 1:
			strcat(str,"Hit me.");
			break;
		case 2:
			strcat(str,"Hit it.");
			break;
		case 3:
			strcat(str,"Give me another.");
			break;
		case 4:
			strcat(str,"More..");
			break;
		case 5:
			strcat(str,"Just one more.");
			break;
		case 6:
			strcat(str,"Give me a small one.");
			break;
		case 7:
			strcat(str,"Hit it, and hurry.");
			break;
		case 8:
			strcat(str,"Another...");
			break;
		case 9:
			strcat(str,"Um... Hit.");
			break;
		case 10:
			strcat(str,"May I have another.");
			break;
		default:
			strcat(str,"Gi'me what I want.");
			break;
	}
	strcat(str,"\r\n");
	return(str);
}

// ===========================================================================
//  This function creates random ways to say "double"
// ===========================================================================
char *doubit()
{
	static char str[81];

	strcpy(str,"`bright blue`");
	switch(g_rand(10))
	{
		case 1:
			strcat(str,"Double.");
			break;
		case 2:
			strcat(str,"Double down.");
			break;
		case 3:
			strcat(str,"Double the bet and hit me!");
			break;
		case 4:
			strcat(str,"Another card and double the dough.");
			break;
		case 5:
			strcat(str,"Double me.");
			break;
		case 6:
			strcat(str,"All right... Double!");
			break;
		case 7:
			strcat(str,"Here goes nothing... Double!");
			break;
		case 8:
			strcat(str,"Double my bet and give me one more card.");
			break;
		case 9:
			strcat(str,"Hmmm... Double it.");
			break;
		case 10:
			strcat(str,"Make me glad to double?");
			break;
		default:
			strcat(str,"Double :)");
			break;
	}
	strcat(str,"\r\n");
	return(str);
}

// ===========================================================================
//  This function creates random ways to say "stand"
// ===========================================================================
char *stand()
{
	static char str[81];

	strcpy(str,"`Bright Cyan`");
	switch(g_rand(10))
	{
		case 1:
			strcat(str,"Stand.");
			break;
		case 2:
			strcat(str,"I'll stay.");
			break;
		case 3:
			strcat(str,"That's enough.");
			break;
		case 4:
			strcat(str,"Just right.");
			break;
		case 5:
			strcat(str,"I'm not going to take another.");
			break;
		case 6:
			strcat(str,"Wait, no more!");
			break;
		case 7:
			strcat(str,"Hold it.");
			break;
		case 8:
			strcat(str,"No way!");
			break;
		case 9:
			strcat(str,"Um... Stand.");
			break;
		case 10:
			strcat(str,"No thanks.");
			break;
		default:
			strcat(str,"Enough already.");
			break;
	}
	strcat(str,"\r\n");
	return(str);
}

// ===========================================================================
//  This function creates random ways to say "split"
// ===========================================================================
char *split()
{
	static char str[81];

	strcpy(str,"`Bright Yellow`");
	switch(g_rand(10))
	{
		case 1:
			strcat(str,"Split.");
			break;
		case 2:
			strcat(str,"Split 'em.");
			break;
		case 3:
			strcat(str,"Split it.");
			break;
		case 4:
			strcat(str,"Split, please.");
			break;
		case 5:
			strcat(str,"I'm gonna split instead of hitting.");
			break;
		case 6:
			strcat(str,"Wait! Split 'em...");
			break;
		case 7:
			strcat(str,"Split mine.");
			break;
		case 8:
			strcat(str,"Double the cards, for double the money.");
			break;
		case 9:
			strcat(str,"Um... Split.");
			break;
		case 10:
			strcat(str,"I think I want to split them.");
			break;
		default:
			strcat(str,"Split...Split!");
			break;
	}
	strcat(str,"\r\n");
	return(str);
}

// ===========================================================================
//  This function creates random ways to say "joined"
// ===========================================================================
char *joined()
{
	static char str[81];

switch(g_rand(10))
{
	case 1:
		strcpy(str,"joined.");
		break;
	case 2:
		strcpy(str,"sat down next to you.");
		break;
	case 3:
		strcpy(str,"pulled up a chair.");
		break;
	case 4:
		strcpy(str,"begged to join.");
		break;
	case 5:
		strcpy(str,"dropped in.");
		break;
	case 6:
		strcpy(str,"joined our game.");
		break;
	case 7:
		strcpy(str,"staggered towards the table...and plopped down!");
		break;
	case 8:
		strcpy(str,"slams a roll of cash on the table.");
		break;
	case 9:
		strcpy(str,"joined the game.");
		break;
	case 10:
		strcpy(str,"asks to join, while sitting down.");
		break;
	default:
		strcpy(str,"walks up bragging about last time.");
		break;
}
return(str);
}

// ===========================================================================
//  This function creates random ways to say "left"
// ===========================================================================
char *left()
{
	static char str[81];

switch(g_rand(10))
{
	case 1:
		strcpy(str,"left abruptly.");
		break;
	case 2:
		strcpy(str,"sneaked away.");
		break;
	case 3:
		strcpy(str,"took the money and ran.");
		break;
	case 4:
		strcpy(str,"ran out the door.");
		break;
	case 5:
		strcpy(str,"left the game.");
		break;
	case 6:
		strcpy(str,"slipped out the backway.");
		break;
	case 7:
		strcpy(str,"chuckled on the way out.");
		break;
	case 8:
		strcpy(str,"left wondering what happened.");
		break;
	case 9:
		strcpy(str,"decided to cut and run.");
		break;
	case 10:
		strcpy(str,"felt the time was up.");
		break;
	default:
		strcpy(str,"left with everything...");
		break;
}
return(str);
}

// ===========================================================================
//  This function creates the file "GAC_BJ.DAT" in the current directory.
// ===========================================================================
void create_gamedab()
{

	if((gamedab=sopen("gac_bj.dat"
		,O_WRONLY|O_CREAT|O_BINARY, SH_DENYNO, S_IWRITE|S_IREAD))==-1)
	{
		od_printf("Error creating gac_bj.dat\r\n");
		gac_pause();
		UpdateCursor(); // 12/96        
		od_log_write("Error creating gac_bj.dat");
		od_exit(1, FALSE);
	}
	misc=0;
	curplayer=0;
	memset(node,0,sizeof(node));
	memset(status,0,sizeof(status));
	write(gamedab,&misc,1);
	write(gamedab,&curplayer,1);
	write(gamedab,&total_nodes,1);
	write(gamedab,node,total_nodes*2);
	write(gamedab,status,total_nodes);
	close(gamedab);
}

// ===========================================================================
//  This function opens the file "GAC_BJ.DAT" in the current directory and
//  leaves it open with deny none access. This file uses record locking
//  for shared access.                                                                                                          */
// ===========================================================================
void open_gamedab()
{
	if((gamedab=open("gac_bj.dat",O_RDWR|O_DENYNONE|O_BINARY))==-1)
	{
		od_printf("Error opening gac_bj.dat\r\n");                //  open deny none */
		gac_pause();
		UpdateCursor(); // 12/96        
		od_log_write("Error opening gac_bj.dat");
		od_exit(1, FALSE);
	}
}

// ===========================================================================
//  Lists the players currently in the game and the status of the shoe.
// ===========================================================================
void list_bj_players()
{
	INT16 i;

	getgamedat(0);
	od_printf("\r\n");
	UpdateCursor();
	if(!total_players)
	{
		od_printf("`Bright White`No game in progress\r\n");
		UpdateCursor();
		return;
	}
	for(i=0;i<total_nodes;i++)
	if(node[i])
	{
		od_printf("`Magenta`Node %2d: `bright magenta`%s `Magenta`%s\r\n"
			,i+1, getname(i+1),activity(status[i]));
		UpdateCursor();
	}
	getcarddat();
}

// ===========================================================================
//  This function replaces the card symbols in 'str' with letters to
//  represent the different suits.                                                                                      */
// ===========================================================================
void strip_symbols(char *str)
{
	INT16 i,j;

j=strlen(str);
for(i=0;i<j;i++)
	if(str[i]>=3 && str[i]<=6)
		switch(str[i]) {
			case 3:
				str[i]='H';
				break;
			case 4:
				str[i]='D';
				break;
			case 5:
				str[i]='C';
				break;
			case 6:
				str[i]='S';
				break; }
}


// ===========================================================================
//  Reads information from GAC_BJ.DAT file. If 'lockit' is 1, the file is
//  and putgamedat must be called to unlock it. If your updating the info
//  in GAC_BJ.DAT, you must first call getgamedat(1), then putgamedat().                */
// ===========================================================================
void getgamedat(char lockit)
{
		INT16 i=0;

//  retry 100 times taking at least 3 seconds */
// Seek to the beginning of the file
lseek(gamedab,0L,SEEK_SET);
while(lock(gamedab,0L,filelength(gamedab))==-1 && i++<100)
	od_sleep(30);  //  lock the whole thing */
if(i>=100) {
	od_printf("`Bright red`Error locking gac_bj.dat\r\n"); //}
	od_log_write("Error locking gac_bj.dat");}

lseek(gamedab,0L,SEEK_SET);
read(gamedab,&misc,1);
read(gamedab,&curplayer,1);
read(gamedab,&total_nodes,1);
total_players=0;
for(i=0; i<total_nodes; i++) {
	read(gamedab,&node[i],2);          //  user number playing for each node */
	node[i]=LE_SHORT(node[i]);
	if(node[i])
		total_players++;
}
read(gamedab,status,total_nodes);          //  the status of the player */
	if(lockit==0 )
	{
		unlock(gamedab,0L,filelength(gamedab));
	}
}

// ===========================================================================
//  Writes information to GAC_BJ.DAT file. getgamedat(1) MUST be called before
//  this function is called.
// ===========================================================================
void putgamedat()
{
	int i;

lseek(gamedab,0L,SEEK_SET);
write(gamedab,&misc,1);
write(gamedab,&curplayer,1);
write(gamedab,&total_nodes,1);
	for(i=0;i<total_nodes;i++) {
		node[i]=LE_SHORT(node[i]);
		write(gamedab,&node[i],2);
		node[i]=LE_SHORT(node[i]);
	}
write(gamedab,status,total_nodes);
unlock(gamedab,0L,filelength(gamedab));

}

// ===========================================================================
//  This function makes the next active node the current player
// ===========================================================================
void nextplayer()
{
		INT16 i;

getgamedat(1);                      //  get current info and lock */

if((!curplayer                                          //  if no current player */
		|| total_players==1)            //  or only one player in game */
		&& node[node_num-1]) {          //  and this node is in the game */
	curplayer=node_num;                     //  make this node current player */
		putgamedat();                   //  write data and unlock */
		return; }                       //  and return */

for(i=curplayer;i<total_nodes;i++)      //  search for the next highest node */
		if(node[i])                     //  that is active */
	break;
if(i>=total_nodes) {                            //  if no higher active nodes, */
	for(i=0;i<curplayer-1;i++)              //  start at bottom and go up  */
	if(node[i])
			break;
	if(i==curplayer-1)                              //  if no active nodes found */
		curplayer=0;                            //  make current player 0 */
		else
		curplayer=i+1; }
else
	curplayer=i+1;                                  //  else active node current player */

putgamedat();                       //  write info and unlock */
}



// This function displays a card in ANSI format at the specified
// top-left coordinates.  The cards are numbered 1-52.  Corresponding
// to the cards 2-A in the suit order Hearts, Clubs, Diamonds, Spades.
// e.g. # - 1 2 3 4 5 6 7 8  9 10 11 12 13   
//   card - 2 3 4 5 6 7 8 9 10 J  Q  K  A    (Hearts)

void DisplayCard( INT16 left, INT16 top, card_t card)
{
	// the lines that will ultimately be printed.
	char line1[125], line2[125], line3[125], line4[125], line5[125];
	// some color configs...
	char suitcolor[20], cardchar, barcolor[25];
	// storage for the current border types
	char box_chars[8];
	// loop
	INT16 i;
	
	
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



	// Set the color of the bar in the center of the card...
	sprintf(barcolor, "`bright black on white`%c", 179);
	
	// This area sets the strings to be used for printing the cards.
	// switch((card-1)/13)
	switch(card.suit)
	{              
	
	//     

		// The card is a heart
		case H:
		{
			if (!symbols) 
				cardchar = 'H';
			else
				cardchar = '';
			strcpy(suitcolor, "`red on white`");
			break;
		}
		// The card is a club
		case C:
		{
			if (!symbols) 
				cardchar = 'C';
			else
				cardchar = '';
			strcpy(suitcolor, "`black on white`");
			break;
		}
		// The card is a diamond
		case D:
		{
			if (!symbols) 
				cardchar = 'D';
			else
				cardchar = '';
			strcpy(suitcolor, "`red on white`");
			break;
		}
		// The card is a spade
		case S:
		{
			if (!symbols) 
				cardchar = 'S';
			else
				cardchar = '';
			strcpy(suitcolor, "`black on white`");
			break;
		}
	}

	// Determine which card it is and set the strings...
	switch(card.value)
	{
		// an ace (a multiple of 13)
		case A:                    
		{
			sprintf(line1, "%sA%s%s   %s%s ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s   %s%s ", suitcolor, cardchar, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line3, "%s %s%s %c %s%s ", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s   %s%sA", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			break;
		}
		// a 2
		case 2:
		{
			sprintf(line1, "%s2%s%s %c %s%s ", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s   %s%s ", suitcolor, cardchar, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line3, "%s %s%s   %s%s ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s %c %s%s2", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			break;
		}
		// a 3
		case 3:
		{
			sprintf(line1, "%s3%s%s %c %s%s ", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s   %s%s ", suitcolor, cardchar, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line3, "%s %s%s %c %s%s ", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s %c %s%s3", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			break;
		}
		// a 4
		case 4:
		{
			sprintf(line1, "%s4%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s   %s%s ", suitcolor, cardchar, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line3, "%s %s%s   %s%s ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s%c %c%s%s4", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a 5
		case 5:
		{                          
			sprintf(line1, "%s5%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s   %s%s ", suitcolor, cardchar, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line3, "%s %s%s %c %s%s ", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s%c %c%s%s5", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a 6
		case 6:
		{
			sprintf(line1, "%s6%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s   %s%s ", suitcolor, cardchar, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line3, "%s %s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s%c %c%s%s6", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a 7
		case 7:
		{
			sprintf(line1, "%s7%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s %c %s%s ", suitcolor, cardchar, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line3, "%s %s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s   %s%s%c", suitcolor, barcolor, suitcolor, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s%c %c%s%s7", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a 8
		case 8:
		{
			sprintf(line1, "%s8%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s %c %s%s ", suitcolor, cardchar, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line3, "%s %s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s %c %s%s%c", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s%c %c%s%s8", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a 9
		case 9:
		{
			sprintf(line1, "%s9%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s%c %c%s%s ", suitcolor, cardchar, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line3, "%s %s%s %c %s%s ", suitcolor, barcolor, suitcolor, cardchar, barcolor, suitcolor);
			sprintf(line4, "%s %s%s%c %c%s%s%c", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s%c %c%s%s9", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a 10
		case 10:
		{
			sprintf(line1, "%s1%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line2, "%s0%s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line3, "%s%c%s%s%c %c%s%s%c", suitcolor, cardchar, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor, cardchar);
			sprintf(line4, "%s %s%s%c %c%s%s1", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			sprintf(line5, "%s %s%s%c %c%s%s0", suitcolor, barcolor, suitcolor, cardchar, cardchar, barcolor, suitcolor);
			break;
		}
		// a Jack
		case J:
		{
			sprintf(line1, "%sJ%s%s   %s%s ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s  %c%s%s ", suitcolor, cardchar, barcolor, suitcolor, 209, barcolor, suitcolor);
			sprintf(line3, "%s %s%s  %c%s%s ", suitcolor, barcolor, suitcolor, 179, barcolor, suitcolor);
			sprintf(line4, "%s %s%s%c%c%c%s%s%c", suitcolor, barcolor, suitcolor, 212, 205, 190, barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s   %s%sJ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			break;
		}
		// a Queen
		case Q:
		{
			sprintf(line1, "%sQ%s%s   %s%s ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			sprintf(line2, "%s%c%s%s%c%c%c%s%s ", suitcolor, cardchar, barcolor, suitcolor, 213, 205,184 , barcolor, suitcolor);
			sprintf(line3, "%s %s%s%c %c%s%s ", suitcolor, barcolor, suitcolor, 179,179 , barcolor, suitcolor);
			sprintf(line4, "%s %s%s%c%c%c%s%s%c", suitcolor, barcolor, suitcolor,212,205 ,181 , barcolor, suitcolor, cardchar);
			sprintf(line5, "%s %s%s   %s%sQ", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
			break;
		}
		// a King
		case K:
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
	od_set_color(D_CYAN, D_BLACK);
	// Create a box for the card to go in.
	od_draw_box( (char) left, (char) top, (char) (left + 10), (char) (top + 6));
	
	// Print the guts of the card
	od_set_cursor(top+1, left+2);
	od_printf("%s", line1);
	od_set_cursor(top+2, left+2);
	od_printf("%s", line2);
	od_set_cursor(top+3, left+2);
	od_printf("%s", line3);
	od_set_cursor(top+4, left+2);
	od_printf("%s", line4);
	od_set_cursor(top+5, left+2);
	od_printf("%s", line5);

	// Restore the current box characters
	for (i=0;i<8;i++)
	{
		od_control.od_box_chars[i] = box_chars[i];
	}
	return;
}

void DisplayCardBack( INT16 left, INT16 top)
{
	// the lines that will ultimately be printed.
	char line1[125], line2[125], line3[125], line4[125], line5[125];
	// some color configs...
	char suitcolor[20], barcolor[25];
	// storage for the current border types
	char box_chars[8];
	// loop
	INT16 i;
	
	
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



	// Set the color of the bar in the center of the card...
	sprintf(barcolor, "`bright green`%c", 179);
	
	// This area sets the strings to be used for printing the cards.
	// switch((card-1)/13)
	strcpy(suitcolor, "`cyan`");

	sprintf(line1, "%s<%s%s   %s%s>", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
	sprintf(line2, "%s<%s%s T %s%s>", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
	sprintf(line3, "%s<%s%sTBJ%s%s>", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
	sprintf(line4, "%s<%s%s J %s%s>", suitcolor, barcolor, suitcolor, barcolor, suitcolor);
	sprintf(line5, "%s<%s%s   %s%s>", suitcolor, barcolor, suitcolor, barcolor, suitcolor);

	
	
	// Set the color of the box to be created.
	od_set_color(D_GREEN, D_BLACK);
	// Create a box for the card to go in.
	od_draw_box( (char) left, (char) top, (char) (left + 10), (char) (top + 6));
	
	// Print the guts of the card
	od_set_cursor(top+1, left+2);
	od_printf("%s", line1);
	od_set_cursor(top+2, left+2);
	od_printf("%s", line2);
	od_set_cursor(top+3, left+2);
	od_printf("%s", line3);
	od_set_cursor(top+4, left+2);
	od_printf("%s", line4);
	od_set_cursor(top+5, left+2);
	od_printf("%s", line5);

	// Restore the current box characters
	for (i=0;i<8;i++)
	{
		od_control.od_box_chars[i] = box_chars[i];
	}
	return;
}


/* This function is responsible for controlling the text output to our now
	graphical screen display.  It must increment the global line number,
	check if the line is to high, and scroll if necessary
*/


void UpdateCursor(void)
{
	currentLine++;

	if (currentLine >= 22)
	{
		// grab a portion of the screen and scroll it up
		od_scroll(1, currentTop, 75, 23, SCROLL_VALUE, SCROLL_NORMAL);
		
		currentLine-=SCROLL_VALUE; // scroll
		od_set_cursor(currentLine, 1);
	}
	return;
}

