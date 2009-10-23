
/*

Copyright 2007 Jakob Dangarden
C translation Copyright 2009 Stephen Hurd

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


// Usurper - Armor Shop
#include <stdio.h>
#include <stdlib.h>

#include "macros.h"
#include "files.h"
#include "structs.h"

#include "todo.h"

const char *backpack = "back-pack";

static void Note_It(const char *spook)
{
	char	*str;

	if(asprintf(&str, " %s%s%s bought a %s%s%s.", config.plycolor, player->name2, config.textcol1, uitemc, spook, config.textcol1)<0)
		CRASH;
	newsy(true, "Armor", str, NULL);
	free(str);
}

static bool Purchase_Now(uint16_t i)
{
	SAYPART("I have a nice ");
	SAYPART(objects[i].name);
	SAYPART(" here");
	PART(", ");
	PLAYER(config.reese_name);
	TEXT(" says.");
	PART("You can get it for ");
	d_part(YELLOW, commastr(objects[i].value));
	PART(" ");
	PART(config.moneytype);
	PART(" ");
	PART(config.moneytype3);
	TEXT(".");
	pbreak();

	if(confirm("Buy it ",'Y')) {
		decplayermoney(player,objects[i].value);
		switch(objects[i].ttype) {
			case Head:
				player->head=i;
				break;
			case Body:
				player->body=i;
				break;
			case Arms:
				player->arms=i;
				break;
			case Hands:
				player->hands=i;
				break;
			case Legs:
				player->legs=i;
				break;
			case Feet:
				player->feet=i;
				break;
			case Face:
				player->face=i;
				break;
			case Abody:
				player->abody=i;
				break;
			case Shield:
				player->shield=i;
				break;
		}
		objekt_affect(1, i, objects[i].ttype, player, true);
		return true;
	}

	return false;
}

static bool Purchase_Help(enum objtype tupp, long max_spend)
{
	bool		ok=false;
	uint16_t	i;
	uint16_t	best=UINT16_MAX;
	uint15_t	bestnr;

	for(i=0; i<MAX_OBJECTS; i++) {
		if(objects[i].shop && objects[i].str_need <= player->strength && (!objects[i].curses) && objects[i].value <= max_spend) {
			ok=true;
			if(class_restricted(player->class, objects[i], 0))
				ok=false;
			if(player->dark > 0 && objectks[i].good)
				ok=false;
			if(player.chiv > 0 && objects[i].evil)
				ok=false;
			if(ok) {
				best=i;

				if(objects[i].value > (max_spend - (max_spend / 2)) {
					if(purchase_now(i))
						break;
					else
						ok=false;
				}
				else
					ok=false;
			}
		}
	}

	if((!ok) && best != UINT16_MAX)
		ok=purchase_now(best);
	return ok;
}

static void Meny(void)
{
	newscreen();
	pbreak();

	d_part(MAGENTA, "Armorshop, run by ");
	d_part(MAGENTA, config.reese_name);
	d_line(MAGENTA, " the elf");
	d_line(MAGENTA, mkstring(strlen(config.reese_name)+26);
	pbreak();

	TEXT("As you enter the store you notice a strange but appealing smell.");
	TEXT("You recall that some merchants use magic elixirs to make their selling easier...");
	PLAYER(config.reese_name);
	TEXT(" suddenly appears out of nowhere, with a smile on his face.");
	TEXT("He is known as a respectable citizen, although evil tounges speaks of meetings with dark and mysterious creatures from the deep dungeons.");
	PART("You are interrupted in your thougts as ");
	PLAYER(config.reese_name);
	TEXT(" kindly asks");
	TEXT("what you want :");

	PART("(You have ");
	d_part(YELLOW, commastr(player->gold));
	TEXT(" ");
	TEXT(config.moneytype);
	TEXT(" ");
	TEXT(config.moneytype3);
	TEXT(")");
	
Procedure Meny;
var justval : byte;
begin

 clearscreen;
 crlf;
 s:='Armorshop, run by '+Reese^+' the elf';
 d(5,s);
 d(5,mkstring(length(s),underscore));
 crlf;

 d(config.textcolor,'As you enter the store you notice a strange but appealing smell.');
 d(config.textcolor,'You recall that some merchants use magic elixirs to make their selling easier...');
 sd(global_plycol,Reese^);
 d(config.textcolor,' suddenly appears out of nowhere, with a smile on his face.');
 d(config.textcolor,'He is known as a respectable citizen, although evil tounges speaks of');
 d(config.textcolor,'meetings with dark and mysterious creatures from the deep dungeons.');
 sd(config.textcolor,'You are interrupted in your thougts as ');
 sd(global_plycol,Reese^);
 d(config.textcolor,' kindly asks');
 d(config.textcolor,'what you want :');

 sd(config.textcolor,'(You have ');
 sd(14,commastr(player.gold));
 d(config.textcolor,' '+config.moneytype+' '+config.moneytype3+')');

 crlf;
 justval:=12;

 if config.classic then begin
  menu('(R)eturn to street');
  menu('(B)uy');
  menu('(S)ell');
  menu('(L)ist items');
 end
 else begin
  {1 : s:='Allow Hand Equipment';
   2 : s:='Allow Head Equipment';
   3 : s:='Allow Body Equipment';
   4 : s:='Allow Arm Equipment';
   5 : s:='Allow Left Finger Equipment';
   6 : s:='Allow Right Finger Equipment';
   7 : s:='Allow Leg Equipment';
   8 : s:='Allow Feet Equipment';
   9 : s:='Allow Waist Equipment';
   10: s:='Allow 1 Neck Equipment';
   11: s:='Allow 2 Neck Equipment';
   12: s:='Allow Face Equipment';
   13: s:='Allow Shield';
   14: s:='Allow Around Body Equipment';
   15: s:='Allow Secondary Weapon';
   }

  menu2(ljust('(B)uy',justval));
  justval:=16;
  if config.allowitem[2] then menu2(ljust('(H)ead Items',justval));
  if config.allowitem[3] then menu3(ljust('B(o)dy Items',justval),3);
  if config.allowitem[4] then menu2(ljust('(A)rm Items',justval));
  crlf;

  justval:=12;
  menu2(ljust('(S)ell',justval));
  justval:=16;
  if config.allowitem[1] then menu3(ljust('Ha(n)d Items',justval),4);
  if config.allowitem[7] then menu2(ljust('(L)eg Items',justval));
  if config.allowitem[8] then menu2(ljust('(F)eet Items',justval));
  crlf;

  justval:=12;
  menu2(ljust('(R)eturn',justval));
  justval:=16;
  if config.allowitem[12] then menu3(ljust('Fac(e) Items',justval),5);
  if config.allowitem[14] then menu2(ljust('(C)loaks',justval));
  if config.allowitem[13] then menu3(ljust('Sh(i)elds',justval),4);
  crlf;
  crlf;
  menu('(1) ask '+uplc+reese^+config.textcol1+' to help you with your equipment!');
 end;

end; {meny *end*}

Procedure Display_Menu(force,short : boolean);
var s :s70;
begin

 if short=true then begin
  if player.expert=false then begin
   if (refresh) and (player.auto_meny) then begin
    refresh:=false;
    meny;
   end;
   crlf;
   sd(config.textcolor,'Armor Shop ('+config.textcol2+'?'+config.textcol1+' for menu) :');
  end
  else begin
   crlf;
   if config.classic then begin
    sd(config.textcolor,'Armors (L,B,S,R,?) :');
   end
   else begin
    s:='Armors (B,S';

    if config.allowitem[2]  then s:=s+',H';
    if config.allowitem[3]  then s:=s+',O';
    if config.allowitem[4]  then s:=s+',A';
    if config.allowitem[1]  then s:=s+',N';
    if config.allowitem[7]  then s:=s+',L';
    if config.allowitem[8]  then s:=s+',F';
    if config.allowitem[12] then s:=s+',E';
    if config.allowitem[14] then s:=s+',C';
    if config.allowitem[13] then s:=s+',I';

    s:=s+',R,?) :';

    sd(config.textcolor,s);
   end;
  end;
 end
 else begin
  if (player.expert=false) or (force=true) then begin
   meny;
  end;
 end;

end; {display_menu *end*}

Procedure Armor_Shop;
{ globals }
var
   s : s70;
   bought : integer;
   objekt : ^orec;
   Reese : ^s70;
   refresh : boolean;
   counter : integer;
   needs   : integer;
   ok : boolean;

   head_need,
   body_need,
   arms_need,
   legs_need,
   feet_need,
   face_need,
   shield_need,
   hands_need,
   abody_need : boolean;

var
    komihag, cho, ch : char;

    cc, y,j, justval : integer;

    x, i, xx : longint;

    show,
    leave_place : boolean;

    zz : word;

    inarm : ^armrec;
    soktyp : objtype;

    restrict : boolean;
    normcol,
    shadcol,col : byte;
    aarmor : armrec;

begin

 new(objekt);
 new(reese);
 new(inarm);

 {set default armor type when entering proc, NEW mode}
 if config.allowitem[1] then soktyp:=Arms
  else if config.allowitem[2] then soktyp:=Head
  else if config.allowitem[3] then soktyp:=Body
  else if config.allowitem[4] then soktyp:=Arms
  else if config.allowitem[7] then soktyp:=Legs
  else if config.allowitem[8] then soktyp:=Feet

  else if config.allowitem[12] then soktyp:=Face
  else if config.allowitem[13] then soktyp:=Shield
  else if config.allowitem[14] then soktyp:=Abody
  else soktyp:=Head;

 {fetch Reeses name from .CFG, #16}
 reese^:=cfg_string(16);
 if reese^='' then reese^:='Reese';

 komihag:=' ';
 leave_place:=false;

 crlf;
 repeat

  {update online location, if necessary}
  if onliner.location<>onloc_armorshop then begin
   refresh:=true;
   onliner.location:=onloc_armorshop;
   onliner.doing   :=location_desc(onliner.location);
   add_onliner(OUpdateLocation,onliner);
  end;

  if player.armhag<1 then begin
   crlf;
   d(15,'The strong desk-clerks throw you out!');
   d(15,'You realize that you went a little bit too far in');
   d(15,'your attempts to get a good deal.');
   cho:='R';

   Bad_News('A');

  end
  else if komihag=' ' then begin
   display_menu(true,true);
   cho:=upcase(getchar);
  end
  else begin
   cho:=komihag;
   komihag:=' ';
  end;

  if cho='?' then begin
   if player.expert=true then display_menu(true,false)
                         else display_menu(false,false);
  end;

  if (cho='H') and (config.allowitem[2]=false) and (config.classic=false) then cho:=' ';
  if (cho='O') and (config.allowitem[3]=false) and (config.classic=false) then cho:=' ';
  if (cho='A') and (config.allowitem[4]=false) and (config.classic=false) then cho:=' ';
  if (cho='N') and (config.allowitem[1]=false) and (config.classic=false) then cho:=' ';
  if (cho='L') and (config.allowitem[7]=false) and (config.classic=false) then cho:=' ';
  if (cho='F') and (config.allowitem[8]=false) and (config.classic=false) then cho:=' ';
  if (cho='E') and (config.allowitem[12]=false) and (config.classic=false) then cho:=' ';
  if (cho='C') and (config.allowitem[14]=false) and (config.classic=false) then cho:=' ';
  if (cho='I') and (config.allowitem[13]=false) and (config.classic=false) then cho:=' ';

  if (cho in ['H','O','A','N','L','F','E','C','I']) and (config.classic=false) then begin
   case cho of
    'H': soktyp:=Head;
    'O': soktyp:=Body;
    'A': soktyp:=Arms;
    'N': soktyp:=Hands;
    'L': soktyp:=Legs;
    'F': soktyp:=Feet;
    'E': soktyp:=Face;
    'C': soktyp:=Abody;
    'I': soktyp:=Shield;
   end; {case .end.}

   crlf;
   j:=0;
   cc:=3;
   justval:=14;

   sd(5,ljust('#',4));
   sd(5,ljust('Item',16));
   d(5,rjust('Cost',15));
   crlf;

   for i:=1 to fsob(soktyp) do begin
    load_objekt(objekt^,soktyp,i);
    if (objekt^.shop=true) then begin
     inc(j,1);

     inc(cc,1);

     restrict:=false;
     if class_restricted(player.class,objekt^,0)=true then begin
      restrict:=true;
     end;

     normcol:=3;
     shadcol:=8;

     if restrict then col:=shadcol
                 else col:=normcol;

     {#}
     sd(col,ljust(commastr(j),4));

     {name}
     s:=objekt^.name;
     while length(s)<27 do begin
      s:=s+'.';
     end;
     normcol:=15;
     if restrict then col:=shadcol
                 else col:=normcol;
     sd(col,s);

     {price}
     normcol:=14;
     if restrict then col:=shadcol
                 else col:=normcol;
     s:=commastr(objekt^.value);
     sd(col,s);

     {restrictions}
     if class_restricted(player.class,objekt^,0)=true then begin
      d(7,' *Class Restricted*');
     end
     else begin
      crlf;
     end;

     {menu}
     if cc>global_screenlines-2 then begin
      cc:=1;
      crlf;
      menu2('[C]ontinue  ');
      menu2('(A)bort  ');
      menu2('(B)uy item :');

      repeat
       ch:=upcase(getchar);
      until ch in ['C','A','B',ReturnKey];

      if ch=ReturnKey then ch:='C';

      case ch of
       'C':begin
            sd(config.textcolor,' More');
           end;
       'A':begin
            sd(config.textcolor,' Abort');
            break;
           end;
       'B':begin
            sd(config.textcolor,' Buy item');
            cho:='B';
            break;
           end;

      end; {case .end.}

      crlf;
     end;

    end;
   end;

   crlf;
  end;

  case cho of
   '1':begin {be REESE om hj„lp med ink”pen}
        if config.classic then begin
         {}
        end
        else begin
         {NEW GameMode}
         crlf;
         sd(global_talkcol,'Hey! I need some help over here!');
         sd(config.textcolor,', you shout to ');
         sd(global_plycol,reese^);
         d(config.textcolor,'.');

         sd(global_talkcol,'Ok. Let''s see how much '+config.moneytype+' you got');
         sd(config.textcolor,', ');
         sd(global_plycol,reese^);
         d(config.textcolor,' says.');

         pause;
         crlf;
         if player.gold=0 then begin
          sd(config.textcolor,'You show ');
          sd(global_plycol,reese^);
          d(config.textcolor,' your empty purse.');

          sd(global_talkcol,'Is this supposed to be funny?');
          sd(config.textcolor,', ');
          sd(global_plycol,reese^);
          d(config.textcolor,' says with a strange voice.');
         end
         else if player.gold<50 then begin
          d(config.textcolor,'You show '+reese^+' your '+config.moneytype+' '+config.moneytype3+'.');
          sd(global_talkcol,'You won''t get anything for that!');
          sd(config.textcolor,', ');
          sd(global_plycol,reese^);
          d(config.textcolor,' says in a mocking tone.');
         end
         else begin
          needs:=0;

          head_need   :=false;
          body_need   :=false;
          arms_need   :=false;
          legs_need   :=false;
          feet_need   :=false;
          face_need   :=false;
          shield_need :=false;
          hands_need  :=false;
          abody_need  :=false;

          {1 : s:='Allow Hand Equipment';
           2 : s:='Allow Head Equipment';
           3 : s:='Allow Body Equipment';
           4 : s:='Allow Arm Equipment';
           5 : s:='Allow Left Finger Equipment';
           6 : s:='Allow Right Finger Equipment';
           7 : s:='Allow Leg Equipment';
           8 : s:='Allow Feet Equipment';
           9 : s:='Allow Waist Equipment';
           10: s:='Allow 1 Neck Equipment';
           11: s:='Allow 2 Neck Equipment';
           12: s:='Allow Face Equipment';
           13: s:='Allow Shield';
           14: s:='Allow Around Body Equipment';
           15: s:='Allow Secondary Weapon';
          }

          if (player.head=0) and (config.allowitem[2]) then begin
           inc(needs);
           head_need:=true;
          end;
          if (player.body=0) and (config.allowitem[3]) then begin
           inc(needs);
           body_need:=true;
          end;
          if (player.arms=0) and (config.allowitem[4]) then begin
           inc(needs);
           arms_need:=true;
          end;
          if (player.legs=0) and (config.allowitem[7]) then begin
           inc(needs);
           legs_need:=true;
          end;
          if (player.feet=0) and (config.allowitem[8]) then begin
           inc(needs);
           feet_need:=true;
          end;
          if (player.face=0) and (config.allowitem[12]) then begin
           inc(needs);
           face_need:=true;
          end;
          if (player.shield=0) and (config.allowitem[13]) then begin
           inc(needs);
           shield_need:=true;
          end;
          if (player.hands=0) and (config.allowitem[1]) then begin
           inc(needs);
           hands_need:=true;
          end;
          if (player.abody=0) and (config.allowitem[14]) then begin
           inc(needs);
           abody_need:=true;
          end;

          crlf;
          if needs=0 then begin
           sd(global_talkcol,'You are already fully equipped!');
           sd(config.textcolor,', ');
           sd(global_plycol,reese^);
           d(config.textcolor,' says.');
          end
          else begin

           xx:=player.gold div needs;
           if xx<300 then begin {bort prioritera vissa equipment saker}
           end;

           bought:=0;
           {b”rja plocka ihop grejor}
           if abody_need  then purchase_help(abody,xx);
           if hands_need  then purchase_help(hands,xx);
           if shield_need then purchase_help(shield,xx);
           if face_need   then purchase_help(face,xx);
           if feet_need   then purchase_help(feet,xx);
           if legs_need   then purchase_help(legs,xx);
           if arms_need   then purchase_help(arms,xx);
           if body_need   then purchase_help(body,xx);
           if head_need   then purchase_help(head,xx);

           if bought>0 then begin
            crlf;
            sd(global_talkcol,'A pleasure doing business with you!');
            sd(config.textcolor,', ');
            sd(global_plycol,reese^);
            d(config.textcolor,' smiles.');
           end
           else begin
            crlf;
            sd(global_talkcol,'Too bad we couldn''t find anything suitable.');
            sd(config.textcolor,', ');
            sd(global_plycol,reese^);
            d(config.textcolor,' says.');
           end;

          end;
         end;

        end;
       end;
   'L':begin
        if config.classic then begin
         crlf;
         d(5,'Ancient Armors                 Price');
         cc:=1;
         justval:=14;

         for i:=1 to fs(FsArmorClassic) do begin
          load_armor(i,aarmor);

          {#}
          sd(3,ljust(commastr(i),4));

          {name}
          s:=aarmor.name+config.textcol1;
          repeat
           s:=s+'.';
          until length(s)>22;
          sd(global_itemcol,s);

          {price}
          s:=uyellow+commastr(aarmor.value);
          repeat
           s:='.'+s;
          until length(s)>=15;

          sd(config.textcolor,rjust(s,15));

          crlf;
          inc(cc);
          if cc>global_screenlines-2 then begin
           cc:=0;
           if confirm('Continue','Y')=false then begin
            break;
           end;
          end;
         end;
        end;

       end;
   'S':begin
        if config.classic then begin
         crlf;
         if player.armor=0 then begin
          d(global_talkcol,'You don''t have anything to sell!');
         end
         else begin
          load_armor(player.armor,inarm^);
          xx:=inarm^.value div 2;
          sd(global_plycol,reese^);
          d(config.textcolor,' declares that he will pay you ');
          sd(14,commastr(xx));
          sd(config.textcolor,' '+config.moneytype+' '+config.moneytype3+' for your ');
          d(global_itemcol,inarm^.name);

          if confirm('Will you sell it ','N')=true then begin
           sd(config.textcolor,'You give ');
           sd(global_plycol,reese^);
           d(config.textcolor,' your armor, and receive the '+config.moneytype+'.');

           incplayermoney(player,xx);
           player.armor:=0;
           player.apow:=0;
          end;
         end;
        end
        else begin
         crlf;
         crlf;
         if confirm('Sell ALL armor in your inventory','N')=true then begin
          counter:=0;
          for i:=1 to global_maxitem do begin
           if player.item[i]>0 then begin
            load_objekt(objekt^,player.itemtype[i],player.item[i]);
            if objekt^.ttype in [Head, Body, Arms, Hands, Legs, Feet,
                                 Face, Shield, Abody] then begin
             if objekt^.value>1 then begin
              xx:=objekt^.value div 2;
             end
             else begin
              xx:=objekt^.value;
             end;
             {time to sell}
             if xx<=0 then begin
              sd(global_itemcol,objekt^.name);
              sd(global_talkcol,' is worthless!');
              sd(config.textcolor,', ');
              sd(global_plycol,reese^);
              sd(config.textcolor,' says.');
             end
             else if objekt^.cursed=true then begin
              sd(global_itemcol,objekt^.name);
              d(global_talkcol,' is cursed!');
              sd(global_talkcol,'I don''t buy cursed items!');
              sd(config.textcolor,', ');
              sd(global_plycol,reese^);
              sd(config.textcolor,' says.');
             end
             else begin
              sd(global_plycol,reese^);
              sd(config.textcolor,' bought the ');
              sd(global_itemcol,objekt^.name);
              sd(config.textcolor,' for ');
              sd(14,commastr(xx));
              d(config.textcolor,' '+config.moneytype+' '+config.moneytype3+'.');

              incplayermoney(player,xx);
              player.item[i]:=0;
              inc(counter);
             end;

            end;
           end;
          end;
          if counter=0 then begin
           sd(global_plycol,reese^);
           d(config.textcolor,' looks at your empty '+backpack);
           d(global_talkcol,' You have nothing to sell!');
          end;

         end
         else begin
          repeat
           crlf;
           i:=item_select(player);
           if i>0 then begin
            load_objekt(objekt^,player.itemtype[i],player.item[i]);
            if objekt^.ttype in [Head, Body, Arms, Hands, Legs, Feet,
                                Face, Shield, Abody] then begin
             if objekt^.value>1 then begin
              xx:=objekt^.value div 2;
             end
             else begin
              xx:=objekt^.value;
             end;

             if xx<=0 then begin
              sd(global_talkcol,'That item is worthless!');
              sd(config.textcolor,', ');
              sd(global_plycol,reese^);
              d(config.textcolor,' says.');
             end
             else if objekt^.cursed=true then begin
              sd(global_talkcol,'I don''t buy cursed items!');
              sd(config.textcolor,', ');
              sd(global_plycol,reese^);
              d(config.textcolor,' says.');
             end
             else begin
              sd(global_plycol,reese^);
              sd(config.textcolor,' declares that he will give you ');
              sd(14,commastr(xx));

              sd(config.textcolor,' '+many_money(xx));

              sd(config.textcolor,' for your ');
              d(global_itemcol,objekt^.name+'.');

              menu2('(A)gree  ');
              menu2('(N)o Deal');
              sd(config.textcolor,':');
              repeat
               ch:=upcase(getchar);
              until ch in ['A','N'];

              crlf;
              case ch of
               'N':begin
                    sd(global_talkcol,'NO!? What the heck are you up to?, ');
                    sd(global_plycol,reese^);
                    d(config.textcolor,' asks.')
                   end;
               'A':begin
                    d(14,'Deal!');
                    player.item[i]:=0;
                    incplayermoney(player,xx);
                   end;
              end;
             end;
            end
            else begin
             sd(global_talkcol,'I don''t buy that kind of items');
             sd(config.textcolor,', ');
             sd(global_plycol,reese^);
             d(config.textcolor,' says.');
             crlf;
             pause;
            end;
           end;
          until i=0;
         end;
        end;
       end;

   'B':begin {buy .start.}
        if config.classic then begin
         crlf;
         if player.armor<>0 then begin
          d(global_talkcol,'Get rid of your old armor first!.');
          pause;
         end
         else begin
          d(global_talkcol,'Which one?');
          sd(config.textcolor,':');

          x:=fs(FsArmorClassic);
          zz:=get_number(0,65000);
          if (zz>0) and (zz<=x) then begin
           load_armor(zz,aarmor);

           sd(config.textcolor,'So you want a ');
           d(global_itemcol,aarmor.name);

           crlf;
           sd(config.textcolor,'It will cost you ');
           sd(14,commastr(aarmor.value));
           d(config.textcolor,' in '+config.moneytype+'.');

           sd(config.textcolor,'Pay ? ');
           menu2('(Y)es, ');
           menu2('[N]o, ');
           menu('(H)aggle');
           sd(config.textcolor,':');
           repeat
            ch:=upcase(getchar);
           until ch in ['Y','N','H',ReturnKey];

           if ch=ReturnKey then begin
            ch:='N';
           end;

           case ch of

            'H':begin
                 x:=haggle('A',aarmor.value,reese^);

                 if x<aarmor.value then begin
                  if player.gold<x then begin
                   sd(global_talkcol,'No '+config.moneytype+', no armor!');
                   sd(config.textcolor,', ');
                   sd(global_plycol,reese^);
                   d(config.textcolor,' yells.');
                   pause;
                  end
                  else begin
                   sd(global_plycol,reese^);
                   d(config.textcolor,' gives you the armor.');
                   d(config.textcolor,'You give him the '+config.moneytype+'.');
                   decplayermoney(player,x);
                   player.armor:=zz;
                   player.apow:=aarmor.pow;

                   note_it(aarmor.name);

                   pause;
                  end;
                 end;
                end;
            'N':begin
                 d(15,'No');
                end;
            'Y':begin
                 d(15,'Yes');
                 crlf;
                 if player.gold<aarmor.value then begin
                  sd(global_talkcol,'No '+config.moneytype+', no armor!');
                  sd(config.textcolor,', ');
                  sd(global_plycol,reese^);
                  d(config.textcolor,' yells.');
                  pause;
                 end
                 else begin
                  sd(global_talkcol,'Deal!');
                  sd(config.textcolor,', says ');
                  sd(global_plycol,reese^);
                  d(config.textcolor,' and give you the armor.');

                  d(config.textcolor,'You hand over the '+config.moneytype+'.');

                  decplayermoney(player,aarmor.value);
                  player.armor:=zz;
                  player.apow:=aarmor.pow;

                  note_it(aarmor.name);

                  pause;
                 end;
                end;
           end;
           ch:=' ';

          end;
         end;
        end
        else begin
         crlf;
         d(3,'Enter Item # to buy');
         sd(config.textcolor,':');

         s:=get_string(10);

         if s='?' then begin
          d(15,'List of armors :');
          case soktyp of {komih†g}
           Head:  komihag:='H';
           Body:  komihag:='O';
           Arms:  komihag:='A';
           Hands: komihag:='N';
           Legs:  komihag:='L';
           Feet:  komihag:='F';
           Face:  komihag:='E';
           Abody: komihag:='C';
           Shield:komihag:='I';
          end;
         end;

         x:=str_to_nr(s);
         j:=0;

         if (x>0) and (x<=fsob(soktyp)) then begin
          for i:=1 to fsob(soktyp) do begin
           load_objekt(objekt^,soktyp,i);
           if objekt^.shop=true then begin
            inc(j,1);
           end;
           if j=x then begin

            if (objekt^.good) and (player.chiv<1) and (player.dark>0) then begin
             d(12,'This item is charmed for good characters.');
             d(12,'You can buy it, but you not use it!');
            end
            else if (objekt^.evil=true) and (player.chiv>0) and (player.dark<1) then begin
             d(12,'This item is enchanted and can be used by evil characters only.');
             d(12,'You can buy it, but not use it!');
            end;

            if objekt^.str_need>player.strength then begin
             d(12,'This item is too heavy for you to use!');
            end;

            sd(config.textcolor,'Buy the ');
            sd(global_itemcol,objekt^.name);
            sd(config.textcolor,' for ');
            sd(14,commastr(objekt^.value));
            sd(config.textcolor,' '+config.moneytype);
            sd(config.textcolor,' (Y/[N] or (H)aggle) ?');
            repeat
             cho:=upcase(getchar);
            until cho in ['Y','N','H',ReturnKey];

            crlf;



            if cho='H' then begin
             x:=haggle('A',objekt^.value,reese^);
             if x<objekt^.value then begin
              if player.gold<x then begin
               You_Cant_Afford_It;
              end
              else begin
               if inventory_empty(player)=0 then begin
                d(config.textcolor,'Your inventory is full!');
                if confirm('Drop something ','Y')=true then begin
                 drop_item(player);
                end;
               end;

               if inventory_empty(player)>0 then begin
                j:=inventory_empty(player);
                d(14,'Done!');
                decplayermoney(player,x);
                player.item[j]:=i;
                player.itemtype[j]:=objekt^.ttype;

                note_it(objekt^.name);

                crlf;
                sd(config.textcolor,'Start to use the ');
                sd(global_itemcol,objekt^.name+' '+item_power_display(objekt^));
                sd(config.textcolor,' immediately');
                if confirm('','Y')=true then begin
                 use_item(j);
                end
                else begin
                 sd(config.textcolor,'You put the ');
                 sd(global_itemcol,objekt^.name);
                 d(config.textcolor,' in your '+backpack);
                end;
               end;
              end;
             end;
            end;

            if cho='Y' then begin
             if player.gold<objekt^.value then begin
              You_Cant_Afford_It;
             end
             else begin
              if inventory_empty(player)=0 then begin
               d(config.textcolor,'Your inventory is full!');
               if confirm('Drop something ','Y')=true then begin
                drop_item(player);
               end;
              end;

              if inventory_empty(player)>0 then begin
               j:=inventory_empty(player);
               d(14,'Done!');
               sd(config.textcolor,'You give ');
               sd(global_plycol,reese^+' ');
               sd(14,commastr(objekt^.value));
               d(config.textcolor,' '+config.moneytype+' '+config.moneytype3);
               decplayermoney(player,objekt^.value);
               player.item[j]:=i;
               player.itemtype[j]:=objekt^.ttype;

               note_it(objekt^.name);

               crlf;
               sd(config.textcolor,'Start to use the ');
               sd(global_itemcol,objekt^.name+' '+item_power_display(objekt^));
               sd(config.textcolor,' immediately');
               if confirm('','Y')=true then begin
                use_item(j);
               end
               else begin
                sd(config.textcolor,'You put the ');
                sd(global_itemcol,objekt^.name);
                d(config.textcolor,' in your '+backpack);
               end;
              end;
             end;
            end;
            break;
           end;
          end;
         end;
        end;
       end; {buy .end.}

   'R':begin
        crlf;
        leave_place:=true;
       end;

  end; {case .end.}

 until leave_place=true;

 {dispose pointer variables}
 dispose(objekt);
 dispose(reese);
 dispose(inarm);

end; {Armor_Shop *end*}

end. {Unit Armshop .end.}
