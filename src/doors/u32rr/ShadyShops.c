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
}


// Usurper - Shady Shops

static void Meny(void)
{
	const int offset = 25;

	newscreen();
	pbreak();
	d_line(MAGENTA, "-*- Shady Shops -*-");
	pbreak();
	TEXT("You stumble in to the dark areas of the town. It is here where you can get what you want, without any questions being asked. Trouble is never far away in these neighbourhood.");
	pbreak();

	menu2(ljust("(D)rug Palace", offset));
	menu("(S)teroid Shop");

	menu2(ljust("(O)rbs Health Club", offset));
	menu(Asprintf("(G) %s%s%s Magic Services", uplc, config.groggo_name, config.textcol1));

	menu2(ljust("(B)eer Hut", offset));
	menu("(A)lchemists Heaven");

	menu("(R)eturn to street");
}

static void Display_Menu(bool force, bool terse, bool *refresh)
{
	sethotkeys_on(NoKill, "BAGOSDR\n?");

	if(terse) {
		if(!player->expert) {
			if(*refresh && player->auto_meny) {
				*refresh=false;
				Menu();
			}
			pbreak();
			PART("Shady Shops (");
			d_part(config.hotkeycolor, "?");
			PART(" for menu) :");
		}
		else {
			pbreak();
			PART("Shady Shops (B,A,G,O,S,D,R,?) :");
		}
	}
	else {
		if((!player->expert) || force) {
			Meny();
		}
	}
}

void Shady_Shops(void)
{
	char cho=0;
	bool done=false;
	bool refresh=false;

	do {
		if(onliner->location != DarkAlley) {
			refresh=true;
			onliner->location=DarkAlley;
			strcpy(onliner->doing, location_desc(onliner.location));
		}
		// auto-travel
		switch(global_auto_probe) {
			case NoWhere:
				Display_Menu(true, true, &refresh);
				cho=toupper(getchar());
				break;
			case UmanCave:
				cho='R';
				break;
		}

		// Filter out disabled options
		if(cho=='D' && (!config.allow_drugs)) {
			pbreak();
			d_line(BRIGHT_RED, "Drugs are banned in this game.");
			upause();
			cho=' ';
		}
		else if(cho=='S' && (!config.allow_steroids)) {
			pbreak();
			d_line(BRIGHT_RED, "Steroids are banned in this game.");
			upause();
			cho=' ';
		}

		switch(cho) {
			case '?':
				if(player->expert)
					Display_Menu(true, false, &refresh);
				else
					Display_Menu(false, false, &refresh);
				break;
			case 'R':	// Return
				done=true;
				break;
			case 'O':	// Orbs drink cener
				if((!king->shop_orbs) && (!player->king)) {
					pbreak();
					d_line(BRIGHT_RED, "Orbs Health Club is closed! (The ", upcasestr(kingstring(king->sexy)), "s order!");

  case cho of
   '?':begin
        if player.expert=true then display_menu(true,false)
                              else display_menu(false,false);
       end;
   'R':begin {return}
        done:=true;
       end;
   'O':begin {orbs drink center}

        load_king(fload,king);

        if (king.shop_orbs=false) and (player.king=false) then begin
         crlf;
         d(12,'Orbs Health Club is closed! (The '+upcasestr(kingstring(king.sexy))+'s order!)');
        end
        else begin
         crlf;
         crlf;
         d(config.textcolor,'You decide to enter this somewhat dubious place.');
         orb_center;
        end;
       end;
   'A':begin {alchemist secret order}
        if player.class<>Alchemist then begin
         crlf;
         d(5,'The guards outside the building humiliate you and block the entrance.');
         d(5,'It seems as only Alchemists are allowed.');
        end
        else begin
         alchemisty;
        end;
       end;
   'B':begin {Bobs Beer Hut}

        muffis;
        if global_registered=true then begin
         load_king(fload,king);

         if (king.shop_bobs=false) and (player.king=false) then begin
          crlf;
          d(12,config.bobsplace+' is closed! (The '+upcasestr(kingstring(king.sexy))+'s order!)');
         end
         else begin
          crlf;
          crlf;
          d(config.textcolor,'You enter '+ulcyan+config.bobsplace);
          bobs_inn;
         end;
        end
        else begin
         crlf;
         d(12,'Sorry, only available in the registered version.');
         pause;
        end;

       end;
   'G':begin {Evil Magic}

         load_king(fload,king);

         if (king.shop_evilmagic=false) and (player.king=false) then begin
          crlf;
          d(12,owner+'s place is closed! (The '+upcasestr(kingstring(king.sexy))+'s order!)');
         end
         else begin
          Groggos_Magic;
         end;
       end;
   'D':begin {Drugs}
        load_king(fload,king);

        if (king.shop_drugs=false) and (player.king=false) then begin
         crlf;
         d(12,'The Drug Palace is closed! (The '+upcasestr(kingstring(king.sexy))+'s order!)');
        end
        else begin
         Drug_Store;
        end;
       end;
   'S':begin {Steroids}
        load_king(fload,king);

        if (king.shop_steroids=false) and (player.king=false) then begin
         crlf;
         d(12,'The Steroid Shop is closed! (The '+upcasestr(kingstring(king.sexy))+'s order!)');
        end
        else begin
         Steroid_Store;
        end;
       end;
  end; {case .end.}

 until done;
 crlf;

end; {shady_shops *end*}

end. {Unit Shady .end.}
