Unit BJSupp;
{========================================================================}
                              INTERFACE
{========================================================================}
Uses Crt, Dos;

Function CStr (var s : string) : String;
Function FExists(FileName: string) : Boolean;
Function KillTOPCodes (s : String) : String;
Function KillSpaces (s : String) : String;
Function Minus1 (s : string) : String;
Function PaddedNum (I : byte) : String;
Function Spaces (num : byte) : string;
Function ToStr (Number : LongInt) : String;
Function UpStr (s : string) : String;
Procedure WindowBorder (x : byte; y : byte; x2 : byte; y2 : byte;
                        Fcolor : byte; Bcolor: Byte);
Procedure TimeSlice (num : byte);

{========================================================================}
                            IMPLEMENTATION
{========================================================================}

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

Function KillTOPCodes (s : String) : String;
begin
  While Pos ('^',s) <> 0 do
    Delete (s, Pos ('^',s), 2);
  While s[Length(s)] = #32 do Dec (s[0]);
  KillTOPCodes := s;
end;

Function KillSpaces (s : String) : String;
begin
  While Pos(' ',s) <> 0 do
    Delete (s, Pos (' ',s), 1);
  While s[Length(s)] = #32 do Dec (s[0]);
  KillSpaces := s;
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

Function PaddedNum (I : byte) : String;
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

Procedure TimeSlice(num : byte);
var d: byte;
begin
    for d := 0 to (num - 1) do
    begin
      asm
        mov ax, 01680h
        int 02fh
      end
  end
end;

end.
