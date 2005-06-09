Unit TOPLSupp;
{========================================================================}
                              INTERFACE
{========================================================================}
Uses Crt, Dos, MulAware;

Function Ask2Save : Boolean;
Function CStr (var s : string) : String;
Procedure CursorNorm;
Procedure CursorOff;
Function FExists(FileName: string) : Boolean;
Function Get_Key : Word;
Procedure GetString (var UntypedString; Max_Length : Byte; Default : String);
Function KillAnsi (s : String) : String;
Function KillTOPCodes (s : String) : String;
Function Minus1 (s : string) : String;
Function MultiTaskerStr : String;
Function PaddedNum (I : Integer) : String;
Function Spaces (num : byte) : string;
Function ToStr (Number : LongInt) : String;
Function UpStr (s : string) : String;
Function VersionStr : String;
Procedure WindowBorder (x : byte; y : byte; x2 : byte; y2 : byte;
                        Fcolor : byte; Bcolor: Byte);
Procedure SaveScreen (var Scr);        { Scr is an array [1..4000] of byte }
Procedure RestoreScreen (var Scr);     { or char }
function FLock(Lock:byte; Handle: Word; Pos,Len: LongInt): Word;

{========================================================================}
                            IMPLEMENTATION
{========================================================================}

Function Ask2Save : Boolean;
Var
  Key : Char;
begin
  WindowBorder (22,6,42,8,White, Blue);
  GotoXY (23,7);
  CursorNorm;
  Write ('Save Changes (Y/N)');
  Repeat
    key := Upcase (Readkey);
  Until Key in ['Y','N'];
  If Key = 'Y' then Ask2Save := True else Ask2Save := False;
  TextAttr := $07;
  ClrScr;
end;

Function CStr (var s : string) : String;
Var
  l : byte;
  OutStr : String;
begin
  l := 0;
  While (s[l] <> #0) do
  begin
    Inc (l);
    OutStr[L] := s[l-1];
  end;
  OutStr[0] := Chr(L);
  CStr := OutStr;
end;

Procedure CursorOff;
Var
  Reg : Registers;
begin
  Reg.Ax := 1 shl 8;
  Reg.Cx := $14 shl 8;
  Intr ($10, Reg);
end;

Procedure CursorNorm;
Var
  Reg : Registers;
begin
  Reg.Ax := 1 shl 8;
  Reg.Cx := 6 shl 8 + 7;
  Intr ($10, Reg);
end;

Function FExists(FileName: string) : Boolean;
Var
  f: file;
begin
  {$I-}
  Assign(f, FileName);
  Reset(f);
  Close(f);
  {$I+}
  FExists := (IOResult = 0) and (FileName <> '');
end;

Function Get_Key : Word;
Var
  a_key : char;
begin
  a_key := Readkey;
  if a_key = #0 then
  begin
    a_key := Readkey;
    Get_Key := Ord (a_key) + 256
  end
  else Get_Key := Ord (a_key);
end;

Function KillAnsi (s : String) : String;
Var
  L : byte;
begin
  While Pos (#27,s) <> 0 do
  begin
    L := Pos (#27,s);
    Repeat
      Inc (L);
    Until not (S[L] in ['0'..'9',';','[']);
    Inc (l);
    Delete (s, Pos (#27,s), L - Pos (#27,s));
  end;
  While s[Length(s)] = #32 do Dec (s[0]);
  KillAnsi := s;
end;

Function KillTOPCodes (s : String) : String;
Var
  L : byte;
begin
  While Pos ('^',s) <> 0 do
    Delete (s, Pos ('^',s), 2);
  While s[Length(s)] = #32 do Dec (s[0]);
  KillTOPCodes := s;
end;

Procedure GetString (var UntypedString; Max_Length : Byte; Default : String);
Const
  Valid_Characters : Set of Char = [#1..#12,#14..#26, #28..#254];
Var
  ch : Char;
  s : String[1];
  CursorPos : Byte;
  x,y : Byte;
  l : Byte;
  a_string : String Absolute UntypedString;
  Old : String;
  First : Boolean;

Begin
  Old := A_String;
  X := WhereX;
  Y := WhereY;
  First := True;
  if Default[1] in Valid_Characters then a_string := Default else a_string[0] := #0;
  For l := Ord (a_string[0]) + 1 to Max_Length do a_string[l] := #32;
  a_string[0] := chr (max_length);
  Write (a_string);
  CursorPos := Max_length;
  While a_string[CursorPos - 1] = #32 do dec (CursorPos);
  GotoXY (X + CursorPos - 1, y);
  repeat
    If (Ord (a_string[0]) = Max_Length) and (a_string[Max_length] = #32) then Dec (a_string[0]);
    ch := Readkey;
    If First AND ((ch = #8) OR ((ch in Valid_Characters) AND (ch >= #32))) then
    begin
      a_string := '';
      GotoXY (x,y);
      CursorPos := 1;
      For L := 1 to Max_Length do Write (' ');
      GotoXY (x,y);
    end;
    if (ch = #8) then
    begin
      if CursorPos <> 1 then
      begin
        Dec (CursorPos);
        Delete (a_string, CursorPos, 1);
        GotoXY (X,Y);
        Write (a_string + ' ');
        GotoXY (X + CursorPos - 1, y);
      end;
    end
    else
    begin
      If ch in Valid_Characters then
      begin
        if (CursorPos < Max_Length+1) and (Length (a_string) < Max_Length) and
        (ch <> #13) then
        begin
          S := ch;
          Insert (s, a_string, CursorPos);
          GotoXY (X,Y);
          Write (a_string);
          GotoXY (X + CursorPos,y);
          Inc (CursorPos);
        end;
      end;
      If ch = #27 then
      begin
        a_string := old;
        ch := #13;
      end;
      If ch = #0 then
      begin
        ch := Readkey;
        Case ch of
          'K' : If CursorPos > 1 then Dec (CursorPos);
          'M' : If CursorPos < Max_Length then Inc (CursorPos);
          'S' :
          begin
            if a_string <> '' then
            begin
              Delete (a_string, CursorPos, 1);
              GotoXY (X,Y);
              Write (a_string + ' ');
            end;
          end;
          'G' : CursorPos := 1;
          'O' :
          begin
            CursorPos := Max_length;
            While a_string[CursorPos - 1] = #32 do dec (CursorPos);
          end;
        end;
        GotoXY (X + CursorPos - 1, y);
      end;
    end;
    First := False;
  Until (ch = #13);
  While a_string[Length(a_string)] = #32 do Dec (a_string[0]);
  GotoXY (X,Y);
end;

Function Minus1 (s : string) : String;
Var
  New : String;
  I : byte;
begin
  FillChar (New, SizeOf (New), #0);
  For I := 2 to Length (s) do New[I-1] := s[I];
  New[0] := Chr(Length (s) - 1);
  Minus1 := New;
end;

Function MultiTaskerStr : String;
Var
  a_str : String;
begin
  Case MultiTasker of
    None         : A_Str := 'DOS';
    DESQview     : A_Str := 'DESQview';
    WinEnh,WinStandard : A_Str := 'Windows';
    OS2          : A_Str := 'OS/2';
    DoubleDOS    : A_Str := 'DoubleDOS';
    MultiDos     : A_Str := 'MultiDos Plus';
    VMiX         : A_Str := 'VMiX';
    TopView      : begin
                     If MulVersion <> 0 then
                        A_Str := 'TopView'
                     Else
                        A_Str := 'TaskView, DESQview 2.00-2.25, OmniView, or Compatible';
                   end;
    TaskSwitcher : A_Str := 'DOS 5.0 Task Switcher or Compatible';
    WinNT        : A_Str := 'Windows NT';
  end;
  MultiTaskerStr := A_Str;
end;

Function PaddedNum (I : Integer) : String;
Var
  s : String;
begin
  Str (I,s);
  If I < 10 then s := '0'+s;
  If I < 100 then s := '0'+s;
  If I < 1000 then s := '0'+s;
  If I < 10000 then s := '0'+s;
  PaddedNum := s;
end;

Function Spaces (num : byte) : string;
Var
  L : byte;
  s : String;
begin
  s := '';
  For L := 1 to num do s := s + ' ';
  Spaces := s;
end;

Function ToStr (Number : LongInt) : String;
Var
  a_str : String;
begin
  Str (Number, a_str);
  ToStr := a_str;
end;

Function UpStr (s : string) : String;
Var
  L : byte;
begin
  For L := 1 to Length (s) do s[l] := Upcase (s[l]);
  UpStr := s;
end;

Function VersionStr : String;
Var
  A_Str : String;

begin
  If MultiTasker <> None then VersionStr := ToStr (Hi (MulVersion)) + '.' + ToStr (Lo (MulVersion))
  else VersionStr := ToStr (Lo (DosVersion)) + '.' + ToStr (Hi (DosVersion));
end;

Procedure WindowBorder (x : byte; y : byte; x2 : byte; y2 : byte;
                        Fcolor : byte; Bcolor: Byte);
Var
  Loop : Byte;
  VideoMem : Array [1..4000] of byte absolute $B800:0000;

begin { Window Border }
  { Change to user specified colors }
  Textcolor (FColor);  Textbackground (BColor);
  { Clear region for background color }
  Window (x,y,x2,y2);
  ClrScr;
  { Restore Window }
  Window (1,1,80,25);
  { Draw borders }
  GotoXY (x,y);
  Write ('Ú');
  For Loop := (x+1) to (x2-1) do
  begin
    GotoXY (loop, y);
    Write ('Ä');
  end;
  GotoXY (x2,y);
  Write ('¿');
  For Loop := (y+1) to (y2-1) do
  begin
    GotoXY (x, loop);
    Write ('³');
  end;
  GotoXY (x,y2);
  Write ('À');
  For Loop := (x+1) to (x2-1) do
  begin
    GotoXY (loop,y2);
    Write ('Ä');
  end;
  GotoXY (x2,y2);
  Write ('Ù');
  For Loop := (y+1) to (y2-1) do
  begin
    GotoXY (x2,loop);
    Write ('³');
  end;
  { Do horizontal shadow }
  For Loop := (x+2) to (x2+2) do VideoMem [2*Loop+Y2*160] := $08;
  { Do vertical shadow }
  For Loop := (y+1) to (y2) do
  begin
    VideoMem [2*(X2+1)+Loop*160] := $08;
    VideoMem [(2*(X2+1)+Loop*160)+2] := $08;
  end;
end; { Window Border }

Procedure RestoreScreen (var Scr);
begin
  If Lastmode <> 7 then Move (Scr, Ptr ($B800,0000)^, 4000)
  else Move (Scr, Ptr ($B000,0000)^, 4000);
end;

Procedure SaveScreen (var Scr);
begin
  If Lastmode <> 7 then Move (Ptr ($B800,0000)^, Scr, 4000)
  else Move (Ptr ($B000,0000)^, Scr, 4000);
end;

function FLock(Lock:byte; Handle: Word; Pos,Len: LongInt): Word; (* Assembler;
  ASM
      mov   AL,Lock   { subfunction 0: lock region   }
                      { subfunction 1: unlock region }
      mov   AH,$5C    { DOS function $5C: FLOCK    }
      mov   BX,Handle { put FileHandle in BX       }
      les   DX,Pos
      mov   CX,ES     { CX:DX begin position       }
      les   DI,Len
      mov   SI,ES     { SI:DI length lockarea      }
      int   $21       { Call DOS ...               }
      jb    @End      { if error then return AX    }
      xor   AX,AX     { else return 0              }
  @End:*)
begin
FLock := 0;
end {FLock};

end.
