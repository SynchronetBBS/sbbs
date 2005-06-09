{$M 16384,0,0}
{$V-}
Program Blackjack4TOP;
Uses DOS, Crt, BJSupp;

Type
  IDXRec = Record
             StructLength : Word;
             Alias : String[30];
             RealName : String[40];
             Baud : Word;
             Location : String[30];
             Gender : Integer;
             Quiet : Boolean;
             Task : Word;
             LastAccess : LongInt;
             Channel : LongInt;
             ChannelListed : Boolean;
             Security : Word;
             Actions : Byte;
           end;

  Str30 = String[30];
  Str255 = String[255];

  NodeRec = Record
             StructLength : Word;
             Kind : Integer;
             From : Integer;
             DoneTo : Integer;
             Gender : Integer;
             Alias : Str30;
             Data : Str255;
             Channel : LongInt;
             MinSec : Word;
             MaxSec : Word;
             Data1 : LongInt;
           end;

  ConfigRec = Record
                StartCD : Word;
                BetCD : Byte;
                MaxBet : Byte;
                MinBet : Byte;
                TimeBetweenGames : Byte;
                FirstWarn : Byte;
                SecondWarn : Byte;
                BootFromBJ : Byte;
              end;

Type
  RegisterRec = record
                  TheReg : string;
                  TheName : string;
                end;
Const
  RegisterData : RegisterRec
               = ( TheReg : '00000000000';
                   TheName : '[UNREGISTERED]');

  ProgVerStr = ' v2.00';

  fmReadOnly  = $00;
  fmWriteOnly = $01;
  fmReadWrite = $02;

  fmDenyAll   = $10;
  fmDenyWrite = $20;
  fmDenyRead  = $30;
  fmDenyNone  = $40;

  ConfigData : ConfigRec
               = ( StartCD : 1000;
                   BetCD : 10;
                   MaxBet : 100;
                   MinBet : 10;
                   TimeBetweenGames : 19;
                   FirstWarn : 60;
                   SecondWarn : 90;
                   BootFromBJ : 125
                  );

  HelpData : Array[1..14] of String = (
  #13#10'^A^oEnter ^A^n/BJ BET ^A^m<amount> ^A^oto change your bet (cannot be done during a game).',
  '^A^oEnter ^A^n/BJ CD ^A^oor ^A^n/BJ $ ^A^ofor your current number of Cyberdollars.',
  '^A^oEnter ^A^n/BJ DOUBLE x ^A^oto double down hand x.',
  '^A^oEnter ^A^n/BJ HAND ^A^oto view the dealer''s and your own cards.',
  '^A^oEnter ^A^n/BJ HELP ^A^ofor this commands list.',
  '^A^oEnter ^A^n/BJ HIT x ^A^oto get another card for hand x.',
  '^A^oEnter ^A^n/BJ INSURE ^A^oto make an insurance bet against a blackjack.',
  '^A^oEnter ^A^n/BJ OFF ^A^oto be dealt out of blackjack.',
  '^A^oEnter ^A^n/BJ ON ^A^oto join in the next game of blackjack.',
  '^A^oEnter ^A^n/BJ SCAN ^A^oto see who''s playing blackjack.',
  '^A^oEnter ^A^n/BJ SPLIT x ^A^oto split hand x.',
  '^A^oEnter ^A^n/BJ STAY ^A^oto end your turn.',
  '^A^oEnter ^A^n/BJ TURN ^A^oto see who''s turn it is.'#13#10,
  '^A^mIf ^lx^m is left off commands that need it, hand #^l1^m is assumed.'#13#10);

  ComList : Array[1..16] of String = ('BJ BET', 'BJ $',
  'BJ CD', 'BJ DOUBLE', 'BJ DOUBLE2', 'BJ HAND', 'BJ HELP', 'BJ HIT',
  'BJ HIT 2', 'BJ INSURE', 'BJ OFF', 'BJ ON', 'BJ SCAN', 'BJ SPLIT',
  'BJ STAY', 'BJ TURN');


Var
  IPCDir : String;
  Node : Byte;
  NodeStr : String;
  NodeData : NodeRec;
  NodeData2 : NodeRec;
  GamePlaying : Boolean;
  L : Word;
  Sec : Word;
  OldSec : Word;
  SecPassed : Word;
  NodeTurn : Integer;
  NodeStat : Array [1..255] of Byte; { 0 = Not in game }
                                     { 1 = Waiting }
                                     { 2 = In game }
  NodeCD : Array [1..255] of LongInt;
  NodeInsure : Array [1..255] of Boolean;
  NodeHand : Array [1..6, 1..255] of String[24];
  NodeDoneH : Array [1..6, 1..255] of Boolean;
  NodeBet : Array [1..255] of byte;
  NodeDD : Array [1..6, 1..255] of byte;
  NodeNumHands : Array [1..255] of byte;
  UsedCards : Array [1..6] of String[104];
  DealerHand : String[40];

Procedure InitScr;
Var
  Loop : Word;
begin
  TextAttr := $71;
  ClrScr;
  GotoXY (1,2);
  For loop := 1 to 1920 do Write ('°');
  Textbackground (blue);
  GotoXY (1,1);
  Textbackground (7);
  Textcolor (red);
  Write (' Blackjack for TOP'+ProgVerStr);
  Textcolor (0);
  Write (' Configuration (c) 1994 by David Ong, OmniWare(tm)');
  ClrEol;
  GotoXY (1,25);
  Textcolor (Red);
  Write (' Help: ');
  Textcolor (0);
  Write ('Use cursor keys to select a choice.');
  ClrEol;
  WindowBorder (4,3,60,13,White,7);
  TextAttr := $7F;
  GotoXY (27,3);
  Write ('´ Config Ã');
  GotoXY (6,4);
  Textcolor (0);
  Writeln ('Starting Cyberdollars:');
  GotoXY (6,5);
  Writeln ('Minimum bet:');
  GotoXY (6,6);
  Writeln ('Maximum bet: ');
  GotoXY (6,7);
  Writeln ('Time between games:');
  GotoXY (6,8);
{  For Loop := 1 to 9 do WriteField (Loop);}
end;

Procedure ConfigureBJ;
begin
  InitScr;
  Halt;
end;

Procedure SaveReg;
Var
  ExeFile    : file;
  HeaderSize : word;
  FileName : String;

begin
  FileName := ParamStr (0);
  Writeln ('Writing registration codes to ', FileName);
  Assign (ExeFile, FileName);
  Reset (ExeFile, 1);
  Seek      (ExeFile, 8);
  Blockread (ExeFile, HeaderSize, Sizeof (HeaderSize));
  Seek      (ExeFile, Longint(16) * (seg(RegisterData)
            - PrefixSeg + HeaderSize) + ofs (RegisterData) - 256);
  Blockwrite (ExeFile, RegisterData, sizeof (RegisterData));
  Close      (ExeFile);
end;

Procedure Enter_Reg;
begin
  Writeln;
  Writeln ('BJ4TOP Registration--Your registration code and named will be saved to');
  Writeln ('  the ', ParamStr(0), ' file.');
  Writeln;
  Write ('Your Name (Please note it is case sensitive): ');
  Readln (RegisterData.TheName);
  Write ('Registration Number:');
  Readln (RegisterData.TheReg);
  Writeln;
{  UserName := RegisterData.TheName;}
{  RegisterNum := RegisterData.TheReg;}
{  Check_Register;}
{  If Registered then Writeln ('The registration number checks out, thank you
for registering')}
{  else}
  begin
    Writeln ('The registration number doesn''t check out, make sure you''ve entered');
    Writeln ('everything correctly!'+#7);
    Writeln;
    Writeln ('Name and registration number not saved.');
    Halt (1);
  end;
  Writeln;
  If Not FExists (ParamStr (0)) then Writeln ('Error: The file '+ParamStr(0)+' could not be written to.'+#7)
  else SaveReg;
end;

Procedure InitProgram;
begin
  Randomize;
  Writeln;
  Writeln ('Blackjack for TOP'+ProgVerStr);
  Writeln ('Original BJ4RAP program Copyright 1995 David Ong, OmniWare(tm)');
  Writeln ('TOP modifications Copyright 1996 Paul Sidorsky, ISMWare, All Rights Reserved.');
  Writeln;
  If ParamCount < 1 then
  begin
    Writeln ('Command line parameter(s) missing!');
    Writeln;
    Writeln ('Usage: BJ4TOP <work path> [<node>]');
    Writeln;
    Writeln ('<work path>  Is the path to TOP''s work directory.');
    Writeln ('<node>       Is optional and if given will force BJ4TOP to use the node even');
    Writeln ('             if it is already taken.');
    Writeln;
    Writeln ('Eg: BJ4TOP d:\top\workdir');
    Writeln ('For further help refer to BJ4TOP.DOC');
    Halt;
  end;
{  RegisterKey := '#####';}
{  UserName := RegisterData.TheName;}
{  RegisterNum := RegisterData.TheReg;}
{  Check_Register;}
  If UpStr(ParamStr (1)) = '/REGISTER' then
  begin
    Enter_Reg;
    Halt;
  end;
{ If UpStr(ParamStr(1)) = '/CONFIG' then ConfigureBJ;}
  IPCDir := ParamStr(1);
  If IPCDir[Length(IPCDir)] <> '\' then IPCDir := IPCDir + '\';
  FileMode:=fmReadWrite+FmDenyNone;
  If not FExists (IPCDir + 'NODEIDX.TCH') then
  begin
    Writeln ('Error: The NODEIDX.TCH file was not found in the ', UpStr(IPCDir), ' directory!');
    Writeln ('Make sure TOP uses the same work path and has been previously run or is'#13#10'currently running.');
    Halt;
  end;
  If not FExists (IPCDir + 'NODEIDX2.TCH') then
  begin
    Writeln ('Error: The NODEIDX2.TCH file was not found in the ', UpStr(IPCDir), ' directory!');
    Writeln ('Make sure TOP uses the same work path and has been previously run or is'#13#10'currently running.');
    Halt;
  end;
end;

Function UnUsedNode : byte;
Var
  f : file of byte;
  L : byte;
  B : byte;
  Error : Integer;
begin
  If Paramcount < 2 then
  begin
    L := 0;
    Assign (F, IPCDir + 'NODEIDX2.TCH');
    Reset (f);
    Repeat
      Inc (l);
      {$I-}
      Repeat
        Read (f,b);
      Until IOResult = 0;
      {$I+}
    Until b = 0;
    Close (f);
  end
  else
  begin
    Val (ParamStr(2), L, Error);
    If (Error <> 0) or (L = 0) then
    begin
      Writeln ('Invalid forced node passed!');
      Writeln ('Node must be from 1 to 255');
      Halt;
    end;
  end;
  UnUsedNode := L;
end;

Procedure TakeNode;
Var
  L : byte;
  B : byte;
  f : File of byte;
  IDXFile : File of IDXRec;
  IDXData : IDXRec;
begin
  L := 0;
  Assign (f, IPCDir + 'MSG'+NodeStr+'.TCH');
  Rewrite (f);
  Close (f);
  Assign (f, IPCDir + 'MIX'+NodeStr+'.TCH');
  Rewrite (f);
  Close (f);
  Assign (F, IPCDir + 'NODEIDX2.TCH');
  Reset (f);
  Seek (f, Node);
  b := 1;
  {$I-}
  Repeat
    Write (f,b);
  Until IOResult = 0;
  {$I+}
  Close (f);
  Assign (IDXFile, IPCDir + 'NODEIDX.TCH');
  Reset (IDXFile);
  Seek (IDXFile, Node);
  FillChar (IDXData, SizeOf(IDXData), #0);
  IDXData.Alias := 'J'#8'lackjack Dealer'#0;
  IDXData.Alias[0] := 'B';
  IDXData.RealName := 'J'#0;
  IDXData.RealName[0] := 'B';
  IDXData.Baud := 0;
{  If registered then}
{  begin
    IDXData.Location := 'egistered'+#0;
    IDXData.Location[0] := 'R';
  end}
  {else}
  begin
    IDXData.Location := 'NREGISTERED'+#0;
    IDXData.Location[0] := 'U';
  end;
  IDXData.Gender := 0;
  IDXData.LastAccess := 0;
  IDXData.Channel := 1;
  IDXData.ChannelListed := True;
  IDXData.Security := 65535;
  {$I-}
  Repeat
    Write (IDXFile, IDXData);
  Until IOResult = 0;
  {$I+}
  Close (IDXFile);
end;

Procedure SendMSG (N : byte; NodeData : NodeRec);
Var
  b : byte;
  f : File of byte;
  NodeFile : File of NodeRec;
begin
  Assign (f, IPCDir + 'MIX' + PaddedNum(N)+'.TCH');
  Reset (f);
  b := 0;
  If not Eof (f) then
  Repeat
    {$I-}
    Repeat
      Read (f,b);
    Until IOResult = 0;
    {$I+}
    If Eof (f) then
    begin
      b := 0;
      Write (f,b);
    end;
  Until B = 0
  else Write (f,b);
  b := 1;
  Seek (f, FilePos(f) - 1);
  {$I-}
  Repeat
    Write (f,b);
  Until IOResult = 0;
  {$I+}
  Assign (NodeFile, IPCDir + 'MSG' + PaddedNum(N)+'.TCH');
  Reset (NodeFile);
  Seek (NodeFile, FilePos(f) - 1);
  {$I-}
  Repeat
    Write (NodeFile, NodeData);
  Until IOResult = 0;
  {$I+}
  Close (NodeFile);
  Close (f);
  Assign (F, IPCDir + 'CHGIDX.TCH');
  Reset (f);
  Seek (f, N);
  b := 1;
  {$I-}
  Repeat
    Write (f,b);
  Until IOResult = 0;
  {$I+}
  Close (f);
end;

Procedure BroadCast (T : Byte; Alias : Str30; Data : Str255);
Var
  IDX2 : File of byte;
  b : byte;
  L : Word;
  NodeData : NodeRec;
begin
  FillChar (NodeData, SizeOf(NodeData), #0);
  NodeData.Doneto := -1;
  Alias := Alias + #0;
  Data := Data + #0;
  NodeData.Kind := T;
  NodeData.From := Node;
  NodeData.Alias := Minus1 (Alias);
  NodeData.Alias[0] := Alias[1];
  NodeData.Data := Minus1 (Data);
  NodeData.Data[0] := Data[1];
  NodeData.Channel := 1;
  NodeData.MinSec := 0;
  NodeData.Data1 := 0;
  Assign (IDX2, IPCDir + 'NODEIDX2.TCH');
  {$I-}
  Repeat
    Reset (IDX2);
  Until IOResult = 0;
  {$I+}
  For L := 0 to (FileSize(IDX2) - 1) do
  begin
    {$I-}
    Repeat
      Read (IDX2, b);
    Until IOResult = 0;
    {$I+}
    If (L <> Node) and (b=1) and (NodeStat[L] <> 0) then SendMSG (L, NodeData);
  end;
  Close (IDX2);
end;

Procedure BroadCastAll (T : Byte; Alias : Str30; Data : Str255);
Var
  IDX2 : File of byte;
  b : byte;
  L : Word;
  NodeData : NodeRec;
begin
  FillChar (NodeData, SizeOf(NodeData), #0);
  NodeData.Doneto := -1;
  Alias := Alias + #0;
  Data := Data + #0;
  NodeData.Kind := T;
  NodeData.From := Node;
  NodeData.Alias := Minus1 (Alias);
  NodeData.Alias[0] := Alias[1];
  NodeData.Data := Minus1 (Data);
  NodeData.Data[0] := Data[1];
  NodeData.Channel := 1;
  NodeData.MinSec := 0;
  NodeData.Data1 := 0;
  Assign (IDX2, IPCDir + 'NODEIDX2.TCH');
  {$I-}
  Repeat
    Reset (IDX2);
  Until IOResult = 0;
  {$I+}
  For L := 0 to (FileSize(IDX2) - 1) do
  begin
    {$I-}
    Repeat
      Read (IDX2, b);
    Until IOResult = 0;
    {$I+}
    If (L <> Node) and (b=1) then SendMSG (L, NodeData);
  end;
  Close (IDX2);
end;

Procedure BroadCastAllBut (T : Byte; Alias : Str30; Data : Str255; NoSend : Byte);
Var
  IDX2 : File of byte;
  b : byte;
  L : Word;
  NodeData : NodeRec;
begin
  FillChar (NodeData, SizeOf(NodeData), #0);
  NodeData.Doneto := -1;
  Alias := Alias + #0;
  Data := Data + #0;
  NodeData.Kind := T;
  NodeData.From := node;
  NodeData.Alias := Minus1 (Alias);
  NodeData.Alias[0] := Alias[1];
  NodeData.Data := Minus1 (Data);
  NodeData.Data[0] := Data[1];
  NodeData.Channel := 1;
  NodeData.MinSec := 0;
  NodeData.Data1 := 0;
  Assign (IDX2, IPCDir + 'NODEIDX2.TCH');
  {$I-}
  Repeat
    Reset (IDX2);
  Until IOResult = 0;
  {$I+}
  For L := 0 to (FileSize(IDX2) - 1) do
  begin
    {$I-}
    Repeat
      Read (IDX2, b);
    Until IOResult = 0;
    {$I+}
    If (L <> Node) and (b=1) and (NoSend <> L) then SendMSG (L, NodeData);
  end;
  Close (IDX2);
end;

Procedure GiveMSG (T : Byte; Alias : Str30; Data : Str255; Node : Byte);
Var
  IDX2 : File of byte;
  b : byte;
  L : Word;
  NodeData : NodeRec;
begin
  FillChar (NodeData, SizeOf(NodeData), #0);
  NodeData.Doneto := -1;
  Alias := Alias + #0;
  Data := Data + #0;
  NodeData.Kind := T;
  NodeData.From := node;
  NodeData.Alias := Minus1 (Alias);
  NodeData.Alias[0] := Alias[1];
  NodeData.Data := Minus1 (Data);
  NodeData.Data[0] := Data[1];
  NodeData.Channel := 1;
  NodeData.MinSec := 0;
  NodeData.Data1 := 0;
  SendMSG (Node, NodeData);
end;

Procedure BroadCastBut (T : Byte; Alias : Str30; Data : Str255; NoSend : Byte);
Var
  IDX2 : File of byte;
  b : byte;
  L : Word;
  NodeData : NodeRec;
begin
  FillChar (NodeData, SizeOf(NodeData), #0);
  NodeData.Doneto := -1;
  Alias := Alias + #0;
  Data := Data + #0;
  NodeData.Kind := T;
  NodeData.From := node;
  NodeData.Alias := Minus1 (Alias);
  NodeData.Alias[0] := Alias[1];
  NodeData.Data := Minus1 (Data);
  NodeData.Data[0] := Data[1];
  NodeData.Channel := 1;
  NodeData.MinSec := 0;
  NodeData.Data1 := 0;
  Assign (IDX2, IPCDir + 'NODEIDX2.TCH');
  {$I-}
  Repeat
    Reset (IDX2);
  Until IOResult = 0;
  {$I+}
  For L := 0 to (FileSize(IDX2) - 1) do
  begin
    {$I-}
    Repeat
      Read (IDX2, b);
    Until IOResult = 0;
    {$I+}
    If (L <> Node) and (b=1) and (L <> NoSend) and (NodeStat[L] <> 0) then SendMSG (L, NodeData);
  end;
  Close (IDX2);
end;

Function NodeUser (I : Integer) : String;
Var
  IDXFile : File of IDXRec;
  IDXData : IDXRec;
begin
  If I > 0 then
  begin
    Assign (IDXFile, IPCDir + 'NODEIDX.TCH');
    Reset (IDXFile);
    Seek (IDXFile, I);
    {$I-}
    Repeat
      Read (IDXFile, IDXData);
    Until IOResult = 0;
    {$I+}
    NodeUser := CStr (IDXData.Alias);
    Close (IDXFile);
  end
  else NodeUser := 'everyone';
end;

Function NodeGender (I : Word) : Boolean;
Var
  IDXFile : File of IDXRec;
  IDXData : IDXRec;
begin
  If I > 0 then
  begin
    Assign (IDXFile, IPCDir + 'NODEIDX.TCH');
    Reset (IDXFile);
    Seek (IDXFile, I);
    {$I-}
    Repeat
      Read (IDXFile, IDXData);
    Until IOResult = 0;
    {$I+}
    Close (IDXFile);
    NodeGender := IDXData.Gender = 1;
  end
  else NodeGender := False;
end;

Procedure InitTOP;
begin
  Node := UnusedNode;
  Writeln ('Logging onto TOP as node ', Node);
  Str (Node, NodeStr);
  NodeStr := PaddedNum (Node);
  TakeNode;
  BroadCastAll (2, 'BJ'+#8+'lackjack Dealer', '');
  BroadCastAll (9, '', '^I^i*** Blackjack'+ProgVerStr+' Game Module Active ***');
  BroadCastAll (9, '', '^A^nThis program is registered to: ^A^p'+Registerdata.TheName);
{  If not Registered then}
    BroadCastAll (9, '', '^I^oBe sure to register soon!');
  BroadCastAll (9, '', '^A^nType ^A^p/BJ HELP ^A^nfor the commands.');
end;

Procedure DeInit;
Var
  B : byte;
  f : File of byte;
begin
  BroadCastAll (3, '^A^mThe ^A^pBlackjack Dealer','');
  BroadCastAll (9, '', '^I^i*** Blackjack'+ProgVerStr+' Game Module Deactivated ***');
  Assign (F, IPCDir + 'NODEIDX2.TCH');
  Reset (f);
  Seek (f, node);
  b := 0;
  {$I-}
  Repeat
    Write (f,b);
  Until IOResult = 0;
  {$I+}
  Close (f);
  TextAttr := $07;
  Window (1,1,80,25);
  ClrScr;
end;

Procedure InitScreen;
begin
  Delay (1000);
  ClrScr;
  TextAttr := $71;  { Grey on blue }
  { Put background in, skipping the bottom line to create a status bar }
  For L := 1 to 1920 do Write ('°');
  TextAttr := $74;
  GotoXY (1,1);   ClrEol;
  GotoXY (3,1);  Writeln ('Blackjack for TOP'+ProgVerStr+' - Copyright 1996 Paul Sidorsky - All Rights Reserved');
  WindowBorder (5,3,74,21, White, Blue);
  GotoXY (1,25);
  TextAttr := $70;
  Write (' Press <ESC> to deactivate blackjack');
  ClrEol;
{  If not registered then}
  begin
    GotoXY (67,25);
    Textcolor (Red+Blink);
    Textbackground (LightGray);
    Write ('Unregistered');
  end;
{  else
  begin
    GotoXY (67,25);
    Textcolor (Red);
    Textbackground (LightGray);
    Write ('Registered');
  end;}
  Window (6,4,73,20);
  Textbackground (Blue);
  Textcolor (Yellow);
  Writeln ('Waiting for people to join blackjack.');
end;

Procedure InitVars;
Var
  D : integer;
begin
  For L := 1 to 255 do
  begin
    NodeStat[L] := 0;
    NodeCD[L] := ConfigData.StartCD;
    NodeBet[L] := ConfigData.BetCD;
    NodeInsure[L] := False;
    For D := 1 to 6 do
    begin
      NodeHand[D, L] := '';
      NodeDD[D, L] := 1;
      NodeDoneH[D, L] := False;
    end;
    NodeNumHands[L] := 0;
  end;
  For D := 1 to 6 do UsedCards[D] := '';
end;

Function MSGWait : boolean;
Var
  b : byte;
  f : File of byte;
begin
  Assign (f, IPCDir + 'CHGIDX.TCH');
  Reset (f);
  Seek (f, node);
  {$I-}
  Repeat
    Read (f,b);
  Until IOResult = 0;
  {$I+}
  MSGWait := b = 1;
  Close (f);
end;

Function DoneAllHands(P : Word) : Boolean;
Var
  H : Word;
  C : Word;
begin
  C := 0;
  For H := 1 to NodeNumHands[P] do
    If NodeDoneH[H, P] then Inc(C);
  If C = NodeNumHands[P] then DoneAllHands := True else DoneAllHands := False;
end;

Function OkayToSplit(N : Word; P : Word) : Boolean;
Var
  H : Word;
  C : Word;
begin
  OkayToSplit := False;
  C := 0;
  If NodeNumHands[P] < 6 then
  begin
    For H := 1 to NodeNumHands[P] do
      If Length(NodeHand[H, P]) = 4 then
        Inc(C);
    If C = NodeNumHands[P] then OkayToSplit := True;
  end;
end;

Function GetCard : String;
Const
  Cards : Array [1..13] of Char = 'A23456789TJQK';
  Kinds : Array [1..4] of Char = 'HDSC';
Var
  Value, Kind : Byte;
  Card : String[6];
  Hand : Word;
begin
  Hand := Random (6)+1;
  Repeat
    Value := Random (13)+1;
    Kind := Random (4)+1;
    Card := Cards[Value] + Kinds [Kind];
  Until Pos (Card, UsedCards[Hand]) = 0;
  UsedCards[Hand] := UsedCards[Hand] + Card;
  { Shuffle once only 6 cards are left in that deck. }
  If Length (UsedCards[Hand]) >= 92 then UsedCards[Hand] := '';
  GetCard := Card;
end;

Function HandValue (Hand : String) : Byte;
Var
  Value : Byte;
  Aces : Byte;
begin
  Value := 0;
  { Count aces }
  Aces := 0;
  For L := 1 to Length (Hand) do if Hand[L] = 'A' then Inc (Aces);
  While Hand[0] > #1 do
  begin
    If Hand[1] = 'A' then Value := Value + 11;
    If Hand[1] in ['T','J','Q','K'] then Value := Value + 10;
    If Hand[1] in ['2'..'9'] then Value := Value + Ord(Hand[1])-48;
    Delete (Hand, 1, 2);
  end;
  While (Value > 21) and (Aces <> 0) do
  begin
    Dec (Value,10);
    Dec (Aces);
  end;
  HandValue := Value;
end;

Function ColorHand (Hand : String) : String;
Var
  New : String;
begin
  New := '';
  While Hand[0] > #1 do
  begin
    If Hand[2] in ['D','H'] then New := New + '^H^e'+Hand[1]+Hand[2]+'^A^k '
    else
    If Hand[2] in ['C','S'] then New := New + '^H^a'+Hand[1]+Hand[2]+'^A^k '
    else New := New + '^H^a  ^A^k';
    Delete (Hand, 1, 2);
  end;
  ColorHand := New;
end;

Procedure NextPlayer;
Var
  I : Word;
  X : Byte;
begin
  If NodeStat[NodeTurn] <> 0 then
  begin
    X := 0;
    For I := 1 to NodeNumHands[NodeTurn] Do
    begin
      If NodeHand[I, NodeTurn] <> '' then
        If HandValue (NodeHand[I, NodeTurn]) > 21 then Inc(X);
    end;
    if X >= NodeNumHands[NodeTurn] then NodeStat[NodeTurn] := 1;
  end;
  If NodeStat[NodeTurn] = 1 then
  begin
    If NodeInsure[NodeTurn] then
    begin
      Dec (NodeCD[NodeTurn], Round (NodeBet[NodeTurn]/2));
      GiveMSG (9, '', '^A^pYou^I^n lose ^A^nyour insurance bet of ^A^p'+ToStr(Round(NodeBet[NodeTurn]/2))
               +' ^A^nCyberdollars!', NodeTurn);
      If NodeGender (NodeTurn) then
        BroadCastBut (9, '', '^A^p'+NodeUser(NodeTurn)+'^I^n loses ^A^nher insurance bet of ^A^p'
                      +ToStr(Round(NodeBet[NodeTurn]/2))+' ^A^nCyberdollars!', NodeTurn)
      else
        BroadCastBut (9, '', '^A^p'+NodeUser(NodeTurn)+'^I^n loses ^A^nhis insurance bet of ^A^p'
                      +ToStr(Round(NodeBet[NodeTurn]/2))+' ^A^nCyberdollars!', NodeTurn)
    end;
  end;
  I := NodeTurn;
  Repeat
    Inc (I);
  Until (I = 256) or (NodeStat[I] = 2);
  If I = 256 then NodeTurn := -1       { Dealer's turn }
    else NodeTurn := I;
  If NodeTurn > 1 then
  begin
    NodeData2.DoneTo := -1;
    NodeData2.From := node;
    NodeData2.Gender := 0;
    NodeData2.Alias := '';
    NodeData2.Kind := 9;
    NodeData2.Data := 'A^mIt is your turn in ^A^pblackjack^A^m!'#0;
    NodeData2.Data[0] := '^';
    NodeData2.Channel := 1;
    NodeData2.MinSec := 0;
    NodeData2.Data1 := 0;
    SendMSG (NodeTurn, NodeData2);
  end;
  SecPassed := 0;
  If NodeTurn > 1 then Writeln ('It is ', NodeUser(NodeTurn), '''s turn in blackjack.')
  else Writeln ('It is the Dealer''s turn in blackjack.');
end;

Procedure DisplayHand (Node : Word);
var
  X : Word;
begin
  GiveMSG (9, '', '^A^nDealer''s cards: '+ColorHand(DealerHand[1]+DealerHand[2])+'^H^a  ^A^n  (^A^p'
           +ToStr(HandValue(DealerHand[1]+DealerHand[2]))+'^A^n)', Node);
  If NodeNumHands[Node] > 1 then
  begin
    For X := 1 to NodeNumHands[Node] do
      If not NodeDoneH[X, Node] then
      GiveMSG (9, '', '^A^nHand #'+ToStr(X)+':      '+ColorHand(NodeHand[X, Node])+' ^A^n(^A^p'
               +ToStr(HandValue(NodeHand[X, Node]))+'^A^n)', Node);
  end
  else
    GiveMSG (9, '', '^A^nYour cards:     '+ColorHand(NodeHand[1, Node])+' ^A^n(^A^p'
             +ToStr(HandValue(NodeHand[1, Node]))+'^A^n)', Node);
end;

Procedure RegRemind;
Var
  N : Byte;
begin
  N := Random (4);
  Case N of
    0 : Broadcast (9,'','From ^A^lBlackjack Dealer^A^o, holding up a big placard: ^A^kRemember to register Blackjack!');
    1 : BroadCast (9,'','^I^pThis version of Blackjack for TOP is UNREGISTERED!^A^o');
    2 : BroadCast (9,'','From ^A^lBlackjack Dealer^A^o, in a hypnotic tone: ^A^kRegister blackjack, Register blackjack...');
    3 : BroadCast (9,'','From ^A^lBlackjack Dealer^A^o, over the bar''s PA system: ^A^kBlackjack is UNREGISTERED!');
  end;
end;

Procedure Retal (Node : Word);
Var
  N : Byte;
  J : Byte;
  T : LongInt;
begin
  N := Random (30);
  If N > 9 then
  begin
    J := Random (9);
    Case J of
    0 : begin
          GiveMSG (9, '', '^oThe ^A^lBlackjack Dealer ^A^ois booting ^A^lyou ^A^oto the head!',
                      Node);
          BroadCastAllBut (9, '', '^oThe ^A^lBlackjack Dealer ^A^ois booting ^A^l'+NodeUser(Node)+' ^A^oto the head!', Node);
        end;
    1 : begin
          GiveMSG (9, '', '^oThe ^A^lBlackjack Dealer ^A^ois frowning darkly at ^A^lyou^A^o!',
                      Node);
          BroadCastAllBut (9, '', '^oThe ^A^lBlackjack Dealer ^A^ois frowning darkly at ^A^l'+NodeUser(Node)+'^A^o!', Node);
        end;
    2 : begin
          GiveMSG (9, '', '^oThe ^A^lBlackjack Dealer ^A^ojust smacked ^A^lyou ^A^oacross the back of the head!',
                      Node);
          BroadCastAllBut (9, '', '^oThe ^A^lBlackjack Dealer ^A^ojust smacked ^A^l'+NodeUser(Node)
                        +' ^A^oacross the back of the head!', Node);
        end;
    3 : If NodeStat[Node] > 0 then
          BroadCastAll (9, '^oThe Blackjack Dealer',
              '^oFrom ^A^lThe Blackjack Dealer^A^o, grumbling unhappily: ^A^kStop that, '
                        +NodeUser(Node)+', or I''ll kick you out of the game!');
    4 : If NodeStat[Node] > 0 then
          BroadCastAll (9, '^oThe Blackjack Dealer',
              '^oFrom ^A^lThe Blackjack Dealer^A^o, grumbling unhappily: ^A^kStop that, '
                        +NodeUser(Node)+', or I''ll have to fine you!');
    5 : begin
          GiveMSG (9, '', '^oThe ^A^lBlackjack Dealer ^A^ojust elbowed ^A^lyou ^A^oin the face!',
                      Node);
          BroadCastAllBut (9, '', '^oThe ^A^lBlackjack Dealer ^A^ojust elbowed ^A^l'+NodeUser(Node)+' ^A^oin the face!', Node);
        end;
    6 : BroadCastAll (9, '', '^oThe ^A^lBlackjack Dealer ^A^ojust put up ^A^lhis^A^o deflector shields!');
    7 : begin
          GiveMSG (9, '', '^oThe ^A^lBlackjack Dealer ^A^ojust poked ^A^lyou ^A^oin the ribs!',
                      Node);
          BroadCastAllBut (9, '', '^oThe ^A^lBlackjack Dealer ^A^ojust poked ^A^l'+NodeUser(Node)+' ^A^oin the ribs!', Node);
        end;
    8 : begin
          GiveMSG (9, '', '^oThe ^A^lBlackjack Dealer ^A^ojust gave ^A^lyou ^A^oa good, swift, kick in the butt!',
                      Node);
          BroadCastAllBut (9, '', '^oThe ^A^lBlackjack Dealer ^A^ojust gave ^A^l'+NodeUser(Node)
                           +' ^A^oa good, swift, kick in the butt!', Node);
        end;
    end;
  end
  else
  If ((N = 9) or (N = 8) or (N = 15)) and (NodeStat[NodeData.From] > 0) then
  begin
    If NodeStat[nodedata.from] = 2 then
    begin
      For J := 1 to NodeNumHands[J] do
        If HandValue (NodeHand[J, Node]) < 22 then
          Dec (NodeCD[Node], NodeBet[Node]*NodeDD[J, Node]);
      If NodeInsure[Node] then Dec (NodeCD[Node], Round(NodeBet[Node]/2));
    end;
    NodeStat[Node] := 0;
    BroadCastBut (9,'','^A^nThe ^A^pblackjack dealer ^A^njust tossed ^A^p'+(NodeUser(Node))
                  +' ^A^nout of the ^A^pblackjack^A^n game!', Node);
    GiveMSG (9,'','^A^nThe angry ^A^pblackjack dealer ^A^njust tossed ^A^pyou ^A^nout of the ^A^pblackjack^A^n game!',
                  Node);
    Writeln (NodeUser(Node), ' is dealt out of the blackjack game.');
    If (nodedata.from) = NodeTurn then NextPlayer;
  end;
  If (N = 25) and (NodeStat[NodeData.From] > 0) then
  begin
    T := (((random(5) + 4) MOD 3) + 1) * 100;
    Dec(NodeCD[NodeData.From], T);
    BroadCastBut (9,'','^A^nThe ^pblackjack dealer ^njust ^I^pfined^A '+(NodeUser(Node))
                  +' ^l'+ ToStr(T) +'^n CyberDollars!', Node);
    GiveMSG (9,'','^A^nThe ^pblackjack dealer ^njust ^I^pfined^A you'
             +' ^l'+ ToStr(T) +'^n CyberDollars!', Node);
    Writeln(NodeUser(Node), ' has been fined ', T, 'CDs.');
  end;
end;

Procedure Process (NodeData : NodeRec);
Var
  s : String;
  I : Word;
  L : LongInt;
  Error : Integer;
  J : Word;
  H : Word;
begin
  NodeData2.DoneTo := -1;
  NodeData2.From := node;
  NodeData2.Gender := 0;
  NodeData2.Alias := '';
  NodeData2.Kind := 9;
  NodeData2.Channel := 1;
  NodeData2.MinSec := 0;
  NodeData2.Data1 := 0;
  If NodeData.Kind = 6 then
  begin
    s := KillSpaces(UpStr(CStr(NodeData.Data)));
    If s = 'ON' then
    begin
      If NodeStat[NodeData.From+1] = 0 then
      begin
        GiveMSG (9, '', '^A^nOk, you will now be playing in the next ^A^pblackjack ^A^ngame!',
                 NodeData.from);
        NodeStat[NodeData.from] := 1;
        BroadCastBut (9,'','^A^p'+CStr(NodeData.Alias)+' ^A^nwill be playing in the next ^A^pblackjack^A^n game!',
                      NodeData.from);
        Writeln (CStr(NodeData.Alias), ' joins the blackjack game.');
      end
      else
        GiveMSG (9, '', '^A^nYou are already playing ^A^pblackjack^A^n!', NodeData.from);
    end;
    If s = 'OFF' then
    begin
      If NodeStat[NodeData.from] = 0 then
        GiveMSG (9, '', '^A^nYou aren''t playing in ^A^pblackjack^A^n!', NodeData.from)
      else
      begin
        GiveMSG (9, '', '^A^nOk, you are no longer playing ^A^pblackjack^A^n.', NodeData.from);
        If NodeStat[NodeData.from] = 2 then
        begin
          For H := 1 to NodeNumHands[NodeData.from] do
          Dec (NodeCD[NodeData.from], NodeBet[NodeData.from]*NodeDD[H, NodeData.from]);
          NextPlayer;
        end;
        BroadCastBut (9,'','^A^p'+CStr(NodeData.Alias)+' ^A^nask to be dealt out of the ^A^pblackjack^A^n game!',
                      NodeData.from);
        Writeln (CStr(NodeData.Alias), ' is dealt out of the blackjack game.');
        NodeStat[NodeData.from] := 0;
      end;
    end;
    If (s='INSURE')or(s='INSURANCE')or(s='INSUR')or(s='INS')or(s='INSUER') then
    begin
      If (NodeStat[NodeData.from] = 2) then
      begin
        If NodeData.from = NodeTurn then
        begin
          If NodeInsure[NodeData.from] then
            GiveMSG (9, '', '^A^nYou''ve already made an insurance bet!', NodeData.from)
          else
          begin
            If (Length (NodeHand[1, NodeData.from]) = 4) and (NodeNumHands[NodeData.from] = 1) then
            begin
              GiveMSG (9, '', '^A^nOk, you make an insurance bet.', NodeData.from);
              BroadcastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^nmakes an insurance bet.', NodeData.from);
              NodeInsure[NodeData.from] := True;
            end
            else GiveMSG (9, '', '^A^nYou can''t buy insurance now!', NodeData.from);
          end;
        end
        else
          GiveMSG (9, '', '^A^nIt isn''t your turn!', NodeData.from);
      end
      else
        GiveMSG (9, '', '^A^nYou aren''t playing in the ^A^pblackjack^A^n game.', NodeData.from);
    end;
    If (s='STAY')or(s='STA')or(s='STY')or(s='SAY')or(s='TAY')or(s='STYA')or(s='STAYU')or
       (s='STAND') then
    begin
      If (NodeStat[NodeData.from] = 2) then
      begin
        If NodeData.from = NodeTurn then
        begin
          GiveMSG (9, '', '^A^nOk, you stay.', NodeData.from);
          BroadcastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^nstays.', NodeData.from);
          NextPlayer;
        end
        else
          GiveMSG (9, '', '^A^nIt isn''t your turn!', NodeData.from);
      end
      else
        GiveMSG (9, '', '^A^nYou aren''t playing in the ^A^pblackjack^A^n game.', NodeData.from);
    end;
    If (s='HAND')or(s='HAN')or(s='HADN')or(s='AHND') then
    begin
      If NodeStat[NodeData.from] = 2 then DisplayHand (NodeData.from)
      else
        GiveMSG (9, '', '^A^nYou aren''t playing in the ^A^pblackjack^A^n game.', NodeData.from);
    end;
    If (s = 'CD') or (s = '$') then
      GiveMSG (9, '', '^A^nYou have ^A^p'+ ToStr(NodeCD[NodeData.from])+'^A^n Cyberdollars.', NodeData.from);
    If Copy(s, 1, 3) = 'HIT' then
    begin
      If NodeStat[NodeData.from] = 2 then
      begin
        If NodeData.from = NodeTurn then
        begin
          If Length(s) = 3 then H := 1 else Val(Copy(s, 4, 1), H, Error);
          if H > NodeNumHands[NodeData.from] then GiveMsg(9, '', '^A^nYou don''t have '+ToStr(H)+' hands!', NodeData.from)
          else
          If NodeDoneH[H, NodeData.from] then GiveMSG (9, '', '^A^nYou can''t play that hand!', NodeData.from)
          else
          begin
            if NodeNumHands[NodeData.from] = 1 then
            BroadcastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^nhits.', NodeData.from)
            else
            BroadcastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^nhits hand #'+ToStr(H)+'.', NodeData.from);
            NodeHand[H, NodeData.from] := NodeHand[H, NodeData.from] + GetCard;
            DisplayHand (NodeData.from);
            SecPassed := 0;
            Writeln (NodeUser(NodeTurn), ' hits.');
            If HandValue(NodeHand[H, NodeData.from]) > 21 then
            begin
              GiveMSG (9, '', '^I^nYou busted!', NodeData.from);
              BroadCastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^nbusts!', NodeData.from);
              BroadCastBut (9, '', '^A^p'+NodeUser(NodeTurn)+'^I^n loses ^A^p'+ToStr(NodeBet[NodeTurn])+' ^A^nCyberdollars!'
                            , NodeData.from);
              GiveMSG (9, '', '^A^pYou^I^n lose ^A^p'+ToStr(NodeBet[NodeTurn])+' ^A^nCyberdollars!', NodeData.from);
              Dec (NodeCD[NodeData.from], NodeBet[NodeData.from]);
              Writeln (NodeUser(NodeTurn), ' busts.');
              NodeDoneH[H, NodeData.from] := True;
              If DoneAllHands(NodeData.from) then NextPlayer;
            end;
          end;
        end
        else
          GiveMSG (9, '', '^A^nIt is not your turn!', NodeData.from);
      end
      else
        GiveMSG (9, '',  '^A^nYou aren''t playing in the ^A^pblackjack^A^n game.', NodeData.from);
    end;
{    If (s='HIT2')or(s='HTI2')or(s='HI2T')or(s='IHT2') then
    begin
      If NodeStat[NodeData.from] = 2 then
      begin
        If NodeData.from = NodeTurn then
        begin
          If NodeHand2[NodeData.from] = '' then
            GiveMSG (9, '', '^A^nYou don''t have a second hand!', NodeData.from)
          else
          begin
            If NodeDoneH2[NodeData.from] then GiveMSG (9, '', '^A^nYou can''t play that hand!', NodeData.from)
            else
            begin
              BroadcastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^nhits second hand.', NodeData.from);
              NodeHand2[NodeData.from] := NodeHand2[NodeData.from] + GetCard;
              DisplayHand (NodeData.from);
              SecPassed := 0;
              Writeln (NodeUser(NodeTurn), ' hits second hand.');
              If HandValue(NodeHand2[NodeData.from]) > 21 then
              begin
                GiveMSG (9, '', '^I^nYou busted!', NodeData.from);
                BroadCastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^nbusts!', NodeData.from);
                BroadCastBut (9, '', '^A^p'+NodeUser(NodeTurn)+'^I^n loses ^A^p'+ToStr(NodeBet[NodeTurn])+' ^A^nCyberdollars!'
                              , NodeData.from);
                GiveMSG (9, '', '^A^pYou^I^n lose ^A^p'+ToStr(NodeBet[NodeTurn])+' ^A^nCyberdollars!', NodeData.from);
                Dec (NodeCD[NodeData.from], NodeBet[NodeData.from]);
                Writeln (NodeUser(NodeTurn), ' busts.');
                NodeDoneH2[NodeData.from] := True;
                If NodeDoneH1[NodeData.from] and NodeDoneH2[NodeData.from] then NextPlayer;
              end;
            end;
          end;
        end
        else
          GiveMSG (9, '', '^A^nIt is not your turn!', NodeData.from);
      end
      else
        GiveMSG (9, '',  '^A^nYou aren''t playing in the ^A^pblackjack^A^n game.', NodeData.from);
    end;}
    If (s = 'HELP') or (s = '!') or (s = '?') or (s = 'COMMANDS') then
    begin
      For I := 1 to 14 do
        GiveMSG (9, '', HelpData[I], NodeData.from);
    end;
    If Copy(s, 1, 5) = 'SPLIT' then
    begin
      If NodeStat[NodeData.from] = 2 then
        If NodeData.from = NodeTurn then
        begin
          If Length(s) = 5 then H := 1 else Val(Copy(s, 6, 1), H, Error);
          if H > NodeNumHands[NodeData.from] then GiveMsg(9, '', '^A^nYou don''t have '+ToStr(H)+' hands!', NodeData.from)
          else
          If OkayToSplit(H, NodeData.from) then
          begin
            If HandValue (NodeHand[H, NodeData.from][1]+NodeHand[H, NodeData.from][2])
               = HandValue (NodeHand[H, NodeData.from][3]+NodeHand[H, NodeData.from][4]) then
            begin
              GiveMSG (9, '', '^A^nOk, you split.', NodeData.from);
              SecPassed := 0;
              If NodeNumHands[NodeData.from] = 1 then
              BroadcastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^nsplits.', NodeData.from)
              else
              BroadcastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^nsplits hand #'+ToStr(H)+'.', NodeData.from);
              Inc(NodeNumHands[NodeData.from]);
              NodeHand[NodeNumHands[NodeData.from], NodeData.from] :=
                NodeHand[H, NodeData.from][3]+NodeHand[H, NodeData.from][4];
              Dec (NodeHand[H, NodeData.from][0], 2);
              NodeHand[H, NodeData.from] := NodeHand[H, NodeData.from] + GetCard;
              NodeHand[NodeNumHands[NodeData.From], NodeData.from] :=
                NodeHand[NodeNumHands[NodeData.From], NodeData.from] + GetCard;
              DisplayHand (NodeData.from);
            end
            else GiveMSG (9, '', '^A^nYou can''t split when card values are different!', NodeData.from)
          end
          else GiveMSG (9, '', '^A^nYou can''t split now!', NodeData.from)
        end
        else
          GiveMSG (9, '', '^A^nIt is not your turn!', NodeData.from)
      else
        GiveMSG (9, '',  '^A^nYou aren''t playing in the ^A^pblackjack^A^n game.', NodeData.from);
    end;
    if Copy(s, 1, 2) = 'DD' then s := 'DOUBLE' + s[3];
    if Copy(s, 1, 6) = 'DOUBLE' then
    begin
      If NodeStat[NodeData.from] = 2 then
      begin
        If NodeData.from = NodeTurn then
        begin
          If Length(s) = 6 then H := 1 else Val(Copy(s, 7, 1), H, Error);
          if H > NodeNumHands[NodeData.from] then GiveMsg(9, '', '^A^nYou don''t have '+ToStr(H)+' hands!', NodeData.from)
          else
          If NodeDoneH[H, NodeData.from] then GiveMSG (9, '', '^A^nYou can''t play that hand!', NodeData.from)
          else
          if Length(NodeHand[H, NodeData.from]) <= 4 then
          begin
            if NodeNumHands[NodeData.from] = 1 then
            BroadcastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^ndoubles down.', NodeData.from)
            else
            BroadcastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^ndoubles down hand #'+ToStr(H)+'.', NodeData.from);
            NodeHand[H, NodeData.from] := NodeHand[H, NodeData.from] + GetCard;
            NodeDD[H, NodeData.from] := 2;
            GiveMSG (9, '', '^A^nOk, you ^A^pdouble down^A^n!', NodeData.from);
            DisplayHand (NodeData.from);
            SecPassed := 0;
            Writeln (NodeUser(NodeTurn), ' doubles down.');
            If HandValue(NodeHand[H, NodeData.from]) > 21 then
            begin
              GiveMSG (9, '', '^I^nYou busted!', NodeData.from);
              BroadCastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^nbusts!', NodeData.from);
              BroadCastBut (9, '', '^A^p'+NodeUser(NodeTurn)+'^I^n loses ^A^p'+ToStr(NodeBet[NodeTurn]*NodeDD[H, NodeTurn])
                            +' ^A^nCyberdollars!', NodeData.from);
              GiveMSG (9, '', '^A^pYou^I^n lose ^A^p'+ToStr(NodeBet[NodeTurn]*NodeDD[H, NodeTurn])+' ^A^nCyberdollars!',
                       NodeData.from);
              Dec (NodeCD[NodeData.from], NodeBet[NodeData.from]*NodeDD[H, NodeData.from]);
              Writeln (NodeUser(NodeTurn), ' busts.');
            end;
            NodeDoneH[H, NodeData.from] := True;
            if DoneAllHands(NodeData.from) then NextPlayer;
          end
          else
            GiveMSG (9, '', '^A^nYou can''t double down after hitting!', NodeData.from);
        end
        else
          GiveMSG (9, '', '^A^nIt is not your turn!', NodeData.from);
      end
      else
        GiveMSG (9, '',  '^A^nYou aren''t playing in the ^A^pblackjack^A^n game.', NodeData.from);
    end;
{    If (s = 'DOUBLE2') or (s = 'DOUBLEDOWN2') or (s = 'DD2') then
    begin
      If NodeStat[NodeData.from] = 2 then
      begin
        If NodeData.from = NodeTurn then
        begin
          If NodeHand2[NodeData.from] = '' then
            GiveMSG (9, '', '^A^nYou don''t have a second hand!', NodeData.from)
          else
          begin
            if Length(NodeHand2[NodeData.from]) <= 4 then
            begin
              BroadcastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^ndoubles down on second hand.', NodeData.from);
              NodeHand2[NodeData.from] := NodeHand2[NodeData.from] + GetCard;
              NodeDD2[NodeData.from] := 2;
              GiveMSG (9, '', '^A^nOk, you ^A^pdouble down^A^n!', NodeData.from);
              DisplayHand (NodeData.from);
              SecPassed := 0;
              Writeln (NodeUser(NodeTurn), ' doubles down on second hand.');
              If HandValue(NodeHand2[NodeData.from]) > 21 then
              begin
                GiveMSG (9, '', '^I^nYou busted!', NodeData.from);
                BroadCastBut (9, '', '^A^p'+NodeUser(NodeTurn)+' ^A^nbusts!', NodeData.from);
                BroadCastBut (9, '', '^A^p'+NodeUser(NodeTurn)+'^I^n loses ^A^p'+ToStr(NodeBet[NodeTurn]*NodeDD2[NodeTurn])
                              +' ^A^nCyberdollars!', NodeData.from);
                GiveMSG (9, '', '^A^pYou^I^n lose ^A^p'+ToStr(NodeBet[NodeTurn]*NodeDD2[NodeTurn])+' ^A^nCyberdollars!',
                         NodeData.from);
                Dec (NodeCD[NodeData.from], NodeBet[NodeData.from]*NodeDD2[NodeData.from]);
                Writeln (NodeUser(NodeTurn), ' busts.');
              end;
              NodeDoneH2[NodeData.from] := True;
              If NodeDoneH1[NodeData.from] and NodeDoneH2[NodeData.from] then NextPlayer;
            end
            else
            GiveMSG (9, '', '^a^nYou can''t double down after hitting!', NodeData.from);
          end
        end
        else
          GiveMSG (9, '', '^A^nIt is not your turn!', NodeData.from);
      end
      else
        GiveMSG (9, '',  '^A^nYou aren''t playing in the ^A^pblackjack^A^n game.', NodeData.from);
    end;}
    If (s='TURN')or(s='TUR')or(s='TUNR') then
    begin
      If GamePlaying then
      begin
        If NodeTurn = NodeData.from then
          GiveMSG (9, '', '^A^nIt is ^A^pyour ^A^nturn in blackjack.',
                   NodeData.from)
        else
          GiveMSG (9, '', '^A^nIt is ^A^p'+NodeUser(NodeTurn)+'^A^n''s turn in blackjack.',
                   NodeData.from);
      end
      else
        GiveMSG (9, '', '^A^nThere is no blackjack game being played at the moment.', NodeData.from);
    end;
    If (s='SCAN')or(s='SCA')or(s='SCNA') then
    begin
      GiveMSG (9, '', #13#10'^A^oPlayer                          Playing    Cyberdollars', NodeData.from);
      GiveMSG (9, '', '^A^o-------------------------------------------------------', NodeData.from);
      For I := 1 to 255 do
      begin
        If NodeStat[I] = 1 then
          GiveMSG (9, '', '^A^o'+NodeUser(I)+Spaces(34-Length(KillTOPCodes(NodeUser(I)))) +' No         '+ToStr(NodeCD[I]),
                   NodeData.from);
        If NodeStat[I] = 2 then
          GiveMSG (9, '', '^A^o'+NodeUser(I)+Spaces(34-Length(KillTOPCodes(NodeUser(I)))) +'Yes         '+ToStr(NodeCD[I]),
                   NodeData.from);
      end;
      GiveMSG (9, '', #0, NodeData.from);
    end;
    If (s='BET')or(s='BTE') then
      GiveMSG (9, '', '^A^nYour current bet is ^A^p'+ToStr(NodeBet[NodeData.from])+' ^A^nCyberdollars.', NodeData.from)
    else
    If (s[1] = 'B') and (s[2] = 'E') and (s[3] = 'T') and (s[0] > #3) then
    begin
      If NodeStat[NodeData.from] < 2 then
      begin
        Delete (s, 1, 3);
        Val (s, L, Error);
        If (Error <> 0) or (L > ConfigData.MaxBet) or (L < ConfigData.MinBet) then
        begin
          GiveMSG (9, '', '^A^nBets must be between ^A^p'+ToStr(ConfigData.MinBet)+' ^A^nand ^A^p'+ToStr(ConfigData.MaxBet)+
                   ' ^A^nCyberdollars.', NodeData.from);
        end
        else
        begin
          NodeBet[NodeData.from] := L;
          GiveMSG (9, '', '^A^nOk, your bet will now be ^A^p'+ToStr(L)+' ^A^nCyberdollars.', NodeData.from);
        end;
      end
      else
        GiveMSG (9, '', '^A^nSorry, you cannot change your bet during a game.', NodeData.from);
    end;
  end;
  If (NodeData.Kind = 3) or (NodeData.Kind = 19) then
  begin
    If NodeStat[NodeData.from] = 2 then
    begin
      For J := 1 to NodeNumHands[Node] do
      If (HandValue (NodeHand[J, Node]) < 22) then
      Dec (NodeCD[NodeData.from], NodeBet[NodeData.from]*NodeDD[J, NodeData.from]);
      If NodeInsure[Node] then Dec (NodeCD[Node], Round(NodeBet[Node]/2));
      NodeStat[NodeData.from] := 0;
      BroadCastBut (9,'','^A^p'+CStr(NodeData.Alias)+' ^A^nis dealt out of the ^A^pblackjack^A^n game!',
                    NodeData.from);
      Writeln (CStr(NodeData.Alias), ' is dealt out of the blackjack game.');
      If (NodeData.from) = NodeTurn then NextPlayer;
    end;
    NodeCD[NodeData.from] := ConfigData.StartCD;
    NodeBet[NodeData.from] := ConfigData.BetCD;
  end;
  If (NodeData.Kind = 1) then
  begin
    s := UpStr(CStr(NodeData.Data));
    For J := 1 to 16 do
      If s = ComList[J] then GiveMSG (6, '^A^pThe blackjack dealer', 'You missed a "^A^k/^A^o" in your blackjack command.'
                                      , NodeData.from);
  end;
  If (NodeData.Kind = 10) and (NodeData.DoneTo = Node) then
  begin
    s := CStr(NodeData.Data);
    If Pos ('just barfed ALL OVER', s) <> 0 then Retal (NodeData.from);
    If Pos ('just bonked',s) <> 0 then Retal (NodeData.from);
    If Pos ('is booting',s) <> 0 then Retal (NodeData.from);
    If Pos ('with a disruptor!',s) <> 0 then Retal (NodeData.from);
    If Pos ('just tossed a grenade!',s) <> 0 then Retal (NodeData.from);
    If Pos ('is hitting',s) <> 0 then Retal (NodeData.from);
    If Pos ('is kicking the life out of',s) <> 0 then Retal (NodeData.from);
    If Pos ('with a mushroom cloud!',s) <> 0 then Retal (NodeData.from);
    If Pos ('a phaser and blasted',s) <> 0 then Retal (NodeData.from);
    If Pos ('is punching',s) <> 0 then Retal (NodeData.from);
    If Pos ('just shot',s) <> 0 then Retal (NodeData.from);
    If Pos ('just slapped',s) <> 0 then Retal (NodeData.from);
    If Pos ('across the back of the head!',s) <> 0 then Retal (NodeData.from);
    If Pos ('just spit on',s) <> 0 then Retal (NodeData.from);
    If Pos ('is stripping',s) <> 0 then Retal (NodeData.from);
    If Pos ('just zapped',s) <> 0 then Retal (NodeData.from);
    If Pos ('just bashed',s) <> 0 then Retal (NodeData.from);
    If Pos ('just stapled',s) <> 0 then Retal (NodeData.from);
    If Pos ('just killed',s) <> 0 then Retal (NodeData.from);
    If Pos ('just elbowed',s) <> 0 then Retal (NodeData.from);
    If Pos ('just poked',s) <> 0 then Retal (NodeData.from);
  end;
end;

Procedure GetMSGs;
Var
  b : byte;
  z : byte;
  f : file of byte;
  NodeFile : File of NodeRec;
begin
  z := 0;
  Assign (f, IPCDir + 'MIX'+NodeStr+'.TCH');
  Assign (NodeFile, IPCDir + 'MSG'+NodeStr+'.TCH');
  Reset (f);
  Reset (NodeFile);
  Repeat
    {$I-}
    Repeat
      Read (f,b);
    Until IOResult = 0;
    Seek (f, FilePos (f) - 1);
    Repeat
      Write (f,z);
    Until IOResult = 0;
    Repeat
      Read (NodeFile, NodeData);
    Until IOResult = 0;
    {$I+}
    If b = 1 then Process (NodeData);
  Until Eof (f);
  Close (f);
  Close (NodeFile);
  b := 0;
  Assign (f, IPCDir + 'CHGIDX.TCH');
  Reset (f);
  Seek (f, node);
  {$I-}
  Repeat
    Write (f,b);
  Until IOResult = 0;
  {$I+}
  Close (f);
end;

Procedure WaitSec;
begin
  GetTime (L, L, OldSec, L);
  Repeat
    GetTime (L, L, Sec, L);
  Until Sec <> OldSec;
  OldSec := Sec;
  Inc (SecPassed);
end;

Procedure StartGame;
Var
  SomeoneIn : Boolean;
  I : Word;
begin
  Writeln ('Starting a new blackjack game');
 { UsedCards := ''; }
  NodeData2.DoneTo := -1;
  NodeData2.From := node;
  NodeData2.Gender := 0;
  NodeData2.Alias := '';
  NodeData2.Kind := 9;
  NodeData2.Channel := 1;
  NodeData2.MinSec := 0;
  NodeData2.Data1 := 0;
  SomeoneIn := False;
  { Get dealer's hand }
  DealerHand := GetCard + GetCard;
  If (Random(5) = 0) {and (not registered)} then RegRemind;
  For I := 1 to 255 do
    If NodeStat[I] > 0 then
    begin
      SomeoneIn := True;
      GiveMSG (9, '', #0, I);
      GiveMSG (9, '', '^A^nA new ^A^pblackjack ^A^ngame has begun!', I);
      NodeStat[I] := 2;
    end;
  { Deal first cards }
  For I := 1 to 255 do
    If NodeStat[I] = 2 then
    begin
      NodeNumHands[I] := 1;
      NodeHand[1, I] := GetCard + GetCard;
      GiveMSG (9, '', '^A^nDealer''s cards: '+ColorHand(DealerHand[1]+DealerHand[2])+'^H^a  ^A^n  (^A^p'
               +ToStr(HandValue(DealerHand[1]+DealerHand[2]))+'^A^n)', I);
      GiveMSG (9, '', '^A^nYour cards:     '+ColorHand(NodeHand[1, I])+' ^A^n(^A^p'+ToStr(HandValue(NodeHand[1, I]))+'^A^n)',
               I);
      If DealerHand[1] = 'A' then GiveMSG (9, '',
        '^A^nDealer has possible blackjack, you may want to make an insurance bet.', I);
    end;
  If SomeoneIn then
  begin
    For I := 255 downto 1 do If NodeStat[I] = 2 then NodeTurn := I;
    GamePlaying := True;
    GiveMSG (9, '', '^A^nIt is your turn in ^A^pblackjack^A^n!', NodeTurn);
    Writeln ('It is ', NodeUser(NodeTurn), '''s turn in blackjack.');
  end
  else Writeln ('No players.');
  SecPassed := 0;
end;

Procedure WarnUser;
begin
  GiveMSG (9, '', #7'^I^nIt is still your turn in ^I^pblackjack^A^n!', NodeTurn);
  WaitSec;
end;

Procedure DealerPlay;
Var
  H : Word;
  I : Word;
  DealerValue : Word;
begin
  BroadCast (9, '', '');
  BroadCast (9, '', '^A^nThe ^A^pBlackjack dealer ^A^nnow plays.');
  WaitSec;
  BroadCast (9, '', '^A^nDealer''s cards: '+ColorHand(DealerHand)+' ^A^n(^A^p'+ToStr(HandValue(DealerHand))+'^A^n)');
  WaitSec;
  While HandValue (DealerHand) < 17 do
  begin
    Writeln ('Dealer hits.');
    BroadCast (9, '', '^A^pDealer ^A^nhits.');
    DealerHand := DealerHand + GetCard;
    WaitSec;
    BroadCast (9, '', '^A^nDealer''s cards: '+ColorHand(DealerHand)+' ^A^n(^A^p'+ToStr(HandValue(DealerHand))+'^A^n)');
    WaitSec;
  end;
  WaitSec;
  If HandValue (DealerHand) < 22 then BroadCast (9, '', '^A^pDealer ^A^nstays.'#13#10)
  else BroadCast (9, '', '^A^pDealer ^A^nbusts!'#13#10);
  BroadCast (9, '', '^A^nThe ^A^pblackjack ^A^ngame is over!');

  { Pay the winners }
  DealerValue := HandValue (DealerHand);
  If DealerValue > 21 then DealerValue := 0;
  If (DealerValue = 21) and (Length (DealerHand) = 4) then
  begin
    { Blackjack for dealer }
    BroadCast (9, '', '^A^nThe ^A^pdealer ^A^nhas a ^I^pblackjack^A^n!');
    For I := 1 to 255 do if NodeStat[I] = 2 then
    begin
      If NodeInsure[I] then
      begin
        Inc (NodeCD[I], Round (NodeBet[I]));
        GiveMSG (9, '', '^A^pYou^I^n win ^A^nyour insurance bet and get ^A^p'+ToStr(Round(NodeBet[I]))
                 +' ^A^nCyberdollars!', I);
        If NodeGender (I) then
          BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n wins ^A^nher insurance bet and gets ^A^p'
                        +ToStr(Round(NodeBet[I]))+' ^A^nCyberdollars!', I)
        else
          BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n wins ^A^nhis insurance bet and gets ^A^p'
                        +ToStr(Round(NodeBet[I]))+' ^A^nCyberdollars!', I)
      end;
      For H := 1 to NodeNumHands[I] do
      begin
      If (HandValue (NodeHand[H, I]) = 21) and (Length(NodeHand[H, I]) = 4) then
      begin
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^A^n''s hand #'+ToStr(H)+': '+ColorHand(NodeHand[H, I])+
                   ' ^A^n(^A^p'+ToStr(HandValue(NodeHand[H, I]))+'^A^n)', I);
        GiveMSG (9, '', '^A^pYour^A^n hand #'+ToStr(H)+':   '+ColorHand(NodeHand[H, I])+
                   ' ^A^n(^A^p'+ToStr(HandValue(NodeHand[H, I]))+'^A^n)', I);
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^A^n loses nothing.',I);
        GiveMSG (9, '', '^A^pYou^A^n lose nothing.',I);
      end
      else
      begin
        If HandValue (NodeHand[H, I]) < 22 then
        begin
          BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^A^n''s hand #'+ToStr(H)+': '+ColorHand(NodeHand[H, I])+
                        ' ^A^n(^A^p'+ToStr(HandValue(NodeHand[H, I]))+'^A^n)', I);
          GiveMSG (9, '', '^A^pYour^A^n hand #'+ToStr(H)+':   '+ColorHand(NodeHand[H, I])+
                  ' ^A^n(^A^p'+ToStr(HandValue(NodeHand[H, I]))+'^A^n)', I);
          BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n loses ^A^p'+ToStr(NodeBet[I]*NodeDD[H, I])+' ^A^nCyberdollars!',I);
          GiveMSG (9, '', '^A^pYou^I^n lose ^A^p'+ToStr(NodeBet[I]*NodeDD[H, I])+' ^A^nCyberdollars!',I);
          Dec (NodeCD[I], NodeBet[I]*NodeDD[H, I]);
        end;
      end;
{      If (HandValue (NodeHand2[I]) = 21) and (Length(NodeHand2[I]) = 4) and
         (NodeHand2[I] <> '') then
      begin
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^A^n''s second hand: '+ColorHand(NodeHand2[I])+
                   ' ^A^n(^A^p'+ToStr(HandValue(NodeHand2[I]))+'^A^n)', I);
        GiveMSG (9, '', '^A^pYour^A^n second hand: '+ColorHand(NodeHand2[I])+
                   ' ^A^n(^A^p'+ToStr(HandValue(NodeHand2[I]))+'^A^n)', I);
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^A^n loses nothing.',I);
        GiveMSG (9, '', '^A^pYou^A^n lose nothing.',I);
      end
      else
      begin
        If (NodeHand2[I] <> '') and (HandValue (NodeHand2[I]) < 22) then
        begin
          BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^A^n''s second hand: '+ColorHand(NodeHand2[I])+
                     ' ^A^n(^A^p'+ToStr(HandValue(NodeHand2[I]))+'^A^n)', I);
          GiveMSG (9, '', '^A^pYour^A^n second hand: '+ColorHand(NodeHand2[I])+
                     ' ^A^n(^A^p'+ToStr(HandValue(NodeHand2[I]))+'^A^n)', I);
          BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n loses ^A^p'+ToStr(NodeBet[I]*NodeDD2[I])+' ^A^nCyberdollars!',I);
          GiveMSG (9, '', '^A^pYou^I^n lose ^A^p'+ToStr(NodeBet[I]*NodeDD2[I])+' ^A^nCyberdollars!',I);
          Dec (NodeCD[I], NodeBet[I]*NodeDD2[I]);
        end;
      end;}
      end;
    end;
  end
  else
  For I := 1 to 255 do if NodeStat[I] = 2 then
  begin
    For H := 1 to NodeNumHands[I] do
    begin
    If HandValue (NodeHand[H, I]) < 22 then
    begin
      BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^A^n''s hand #'+ToStr(H)+': '+ColorHand(NodeHand[H, I])+
                 ' ^A^n(^A^p'+ToStr(HandValue(NodeHand[H, I]))+'^A^n)', I);
      GiveMSG (9, '', '^A^pYour^A^n hand #'+ToStr(H)+':   '+ColorHand(NodeHand[H, I])+
                 ' ^A^n(^A^p'+ToStr(HandValue(NodeHand[H, I]))+'^A^n)', I);
      If (HandValue (NodeHand[H, I]) = 21) and (Length (NodeHand[H, I]) = 4) then
      begin
        { Blackjack }
        GiveMSG (9, '', '^I^nYou got a ^I^pblackjack^I^n!', I);
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+' ^A^nhas a ^A^pblackjack^A^n!', I);
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n wins ^A^p'+ToStr(Round(NodeBet[I]*(3/2)))
                          +' ^A^nCyberdollars!', I);
        GiveMSG (9, '', '^A^pYou^I^n win ^A^p'+ToStr(Round(NodeBet[I]*(3/2)))+' ^A^nCyberdollars!', I);
        Inc (NodeCD[I], Round(NodeBet[I]*(3/2)));
        Writeln (NodeUser(I), ' gets a blackjack.');
      end
      else
      If HandValue (NodeHand[H, I]) > DealerValue then
      begin
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n wins ^A^p'+ToStr(NodeBet[I]*NodeDD[H, I])+' ^A^nCyberdollars!',I);
        GiveMSG (9, '', '^A^pYou^I^n win ^A^p'+ToStr(NodeBet[I]*NodeDD[H, I])+' ^A^nCyberdollars!',I);
        Inc (NodeCD[I], NodeBet[I]*NodeDD[H, I]);
      end
      else
      If HandValue (NodeHand[H, I]) < DealerValue then
      begin
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n loses ^A^p'+ToStr(NodeBet[I]*NodeDD[H, I])+' ^A^nCyberdollars!',I);
        GiveMSG (9, '', '^A^pYou^I^n lose ^A^p'+ToStr(NodeBet[I]*NodeDD[H, I])+' ^A^nCyberdollars!',I);
        Dec (NodeCD[I], NodeBet[I]*NodeDD[H, I]);
      end
      else
      If HandValue (NodeHand[H, I]) = DealerValue then
      begin
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^A^n loses nothing.',I);
        GiveMSG (9, '', '^A^pYou^A^n lose nothing.',I);
      end;
      If NodeInsure[I] then
      begin
        Dec (NodeCD[I], Round (NodeBet[I]/2));
        GiveMSG (9, '', '^A^pYou^I^n lose ^A^nyour insurance bet of ^A^p'+ToStr(Round(NodeBet[I]/2))
                 +' ^A^nCyberdollars!', I);
        If NodeGender (I) then
          BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n loses ^A^nher insurance bet of ^A^p'
                        +ToStr(Round(NodeBet[I]/2))+' ^A^nCyberdollars!', I)
        else
          BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n loses ^A^nhis insurance bet of ^A^p'
                        +ToStr(Round(NodeBet[I]/2))+' ^A^nCyberdollars!', I)
      end;
    end;
    end;
{    If (NodeHand2[I] <> '') and (HandValue (NodeHand2[I]) < 22) then
    begin
      BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^A^n''s second hand: '+ColorHand(NodeHand2[I])+
                 ' ^A^n(^A^p'+ToStr(HandValue(NodeHand2[I]))+'^A^n)', I);
      GiveMSG (9, '', '^A^pYour^A^n second hand: '+ColorHand(NodeHand2[I])+
                 ' ^A^n(^A^p'+ToStr(HandValue(NodeHand2[I]))+'^A^n)', I);
      If (HandValue (NodeHand2[I]) = 21) and (Length (NodeHand2[I]) = 4) then
      begin}
        { Blackjack }{
        GiveMSG (9, '', '^I^nYou got a ^I^pblackjack^I^n!', I);
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+' ^A^nhas a ^A^pblackjack^A^n!', I);
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n wins ^A^p'+ToStr(Round(NodeBet[I]*(3/2)))
                          +' ^A^nCyberdollars!', I);
        GiveMSG (9, '', '^A^pYou^I^n win ^A^p'+ToStr(Round(NodeBet[I]*(3/2)))+' ^A^nCyberdollars!', I);
        Inc (NodeCD[I], Round(NodeBet[I]*(3/2)));
        Writeln (NodeUser(I), ' gets a blackjack.');
      end
      else
      If HandValue (NodeHand2[I]) > DealerValue then
      begin
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n wins ^A^p'+ToStr(NodeBet[I]*NodeDD2[I])+' ^A^nCyberdollars!',I);
        GiveMSG (9, '', '^A^pYou^I^n win ^A^p'+ToStr(NodeBet[I]*NodeDD2[I])+' ^A^nCyberdollars!',I);
        Inc (NodeCD[I], NodeBet[I]*NodeDD2[I]);
      end
      else
      If HandValue (NodeHand2[I]) < DealerValue then
      begin
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n loses ^A^p'+ToStr(NodeBet[I]*NodeDD2[I])+' ^A^nCyberdollars!',I);
        GiveMSG (9, '', '^A^pYou^I^n lose ^A^p'+ToStr(NodeBet[I]*NodeDD2[I])+' ^A^nCyberdollars!',I);
        Dec (NodeCD[I], NodeBet[I]*NodeDD2[I]);
      end
      else
      If HandValue (NodeHand2[I]) = DealerValue then
      begin
        BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^A^n loses nothing.',I);
        GiveMSG (9, '', '^A^pYou^A^n lose nothing.',I);
      end;
      If NodeInsure[I] then
      begin
        Dec (NodeCD[I], Round (NodeBet[I]/2));
        GiveMSG (9, '', '^A^pYou^I^n lose ^A^nyour insurance bet of ^A^p'+ToStr(Round(NodeBet[I]/2))
                 +' ^A^nCyberdollars!', I);
        If NodeGender (I) then
          BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n loses ^A^nher insurance bet of ^A^p'
                        +ToStr(Round(NodeBet[I]/2))+' ^A^nCyberdollars!', I)
        else
          BroadCastBut (9, '', '^A^p'+NodeUser(I)+'^I^n loses ^A^nhis insurance bet of ^A^p'
                        +ToStr(Round(NodeBet[I]/2))+' ^A^nCyberdollars!', I)
      end;
    end;}
  end;
  GamePlaying := False;
  SecPassed := 0;
  For I := 1 to 255 do
  begin
    If NodeStat[I] = 2 then NodeStat[I] := 1;
    For H := 1 to 6 do
    begin
      NodeHand[H, I] := '';
      NodeDD[H, I] := 1;
      NodeDoneH[H, I] := False;
    end;
    NodeNumHands[I] := 0;
    NodeInsure[I] := False;
  end;
end;

Procedure BootUser;
begin
  GiveMSG (9, '', #7'^I^nYou have been thrown out of the ^I^pblackjack^I^n game for taking too long!', NodeTurn);
  BroadCastBut (9, '', '^A^nThe ^A^pBlackjack dealer^A^n has thrown ^A^p'+NodeUser(NodeTurn)
                +'^A^n out of the game for taking too long!', NodeTurn);
  NodeStat[NodeTurn] := 0;
  Dec (NodeCD[NodeTurn], NodeBet[NodeTurn]);
  WaitSec;
  NextPlayer;
end;

Procedure RunBJ;
Var
  ch : Char;
begin
  ch := #0;
  OldSec := 0;
  SecPassed := 0;
  GamePlaying := False;
  Repeat
    GetTime (L, L, Sec, L);
    If Sec <> OldSec then
    begin
      OldSec := Sec;
      Inc (SecPassed);
    end;
    If not GamePlaying and (SecPassed > ConfigData.TimeBetweenGames) then StartGame;
    If GamePlaying and (SecPassed = ConfigData.FirstWarn) then WarnUser;
    If GamePlaying and (SecPassed = ConfigData.SecondWarn) then WarnUser;
    If GamePlaying and (SecPassed = ConfigData.BootFromBJ) then BootUser;
    If MSGWait then GetMSGs;
    TimeSlice (2);
    If (NodeTurn = -1) and (GamePlaying) then DealerPlay;
    If Keypressed then ch := Readkey;
  Until ch = #27;
end;

begin
  InitProgram;
  InitTOP;
  InitScreen;
  InitVars;
  RunBJ;
  DeInit;
end.
