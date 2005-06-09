{                                                                         }
{  Copywrite 1993 Mark Dignam - Omen Computer Services - Perth Omen BBS.  }
{  This program ,including the source code MAY not be modified, changed   }
{  or altered in any way without written permission of the author.        }
{                                                                         }
{                                                                         }
{ Ansi Driver for Comms routines                                          }

unit Ansi_Drv;

interface

Uses Crt,dos;

procedure Ansi_Write(ch : char);

Implementation

Var
    Escape,Saved_X,
    Saved_Y               : Byte;
    Control_Code          : String;

function GetNumber(var Line:string):integer;

   var
     i,j,k         : integer;
     temp0,temp1   : String;

  Begin
       temp0 := line;
       val(temp0,i,j);
      if j = 0 then temp0 :=''
       else
      begin
         temp1:= copy(temp0,1,j-1);
         delete(temp0,1,j);
         val(temp1,i,j);
      end;
    line := temp0;
    GetNumber := i;
  end;

 procedure loseit;
    begin
      escape := 0;
      control_code := '';
    end;

 procedure Ansi_Cursor_move;

     var
      x,y       : integer;

    begin
     y := GetNumber(control_code);
     if y = 0 then y := 1;
     x := GetNumber(control_code);
     if x = 0 then x := 1;
     if y > 25 then y := 25;
     if x > 80 then x := 80;
     gotoxy(x,y);
    loseit;
    end;

procedure Ansi_Cursor_up;

 Var
   y,new_y,offset          : integer;

   Begin
     Offset := getnumber(control_code);
        if Offset = 0 then offset := 1;
      y := wherey;
      if (y - Offset) < 1 then
             New_y := 1
          else
             New_y := y - offset;
       gotoxy(wherex,new_y);
  loseit;
  end;

procedure Ansi_Cursor_Down;

 Var
   y,new_y,offset          : integer;

   Begin
     Offset := getnumber(control_code);
        if Offset = 0 then offset := 1;
      y := wherey;
      if (y + Offset) > 25 then
             New_y := 25
          else
             New_y := y + offset;
       gotoxy(wherex,new_y);
  loseit;
  end;

procedure Ansi_Cursor_Left;

 Var
   x,new_x,offset          : integer;

   Begin
     Offset := getnumber(control_code);
        if Offset = 0 then offset := 1;
      x := wherex;
      if (x - Offset) < 1 then
             New_x := 1
          else
             New_x := x - offset;
       gotoxy(new_x,wherey);
  loseit;
  end;

procedure Ansi_Cursor_Right;

 Var
   x,new_x,offset          : integer;

   Begin
     Offset := getnumber(control_code);
        if Offset = 0 then offset := 1;
      x := wherex;
      if (x + Offset) > 80 then
             New_x := 1
          else
             New_x := x + offset;
       gotoxy(New_x,wherey);
  loseit;
  end;

 procedure Ansi_Clear_Screen;

   begin                         {   0J = cusor to Eos           }
     Clrscr;                      {  1j start to cursor           }
     loseit;                       { 2j entie screen/cursor no-move}
   end;

 procedure Ansi_Clear_EoLine;

   begin
     clreol;
     loseit;
   end;


 procedure Reverse_Video;

 var
      tempAttr,tblink,tempAttrlo,tempAttrhi : Byte;

 begin
            LowVideo;
            TempAttrlo := (TextAttr and $7);
            tempAttrHi := (textAttr and $70);
            tblink     := (textattr and $80);
            tempattrlo := tempattrlo * 16;
            tempattrhi := tempattrhi div 16;
            TextAttr   := TempAttrhi+TempAttrLo+TBlink;
  end;


 procedure Ansi_Set_Colors;

 var
    temp0,Color_Code   : integer;

    begin
        if length(control_code) = 0 then control_code :='0';
           while (length(control_code) > 0) do
           begin
            Color_code := getNumber(control_code);
                case Color_code of
                   0          :  begin
                                   LowVideo;
                                   TextColor(LightGray);
                                   TextBackground(Black);
                                 end;
                   1          : HighVideo;
                   5          : TextAttr := (TextAttr or $80);
                   7          : Reverse_Video;
                   30         : textAttr := (TextAttr And $F8) + black;
                   31         : textattr := (TextAttr And $f8) + red;
                   32         : textattr := (TextAttr And $f8) + green;
                   33         : textattr := (TextAttr And $f8) + brown;
                   34         : textattr := (TextAttr And $f8) + blue;
                   35         : textattr := (TextAttr And $f8) + magenta;
                   36         : textattr := (TextAttr And $f8) + cyan;
                   37         : textattr := (TextAttr And $f8) + Lightgray;
                   40         : textbackground(black);
                   41         : textbackground(red);
                   42         : textbackground(green);
                   43         : textbackground(yellow);
                   44         : textbackground(blue);
                   45         : textbackground(magenta);
                   46         : textbackground(cyan);
                   47         : textbackground(white);
                 end;
             end;
       loseit;
  end;


 procedure Ansi_Save_Cur_pos;

    Begin
      Saved_X := WhereX;
      Saved_Y := WhereY;
      loseit;
    end;


 procedure Ansi_Restore_cur_pos;

    Begin
      GotoXY(Saved_X,Saved_Y);
      loseit;
    end;


 procedure Ansi_check_code( ch : char);

   begin
       case ch of
            '0'..'9',';'     : control_code := control_code + ch;
            'H','f'          : Ansi_Cursor_Move;
            'A'              : Ansi_Cursor_up;
            'B'              : Ansi_Cursor_Down;
            'C'              : Ansi_Cursor_Right;
            'D'              : Ansi_Cursor_Left;
            'J'              : Ansi_Clear_Screen;
            'K'              : Ansi_Clear_EoLine;
            'm'              : Ansi_Set_Colors;
            's'              : Ansi_Save_Cur_Pos;
            'u'              : Ansi_Restore_Cur_pos;
        else
          loseit;
        end;
   end;


procedure Ansi_Write(ch : char);

Var
  temp0      : Integer;

begin
       if escape > 0 then
          begin
              case Escape of
                1    : begin
                         if ch = '[' then
                            begin
                              escape := 2;
                              Control_Code := '';
                            end
                         else
                             escape := 0;
                       end;
                2    : Ansi_Check_code(ch);
              else
                begin
                   escape := 0;
                   control_code := '';
                end;
              end;
          end
       else
         Begin
          Case Ch of
             #27       : Escape := 1;
             #9        : Begin
                            temp0:= wherex;
                            temp0 := temp0 div 8;
                            temp0 := temp0 + 1;
                            temp0 := temp0 * 8;
                            gotoxy(temp0,wherey);
                         end;
             #12       : ClrScr;
          else
                 begin
                    if ((wherex = 80) and (wherey = 25)) then
                      begin
                        windmax := (80 + (24*256));
                        if ch <> #7 then write(ch);
                        windmax := (79 + (24*256));
                      end
                    else
                      If ch <> #7 then write(ch);
                    escape := 0;
                 end;
           end;
         end;
  End;
end.
