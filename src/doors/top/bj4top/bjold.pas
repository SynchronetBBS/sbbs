{$A+,B-,D-,E+,F-,G-,I+,L-,N-,O-,R-,S+,V+,X-}
{$M 16384,0,0}
{$V-}
Program Blackjack4RAP;
Uses DOS, Crt, BJSupp, NewReg;

Type
  IDXRec = Record
             Alias : String[30];
             RealName : String[40];
             Baud : Word;
             Location : String[25];
             Gender : Word;
             Description : String[60];
             Age : Byte;
             Quiet : ShortInt;
             Task : ShortInt;
             Buf : Array [1..346] of byte;
           end;

  Str30 = String[30];
  Str255 = String[255];

  NodeRec = Record
             Kind : Word;
             From : Word;
             DoneTo : Integer;
             Gender : Word;
             Alias : Str30;
             Data : Str255;
             {$IFDEF OLDRAP}
             {$ELSE}
             TTTStart : Integer;
             {$ENDIF}
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

  ProgVerStr = ' v0.90wb';

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

  HelpData : Array[1..15] of String = (
  #13#10'^^0EEnter ^^0D/BJ BET ^^0C<amount> ^^0Eto change your bet (cannot be done during a game).',
  '^^0EEnter ^^0D/BJ CD ^^0Eor ^^0D/BJ $ ^^0Efor your current number of Cyberdollars.',
  '^^0EEnter ^^0D/BJ DOUBLE ^^0Eto double down.',
  '^^0EEnter ^^0D/BJ DOUBLE 2 ^^0Eto double down on your second hand.',
  '^^0EEnter ^^0D/BJ HAND ^^0Eto view the dealer''s and your own cards.',
  '^^0EEnter ^^0D/BJ HELP ^^0Efor this commands list.',
  '^^0EEnter ^^0D/BJ HIT ^^0Eto be dealt a card.',
  '^^0EEnter ^^0D/BJ HIT 2 ^^0Eto be dealt a card to your second hand.',
  '^^0EEnter ^^0D/BJ INSURE ^^0Eto make an insurance bet against a blackjack.',
  '^^0EEnter ^^0D/BJ OFF ^^0Eto be dealt out of blackjack.',
  '^^0EEnter ^^0D/BJ ON ^^0Eto join in the next game of blackjack.',
  '^^0EEnter ^^0D/BJ SCAN ^^0Eto see who''s playing blackjack.',
  '^^0EEnter ^^0D/BJ SPLIT ^^0Eto split your hand.',
  '^^0EEnter ^^0D/BJ STAY ^^0Eto end your turn.',
  '^^0EEnter ^^0D/BJ TURN ^^0Eto see who''s turn it is.'#13#10);

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
  NodeHand : Array [1..255] of String[24];
  NodeHand2 : Array [1..255] of String[24];
  NodeDoneH1  : Array [1..255] of Boolean;
  NodeDoneH2 : Array [1..255] of Boolean;
  NodeBet : Array [1..255] of byte;
  NodeDD : Array [1..255] of byte;
  NodeDD2 : Array [1..255] of byte;
  UsedCards : String;
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
  Write (' Blackjack for RAP'+ProgVerStr);
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
  Writeln ('BJ4RAP Registration--Your registration code and named will be saved to');
  Writeln ('  the ', ParamStr(0), ' file.');
  Writeln;
  Write ('Your Name (Please note it is case sensitive): ');
  Readln (RegisterData.TheName);
  Write ('Registration Number:');
  Readln (RegisterData.TheReg);
  Writeln;
  UserName := RegisterData.TheName;
  RegisterNum := RegisterData.TheReg;
  Check_Register;
  If Registered then Writeln ('The registration number checks out, thank you for registering')
  else
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
  Writeln ('Blackjack for RAP'+ProgVerStr);
  Writeln ('Copyright (c) 1995 by David Ong, OmniWare(tm), All Rights Reserved.');
  Writeln;
  If ParamCount < 1 then
  begin
    Writeln ('Command line parameter(s) missing!');
    Writeln;
    Writeln ('Usage: BJ4RAP <work path> [<node>]');
    Writeln;
    Writeln ('<work path>  Is the path to RAP''s work directory.');
    Writeln ('<node>       Is optional and if given will force BJ4RAP to use the node even');
    Writeln ('             if it is already taken.');
    Writeln;
    Writeln ('Eg: BJ4RAP d:\rap\workdir');
    Writeln ('For further help refer to BJ4RAP.DOC');
    Halt;
  end;
  RegisterKey := '#####';
  UserName := RegisterData.TheName;
  RegisterNum := RegisterData.TheReg;
  Check_Register;
  If UpStr(ParamStr (1)) = '/REGISTER' then
  begin
    Enter_Reg;
    Halt;
  end;
{ If UpStr(ParamStr(1)) = '/CONFIG' then ConfigureBJ;}
  IPCDir := ParamStr(1);
  If IPCDir[Length(IPCDir)] <> '\' then IPCDir := IPCDir + '\';
  FileMode:=fmReadWrite+FmDenyNone;
  If not FExists (IPCDir + 'NODEIDX.RAP') then
  begin
    Writeln ('Error: The NODEIDX.RAP file was not found in the ', UpStr(IPCDir), ' directory!');
    Writeln ('Make sure RAP uses the same work path and has been previously run or is'#13#10'currently running.');
    Halt;
  end;
  If not FExists (IPCDir + 'NODEIDX2.RAP') then
  begin
    Writeln ('Error: The NODEIDX2.RAP file was not found in the ', UpStr(IPCDir), ' directory!');
    Writeln ('Make sure RAP uses the same work path and has been previously run or is'#13#10'currently running.');
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
    Assign (F, IPCDir + 'NODEIDX2.RAP');
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
  Assign (f, IPCDir + 'NODE'+NodeStr+'.RAP');
  Rewrite (f);
  Close (f);
  Assign (f, IPCDir + 'MIDX'+NodeStr+'.RAP');
  Rewrite (f);
  Close (f);
  Assign (F, IPCDir + 'NODEIDX2.RAP');
  Reset (f);
  Seek (f, Node - 1);
  b := 1;
  {$I-}
  Repeat
    Write (f,b);
  Until IOResult = 0;
  {$I+}
  Close (f);
  Assign (IDXFile, IPCDir + 'NODEIDX.RAP');
  Reset (IDXFile);
  Seek (IDXFile, Node - 1);
  FillChar (IDXData, SizeOf(IDXData), #0);
  IDXData.Alias := '^0FBJ'#8'lackjack Dealer'#0;
  IDXData.Alias[0] := '^';
  IDXData.RealName := 'J'#0;
  IDXData.RealName[0] := 'B';
  IDXData.Baud := 0;
  If registered then
  begin
    IDXData.Location := 'egistered'+#0;
    IDXData.Location[0] := 'R';
  end
  else
  begin
    IDXData.Location := 'NREGISTERED'+#0;
    IDXData.Location[0] := 'U';
  end;
  IDXData.Gender := 0;
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
  Assign (f, IPCDir + 'MIDX' + PaddedNum(N)+'.RAP');
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
  Assign (NodeFile, IPCDir + 'NODE' + PaddedNum(N)+'.RAP');
  Reset (NodeFile);
  Seek (NodeFile, FilePos(f) - 1);
  {$I-}
  Repeat
    Write (NodeFile, NodeData);
  Until IOResult = 0;
  {$I+}
  Close (NodeFile);
  Close (f);
  Assign (F, IPCDir + 'CHGIDX.RAP');
  Reset (f);
  Seek (f, N - 1);
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
  {$IFDEF OLDRAP}
  {$ELSE}
  NodeData.TTTStart := -1;
  {$ENDIF}
  Alias := Alias + #0;
  Data := Data + #0;
  NodeData.Kind := T;
  NodeData.From := Node - 1;
  NodeData.Alias := Minus1 (Alias);
  NodeData.Alias[0] := Alias[1];
  NodeData.Data := Minus1 (Data);
  NodeData.Data[0] := Data[1];
  Assign (IDX2, IPCDir + 'NODEIDX2.RAP');
  {$I-}
  Repeat
    Reset (IDX2);
  Until IOResult = 0;
  {$I+}
  For L := 1 to 255 do
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
  {$IFDEF OLDRAP}
  {$ELSE}
  NodeData.TTTStart := -1;
  {$ENDIF}
  Alias := Alias + #0;
  Data := Data + #0;
  NodeData.Kind := T;
  NodeData.From := Node - 1;
  NodeData.Alias := Minus1 (Alias);
  NodeData.Alias[0] := Alias[1];
  NodeData.Data := Minus1 (Data);
  NodeData.Data[0] := Data[1];
  Assign (IDX2, IPCDir + 'NODEIDX2.RAP');
  {$I-}
  Repeat
    Reset (IDX2);
  Until IOResult = 0;
  {$I+}
  For L := 1 to 255 do
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
  {$IFDEF OLDRAP}
  {$ELSE}
  NodeData.TTTStart := -1;
  {$ENDIF}
  Alias := Alias + #0;
  Data := Data + #0;
  NodeData.Kind := T;
  NodeData.From := Node - 1;
  NodeData.Alias := Minus1 (Alias);
  NodeData.Alias[0] := Alias[1];
  NodeData.Data := Minus1 (Data);
  NodeData.Data[0] := Data[1];
  Assign (IDX2, IPCDir + 'NODEIDX2.RAP');
  {$I-}
  Repeat
    Reset (IDX2);
  Until IOResult = 0;
  {$I+}
  For L := 1 to 255 do
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
  {$IFDEF OLDRAP}
  {$ELSE}
  NodeData.TTTStart := -1;
  {$ENDIF}
  Alias := Alias + #0;
  Data := Data + #0;
  NodeData.Kind := T;
  NodeData.From := Node - 1;
  NodeData.Alias := Minus1 (Alias);
  NodeData.Alias[0] := Alias[1];
  NodeData.Data := Minus1 (Data);
  NodeData.Data[0] := Data[1];
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
  {$IFDEF OLDRAP}
  {$ELSE}
  NodeData.TTTStart := -1;
  {$ENDIF}
  Alias := Alias + #0;
  Data := Data + #0;
  NodeData.Kind := T;
  NodeData.From := Node - 1;
  NodeData.Alias := Minus1 (Alias);
  NodeData.Alias[0] := Alias[1];
  NodeData.Data := Minus1 (Data);
  NodeData.Data[0] := Data[1];
  Assign (IDX2, IPCDir + 'NODEIDX2.RAP');
  {$I-}
  Repeat
    Reset (IDX2);
  Until IOResult = 0;
  {$I+}
  For L := 1 to 255 do
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
    Assign (IDXFile, IPCDir + 'NODEIDX.RAP');
    Reset (IDXFile);
    Seek (IDXFile, I - 1);
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
    Assign (IDXFile, IPCDir + 'NODEIDX.RAP');
    Reset (IDXFile);
    Seek (IDXFile, I - 1);
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

Procedure InitRAP;
begin
  Node := UnusedNode;
  Writeln ('Logging onto RAP as node ', Node);
  Str (Node, NodeStr);
  NodeStr := PaddedNum (Node);
  TakeNode;
  BroadCastAll (2, '^^0FBJ'#8'lackjack Dealer', '');
  BroadCastAll (9, '', '^^89*** Blackjack'+ProgVerStr+' Game Module Active ***');
  BroadCastAll (9, '', '^^0DThis program is registered to: ^^0F'+Registerdata.TheName);
  If not Registered then
    BroadCastAll (9, '', '^^8EBe sure to register soon!');
  BroadCastAll (9, '', '^^0DType ^^0F/BJ HELP ^^0Dfor the commands.');
end;

Procedure DeInit;
Var
  B : byte;
  f : File of byte;
begin
  BroadCastAll (3, '^^0CThe ^^0FBlackjack Dealer','');
  BroadCastAll (9, '', '^^89*** Blackjack'+ProgVerStr+' Game Module Deactivated ***');
  Assign (F, IPCDir + 'NODEIDX2.RAP');
  Reset (f);
  Seek (f, Node - 1);
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
  GotoXY (8,1);  Writeln ('Blackjack for RAP'+ProgVerStr+' (c) 1995 by David Ong, All Rights Reserved.');
  WindowBorder (5,3,74,21, White, Blue);
  GotoXY (1,25);
  TextAttr := $70;
  Write (' Press <ESC> to deactivate blackjack');
  ClrEol;
  If not registered then
  begin
    GotoXY (67,25);
    Textcolor (Red+Blink);
    Textbackground (LightGray);
    Write ('Unregistered');
  end
  else
  begin
    GotoXY (67,25);
    Textcolor (Red);
    Textbackground (LightGray);
    Write ('Registered');
  end;
  Window (6,4,73,20);
  Textbackground (Blue);
  Textcolor (Yellow);
  Writeln ('Waiting for people to join blackjack.');
end;

Procedure InitVars;
begin
  For L := 1 to 255 do
  begin
    NodeStat[L] := 0;
    NodeCD[L] := ConfigData.StartCD;
    NodeHand[L] := '';
    NodeHand2[L] := '';
    NodeBet[L] := ConfigData.BetCD;
    NodeDD[L] := 1;
    NodeDD2[L] := 1;
    NodeDoneH1[L] := False;
    NodeDoneH2[L] := False;
    NodeInsure[L] := False;
  end;
  UsedCards := '';
end;

Function MSGWait : boolean;
Var
  b : byte;
  f : File of byte;
begin
  Assign (f, IPCDir + 'CHGIDX.RAP');
  Reset (f);
  Seek (f, Node - 1);
  {$I-}
  Repeat
    Read (f,b);
  Until IOResult = 0;
  {$I+}
  MSGWait := b = 1;
  Close (f);
end;

Function GetCard : String;
Const
  Cards : Array [1..13] of Char = 'A23456789TJQK';
  Kinds : Array [1..4] of Char = 'HDSC';
Var
  Value, Kind : Byte;
  Card : String[6];
begin
  Repeat
    Value := Random (13)+1;
    Kind := Random (4)+1;
    Card := Cards[Value] + Kinds [Kind];
  Until Pos (Card, UsedCards) = 0;
  UsedCards := UsedCards + Card;
  If Length (UsedCards) > 92 then UsedCards := '';
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
    If Hand[2] in ['D','H'] then New := New + '^^74'+Hand[1]+Hand[2]+'^^0A '
    else
    If Hand[2] in ['C','S'] then New := New + '^^70'+Hand[1]+Hand[2]+'^^0A '
    else New := New + '^^70  ^^0A';
    Delete (Hand, 1, 2);
  end;
  ColorHand := New;
end;

Procedure NextPlayer;
Var
  I : Word;
begin
  If NodeStat[NodeTurn] <> 0 then
    If NodeHand2[NodeTurn] <> '' then
    begin
      If (HandValue (NodeHand[NodeTurn]) > 21) and
      (HandValue (NodeHand2[NodeTurn]) > 21) then NodeStat[NodeTurn] := 1;
    end
    else If HandValue (NodeHand[NodeTurn]) > 21 then NodeStat[NodeTurn] := 1;
  If NodeStat[NodeTurn] = 1 then
  begin
    If NodeInsure[NodeTurn] then
    begin
      Dec (NodeCD[NodeTurn], Round (NodeBet[NodeTurn]/2));
      GiveMSG (9, '', '^^0FYou^^8D lose ^^0Dyour insurance bet of ^^0F'+ToStr(Round(NodeBet[NodeTurn]/2))
               +' ^^0DCyberdollars!', NodeTurn);
      If NodeGender (NodeTurn) then
        BroadCastBut (9, '', '^^0F'+NodeUser(NodeTurn)+'^^8D loses ^^0Dher insurance bet of ^^0F'
                      +ToStr(Round(NodeBet[NodeTurn]/2))+' ^^0DCyberdollars!', NodeTurn)
      else
        BroadCastBut (9, '', '^^0F'+NodeUser(NodeTurn)+'^^8D loses ^^0Dhis insurance bet of ^^0F'
                      +ToStr(Round(NodeBet[NodeTurn]/2))+' ^^0DCyberdollars!', NodeTurn)
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
    {$IFDEF OLDRAP}
    {$ELSE}
    NodeData2.TTTStart := -1;
    {$ENDIF}
    NodeData2.DoneTo := -1;
    NodeData2.From := Node - 1;
    NodeData2.Gender := 0;
    NodeData2.Alias := '';
    NodeData2.Kind := 9;
    NodeData2.Data := '^0DIt is your turn in ^^0Fblackjack^^0D!'#0;
    NodeData2.Data[0] := '^';
    SendMSG (NodeTurn, NodeData2);
  end;
  SecPassed := 0;
  If NodeTurn > 1 then Writeln ('It is ', NodeUser(NodeTurn), '''s turn in blackjack.')
  else Writeln ('It is the Dealer''s turn in blackjack.');
end;

Procedure DisplayHand (Node : Word);
begin
  GiveMSG (9, '', '^^0DDealer''s cards: '+ColorHand(DealerHand[1]+DealerHand[2])+'^^70  ^^0D  (^^0F'
           +ToStr(HandValue(DealerHand[1]+DealerHand[2]))+'^^0D)', Node);
  If NodeHand2[Node] <> '' then
  begin
    If not NodeDoneH1[Node] then
    GiveMSG (9, '', '^^0DFirst hand :    '+ColorHand(NodeHand[Node])+' ^^0D(^^0F'
             +ToStr(HandValue(NodeHand[Node]))+'^^0D)', Node);
    If not NodeDoneH2[Node] then
    GiveMSG (9, '', '^^0DSecond hand:    '+ColorHand(NodeHand2[Node])+' ^^0D(^^0F'
             +ToStr(HandValue(NodeHand2[Node]))+'^^0D)', Node);
  end
  else
    GiveMSG (9, '', '^^0DYour cards:     '+ColorHand(NodeHand[Node])+' ^^0D(^^0F'
             +ToStr(HandValue(NodeHand[Node]))+'^^0D)', Node);
end;

Procedure RegRemind;
Var
  N : Byte;
begin
  N := Random (4);
  Case N of
    0 : Broadcast (9,'','From ^^0BBlackjack Dealer^^0E, holding up a big placard: ^^0ARemember to register Blackjack!');
    1 : BroadCast (9,'','^^8FThis version of Blackjack for RAP is UNREGISTERED!^^0E');
    2 : BroadCast (9,'','From ^^0BBlackjack Dealer^^0E, in a hypnotic tone: ^^0ARegister blackjack, Register blackjack...');
    3 : BroadCast (9,'','From ^^0BBlackjack Dealer^^0E, over the bar''s PA system: ^^0ABlackjack is UNREGISTERED!');
  end;
end;

Procedure Retal (Node : Word);
Var
  N : Byte;
  J : Byte;
begin
  N := Random (20);
  If N > 9 then
  begin
    J := Random (9);
    Case J of
    0 : begin
          GiveMSG (9, '', 'The ^^0BBlackjack Dealer ^^0Eis booting ^^0Byou ^^0Eto the head!',
                      Node);
          BroadCastAllBut (9, '', 'The ^^0BBlackjack Dealer ^^0Eis booting ^^0B'+NodeUser(Node)+' ^^0Eto the head!', Node);
        end;
    1 : begin
          GiveMSG (9, '', 'The ^^0BBlackjack Dealer ^^0Eis frowning darkly at ^^0Byou^^0E!',
                      Node);
          BroadCastAllBut (9, '', 'The ^^0BBlackjack Dealer ^^0Eis frowning darkly at ^^0B'+NodeUser(Node)+'^^0E!', Node);
        end;
    2 : begin
          GiveMSG (9, '', 'The ^^0BBlackjack Dealer ^^0Ejust smacked ^^0Byou ^^0Eacross the back of the head!',
                      Node);
          BroadCastAllBut (9, '', 'The ^^0BBlackjack Dealer ^^0Ejust smacked ^^0B'+NodeUser(Node)
                        +' ^^0Eacross the back of the head!', Node);
        end;
    3,4 : If NodeStat[Node] > 0 then
          BroadCastAll (9, 'The Blackjack Dealer', 'From ^^0BThe Blackjack Dealer^^0E, grumbling unhappily: ^^0AStop that, '
                        +NodeUser(Node)+', or I''ll kick you out of the game!');
    5 : begin
          GiveMSG (9, '', 'The ^^0BBlackjack Dealer ^^0Ejust elbowed ^^0Byou ^^0Ein the face!',
                      Node);
          BroadCastAllBut (9, '', 'The ^^0BBlackjack Dealer ^^0Ejust elbowed ^^0B'+NodeUser(Node)+' ^^0Ein the face!', Node);
        end;
    6 : BroadCastAll (9, '', 'The ^^0BBlackjack Dealer ^^0Ejust put up ^^0Bhis^^0E deflector shields!');
    7 : begin
          GiveMSG (9, '', 'The ^^0BBlackjack Dealer ^^0Ejust poked ^^0Byou ^^0Ein the ribs!',
                      Node);
          BroadCastAllBut (9, '', 'The ^^0BBlackjack Dealer ^^0Ejust poked ^^0B'+NodeUser(Node)+' ^^0Ein the ribs!', Node);
        end;
    8 : begin
          GiveMSG (9, '', 'The ^^0BBlackjack Dealer ^^0Ejust gave ^^0Byou ^^0Ea good, swift, kick in the butt!',
                      Node);
          BroadCastAllBut (9, '', 'The ^^0BBlackjack Dealer ^^0Ejust gave ^^0B'+NodeUser(Node)
                           +' ^^0Ea good, swift, kick in the butt!', Node);
        end;
    end;
  end
  else
  If ((N = 9) or (N = 8)) and (NodeStat[NodeData.From+1] > 0) then
  begin
    If NodeStat[NodeData.From+1] = 2 then
    begin
      Dec (NodeCD[Node], NodeBet[Node]*NodeDD[Node]);
      If (NodeHand2[Node] <> '') and (HandValue (NodeHand2[Node]) < 22) then
        Dec (NodeCD[Node], NodeBet[Node]*NodeDD2[Node]);
      If NodeInsure[Node] then Dec (NodeCD[Node], Round(NodeBet[Node]/2));
    end;
    NodeStat[Node] := 0;
    BroadCastBut (9,'','^^0DThe ^^0Fblackjack dealer ^^0Djust tossed ^^0F'+(NodeUser(Node))
                  +' ^^0Dout of the ^^0Fblackjack^^0D game!', Node);
    GiveMSG (9,'','^^0DThe angry ^^0Fblackjack dealer ^^0Djust tossed ^^0Fyou ^^0Dout of the ^^0Fblackjack^^0D game!',
                  Node);
    Writeln (NodeUser(Node), ' is dealt out of the blackjack game.');
    If (NodeData.From+1) = NodeTurn then NextPlayer;
  end;
end;

Procedure Process (NodeData : NodeRec);
Var
  s : String;
  I : Word;
  L : LongInt;
  Error : Integer;
  J : Word;
begin
  {$IFDEF OLDRAP}
  {$ELSE}
  NodeData2.TTTStart := -1;
  {$ENDIF}
  NodeData2.DoneTo := -1;
  NodeData2.From := Node - 1;
  NodeData2.Gender := 0;
  NodeData2.Alias := '';
  NodeData2.Kind := 9;
  If NodeData.Kind = 6 then
  begin
    s := KillSpaces(UpStr(CStr(NodeData.Data)));
    If s = 'ON' then
    begin
      If NodeStat[NodeData.From+1] = 0 then
      begin
        GiveMSG (9, '', '^^0DOk, you will now be playing in the next ^^0Fblackjack ^^0Dgame!',
                 NodeData.From+1);
        NodeStat[NodeData.From+1] := 1;
        BroadCastBut (9,'','^^0F'+CStr(NodeData.Alias)+' ^^0Dwill be playing in the next ^^0Fblackjack^^0D game!',
                      NodeData.From+1);
        Writeln (CStr(NodeData.Alias), ' joins the blackjack game.');
      end
      else
        GiveMSG (9, '', '^^0DYou are already playing ^^0Fblackjack^^0D!', NodeData.From+1);
    end;
    If s = 'OFF' then
    begin
      If NodeStat[NodeData.From+1] = 0 then
        GiveMSG (9, '', '^^0DYou aren''t playing in ^^0Fblackjack^^0D!', NodeData.From+1)
      else
      begin
        GiveMSG (9, '', '^^0DOk, you are no longer playing ^^0Fblackjack^^0D.', NodeData.From+1);
        If NodeStat[NodeData.From+1] = 2 then
        begin
          Dec (NodeCD[NodeData.From+1], NodeBet[NodeData.From+1]*NodeDD[NodeData.From+1]);
          NextPlayer;
        end;
        BroadCastBut (9,'','^^0F'+CStr(NodeData.Alias)+' ^^0Dask to be dealt out of the ^^0Fblackjack^^0D game!',
                      NodeData.From+1);
        Writeln (CStr(NodeData.Alias), ' is dealt out of the blackjack game.');
        NodeStat[NodeData.From+1] := 0;
      end;
    end;
    If (s='INSURE')or(s='INSURANCE')or(s='INSUR')or(s='INS')or(s='INSUER') then
    begin
      If (NodeStat[NodeData.From+1] = 2) then
      begin
        If NodeData.From+1 = NodeTurn then
        begin
          If NodeInsure[NodeData.From+1] then
            GiveMSG (9, '', '^^0DYou''ve already made an insurance bet!', NodeData.From+1)
          else
          begin
            If (Length (NodeHand[NodeData.From+1]) = 4) and (NodeHand2[NodeData.From+1]='') then
            begin
              GiveMSG (9, '', '^^0DOk, you make an insurance bet.', NodeData.From+1);
              BroadcastBut (9, '', '^^0F'+NodeUser(NodeTurn)+' ^^0Dmakes an insurance bet.', NodeData.From+1);
              NodeInsure[NodeData.From+1] := True;
            end
            else GiveMSG (9, '', '^^0DYou can''t buy insurance now!', NodeData.From+1);
          end;
        end
        else
          GiveMSG (9, '', '^^0DIt isn''t your turn!', NodeData.From+1);
      end
      else
        GiveMSG (9, '', '^^0DYou aren''t playing in the ^^0Fblackjack^^0D game.', NodeData.From+1);
    end;
    If (s='STAY')or(s='STA')or(s='STY')or(s='SAY')or(s='TAY')or(s='STYA')or(s='STAYU')or
       (s='STAND') then
    begin
      If (NodeStat[NodeData.From+1] = 2) then
      begin
        If NodeData.From+1 = NodeTurn then
        begin
          GiveMSG (9, '', '^^0DOk, you stay.', NodeData.From+1);
          BroadcastBut (9, '', '^^0F'+NodeUser(NodeTurn)+' ^^0Dstays.', NodeData.From+1);
          NextPlayer;
        end
        else
          GiveMSG (9, '', '^^0DIt isn''t your turn!', NodeData.From+1);
      end
      else
        GiveMSG (9, '', '^^0DYou aren''t playing in the ^^0Fblackjack^^0D game.', NodeData.From+1);
    end;
    If (s='HAND')or(s='HAN')or(s='HADN')or(s='AHND') then
    begin
      If NodeStat[NodeData.From+1] = 2 then DisplayHand (NodeData.From+1)
      else
        GiveMSG (9, '', '^^0DYou aren''t playing in the ^^0Fblackjack^^0D game.', NodeData.From+1);
    end;
    If (s = 'CD') or (s = '$') then
      GiveMSG (9, '', '^^0DYou have ^^0F'+ ToStr(NodeCD[NodeData.From+1])+'^^0D Cyberdollars.', NodeData.From+1);
    If (s='HIT')or(s='HIT1')or(s='HTI')or(s='IHT') then
    begin
      If NodeStat[NodeData.From+1] = 2 then
      begin
        If NodeData.From+1 = NodeTurn then
        begin
          If NodeDoneH1[NodeData.From+1] then GiveMSG (9, '', '^^0DYou can''t play that hand!', NodeData.From+1)
          else
          begin
            BroadcastBut (9, '', '^^0F'+NodeUser(NodeTurn)+' ^^0Dhits.', NodeData.From+1);
            NodeHand[NodeData.From+1] := NodeHand[NodeData.From+1] + GetCard;
            DisplayHand (NodeData.From+1);
            SecPassed := 0;
            Writeln (NodeUser(NodeTurn), ' hits.');
            If HandValue(NodeHand[NodeData.From+1]) > 21 then
            begin
              GiveMSG (9, '', '^^8DYou busted!', NodeData.From+1);
              BroadCastBut (9, '', '^^0F'+NodeUser(NodeTurn)+' ^^0Dbusts!', NodeData.From+1);
              BroadCastBut (9, '', '^^0F'+NodeUser(NodeTurn)+'^^8D loses ^^0F'+ToStr(NodeBet[NodeTurn])+' ^^0DCyberdollars!'
                            , NodeData.From+1);
              GiveMSG (9, '', '^^0FYou^^8D lose ^^0F'+ToStr(NodeBet[NodeTurn])+' ^^0DCyberdollars!', NodeData.From+1);
              Dec (NodeCD[NodeData.From+1], NodeBet[NodeData.From+1]);
              Writeln (NodeUser(NodeTurn), ' busts.');
              NodeDoneH1[NodeData.From+1] := True;
              If NodeHand2[NodeData.From+1] = '' then NextPlayer
              else If NodeDoneH1[NodeData.From+1] and NodeDoneH2[NodeData.From+1] then NextPlayer;
            end;
          end;
        end
        else
          GiveMSG (9, '', '^^0DIt is not your turn!', NodeData.From+1);
      end
      else
        GiveMSG (9, '',  '^^0DYou aren''t playing in the ^^0Fblackjack^^0D game.', NodeData.From+1);
    end;
    If (s='HIT2')or(s='HTI2')or(s='HI2T')or(s='IHT2') then
    begin
      If NodeStat[NodeData.From+1] = 2 then
      begin
        If NodeData.From+1 = NodeTurn then
        begin
          If NodeHand2[NodeData.From+1] = '' then
            GiveMSG (9, '', '^^0DYou don''t have a second hand!', NodeData.From+1)
          else
          begin
            If NodeDoneH2[NodeData.From+1] then GiveMSG (9, '', '^^0DYou can''t play that hand!', NodeData.From+1)
            else
            begin
              BroadcastBut (9, '', '^^0F'+NodeUser(NodeTurn)+' ^^0Dhits second hand.', NodeData.From+1);
              NodeHand2[NodeData.From+1] := NodeHand2[NodeData.From+1] + GetCard;
              DisplayHand (NodeData.From+1);
              SecPassed := 0;
              Writeln (NodeUser(NodeTurn), ' hits second hand.');
              If HandValue(NodeHand2[NodeData.From+1]) > 21 then
              begin
                GiveMSG (9, '', '^^8DYou busted!', NodeData.From+1);
                BroadCastBut (9, '', '^^0F'+NodeUser(NodeTurn)+' ^^0Dbusts!', NodeData.From+1);
                BroadCastBut (9, '', '^^0F'+NodeUser(NodeTurn)+'^^8D loses ^^0F'+ToStr(NodeBet[NodeTurn])+' ^^0DCyberdollars!'
                              , NodeData.From+1);
                GiveMSG (9, '', '^^0FYou^^8D lose ^^0F'+ToStr(NodeBet[NodeTurn])+' ^^0DCyberdollars!', NodeData.From+1);
                Dec (NodeCD[NodeData.From+1], NodeBet[NodeData.From+1]);
                Writeln (NodeUser(NodeTurn), ' busts.');
                NodeDoneH2[NodeData.From+1] := True;
                If NodeDoneH1[NodeData.From+1] and NodeDoneH2[NodeData.From+1] then NextPlayer;
              end;
            end;
          end;
        end
        else
          GiveMSG (9, '', '^^0DIt is not your turn!', NodeData.From+1);
      end
      else
        GiveMSG (9, '',  '^^0DYou aren''t playing in the ^^0Fblackjack^^0D game.', NodeData.From+1);
    end;
    If (s = 'HELP') or (s = '!') or (s = '?') or (s = 'COMMANDS') then
    begin
      For I := 1 to 15 do
        GiveMSG (9, '', HelpData[I], NodeData.From+1);
    end;
    If (s='SPLIT')or(s='SPLI')or(s='SPL')or(s='SPLITT')or(s='SPLTI') then
    begin
      If NodeStat[NodeData.From+1] = 2 then
        If NodeData.From+1 = NodeTurn then
        begin
          If (Length (NodeHand[NodeData.From+1]) = 4) and (NodeHand2[NodeData.From+1] = '') then
          begin
            If HandValue (NodeHand[NodeData.From+1][1]+NodeHand[NodeData.From+1][2])
               = HandValue (NodeHand[NodeData.From+1][3]+NodeHand[NodeData.From+1][4]) then
            begin
              GiveMSG (9, '', '^^0DOk, you split.', NodeData.From+1);
              SecPassed := 0;
              BroadcastBut (9, '', '^^0F'+NodeUser(NodeTurn)+' ^^0Dsplits.', NodeData.From+1);
              NodeHand2[NodeData.From+1] := NodeHand[NodeData.From+1][3]+NodeHand[NodeData.From+1][4];
              Dec (NodeHand[NodeData.From+1][0], 2);
              NodeHand[NodeData.From+1] := NodeHand[NodeData.From+1] + GetCard;
              NodeHand2[NodeData.From+1] := NodeHand2[NodeData.From+1] + GetCard;
              DisplayHand (NodeData.From+1);
            end
            else GiveMSG (9, '', '^^0DYou can''t split when card values are different!', NodeData.From+1)
          end
          else GiveMSG (9, '', '^^0DYou can''t split now!', NodeData.From+1)
        end
        else
          GiveMSG (9, '', '^^0DIt is not your turn!', NodeData.From+1)
      else
        GiveMSG (9, '',  '^^0DYou aren''t playing in the ^^0Fblackjack^^0D game.', NodeData.From+1);
    end;
    If (s='DOUBLE')or(s='DOUBLEDOWN')or(s = 'DD')or(s='DOUBLE1')or(s='DOUBLEDOWN1')or(s='DD1')or
       (s='DOUBL')or(s='DOUBLEE')or(s='DOUB')or(s='DOUBEL')or(s='DUOBLE')or(s='DOUBKLE') then
    begin
      If NodeStat[NodeData.From+1] = 2 then
      begin
        If NodeData.From+1 = NodeTurn then
        begin
          BroadcastBut (9, '', '^^0F'+NodeUser(NodeTurn)+' ^^0Ddoubles down.', NodeData.From+1);
          NodeHand[NodeData.From+1] := NodeHand[NodeData.From+1] + GetCard;
          NodeDD[NodeData.From+1] := 2;
          GiveMSG (9, '', '^^0DOk, you ^^0Fdouble down^^0D!', NodeData.From+1);
          DisplayHand (NodeData.From+1);
          SecPassed := 0;
          Writeln (NodeUser(NodeTurn), ' doubles down.');
          If HandValue(NodeHand[NodeData.From+1]) > 21 then
          begin
            GiveMSG (9, '', '^^8DYou busted!', NodeData.From+1);
            BroadCastBut (9, '', '^^0F'+NodeUser(NodeTurn)+' ^^0Dbusts!', NodeData.From+1);
            BroadCastBut (9, '', '^^0F'+NodeUser(NodeTurn)+'^^8D loses ^^0F'+ToStr(NodeBet[NodeTurn]*NodeDD[NodeTurn])
                          +' ^^0DCyberdollars!', NodeData.From+1);
            GiveMSG (9, '', '^^0FYou^^8D lose ^^0F'+ToStr(NodeBet[NodeTurn]*NodeDD[NodeTurn])+' ^^0DCyberdollars!',
                     NodeData.From+1);
            Dec (NodeCD[NodeData.From+1], NodeBet[NodeData.From+1]*NodeDD[NodeData.From+1]);
            Writeln (NodeUser(NodeTurn), ' busts.');
          end;
          NodeDoneH1[NodeData.From+1] := True;
          If NodeHand2[NodeData.From+1] = '' then NextPlayer
          else If NodeDoneH1[NodeData.From+1] and NodeDoneH2[NodeData.From+1]
            then NextPlayer;
        end
        else
          GiveMSG (9, '', '^^0DIt is not your turn!', NodeData.From+1);
      end
      else
        GiveMSG (9, '',  '^^0DYou aren''t playing in the ^^0Fblackjack^^0D game.', NodeData.From+1);
    end;
    If (s = 'DOUBLE2') or (s = 'DOUBLEDOWN2') or (s = 'DD2') then
    begin
      If NodeStat[NodeData.From+1] = 2 then
      begin
        If NodeData.From+1 = NodeTurn then
        begin
          If NodeHand2[NodeData.From+1] = '' then
            GiveMSG (9, '', '^^0DYou don''t have a second hand!', NodeData.From+1)
          else
          begin
            BroadcastBut (9, '', '^^0F'+NodeUser(NodeTurn)+' ^^0Ddoubles down on second hand.', NodeData.From+1);
            NodeHand2[NodeData.From+1] := NodeHand2[NodeData.From+1] + GetCard;
            NodeDD2[NodeData.From+1] := 2;
            GiveMSG (9, '', '^^0DOk, you ^^0Fdouble down^^0D!', NodeData.From+1);
            DisplayHand (NodeData.From+1);
            SecPassed := 0;
            Writeln (NodeUser(NodeTurn), ' doubles down on second hand.');
            If HandValue(NodeHand2[NodeData.From+1]) > 21 then
            begin
              GiveMSG (9, '', '^^8DYou busted!', NodeData.From+1);
              BroadCastBut (9, '', '^^0F'+NodeUser(NodeTurn)+' ^^0Dbusts!', NodeData.From+1);
              BroadCastBut (9, '', '^^0F'+NodeUser(NodeTurn)+'^^8D loses ^^0F'+ToStr(NodeBet[NodeTurn]*NodeDD2[NodeTurn])
                            +' ^^0DCyberdollars!', NodeData.From+1);
              GiveMSG (9, '', '^^0FYou^^8D lose ^^0F'+ToStr(NodeBet[NodeTurn]*NodeDD2[NodeTurn])+' ^^0DCyberdollars!',
                       NodeData.From+1);
              Dec (NodeCD[NodeData.From+1], NodeBet[NodeData.From+1]*NodeDD2[NodeData.From+1]);
              Writeln (NodeUser(NodeTurn), ' busts.');
            end;
            NodeDoneH2[NodeData.From+1] := True;
            If NodeDoneH1[NodeData.From+1] and NodeDoneH2[NodeData.From+1] then NextPlayer;
          end
        end
        else
          GiveMSG (9, '', '^^0DIt is not your turn!', NodeData.From+1);
      end
      else
        GiveMSG (9, '',  '^^0DYou aren''t playing in the ^^0Fblackjack^^0D game.', NodeData.From+1);
    end;
    If (s='TURN')or(s='TUR')or(s='TUNR') then
    begin
      If GamePlaying then
      begin
        If NodeTurn = NodeData.From+1 then
          GiveMSG (9, '', '^^0DIt is ^^0Fyour ^^0Dturn in blackjack.',
                   NodeData.From+1)
        else
          GiveMSG (9, '', '^^0DIt is ^^0F'+NodeUser(NodeTurn)+'^^0D''s turn in blackjack.',
                   NodeData.From+1);
      end
      else
        GiveMSG (9, '', '^^0DThere is no blackjack game being played at the moment.', NodeData.From+1);
    end;
    If (s='SCAN')or(s='SCA')or(s='SCNA') then
    begin
      GiveMSG (9, '', #13#10'^^0EPlayer                          Playing    Cyberdollars', NodeData.From+1);
      GiveMSG (9, '', '^^0E-------------------------------------------------------', NodeData.From+1);
      For I := 1 to 255 do
      begin
        If NodeStat[I] = 1 then
          GiveMSG (9, '', '^^0E'+NodeUser(I)+Spaces(34-Length(KillRAPCodes(NodeUser(I)))) +' No         '+ToStr(NodeCD[I]),
                   NodeData.From+1);
        If NodeStat[I] = 2 then
          GiveMSG (9, '', '^^0E'+NodeUser(I)+Spaces(34-Length(KillRAPCodes(NodeUser(I)))) +'Yes         '+ToStr(NodeCD[I]),
                   NodeData.From+1);
      end;
      GiveMSG (9, '', #0, NodeData.From+1);
    end;
    If (s='BET')or(s='BTE') then
      GiveMSG (9, '', '^^0DYour current bet is ^^0F'+ToStr(NodeBet[NodeData.From+1])+' ^^0DCyberdollars.', NodeData.From+1)
    else
    If (s[1] = 'B') and (s[2] = 'E') and (s[3] = 'T') and (s[0] > #3) then
    begin
      If NodeStat[NodeData.From+1] < 2 then
      begin
        Delete (s, 1, 3);
        Val (s, L, Error);
        If (Error <> 0) or (L > ConfigData.MaxBet) or (L < ConfigData.MinBet) then
        begin
          GiveMSG (9, '', '^^0DBets must be between ^^0F'+ToStr(ConfigData.MinBet)+' ^^0Dand ^^0F'+ToStr(ConfigData.MaxBet)+
                   ' ^^0DCyberdollars.', NodeData.From+1);
        end
        else
        begin
          NodeBet[NodeData.From+1] := L;
          GiveMSG (9, '', '^^0DOk, your bet will now be ^^0F'+ToStr(L)+' ^^0DCyberdollars.', NodeData.From+1);
        end;
      end
      else
        GiveMSG (9, '', '^^0DSorry, you cannot change your bet during a game.', NodeData.From+1);
    end;
  end;
  If (NodeData.Kind = 3) or (NodeData.Kind = 19) then
  begin
    If NodeStat[NodeData.From+1] = 2 then
    begin
      Dec (NodeCD[NodeData.From+1], NodeBet[NodeData.From+1]*NodeDD[NodeData.From+1]);
      If (NodeHand2[Node] <> '') and (HandValue (NodeHand2[Node]) < 22) then
        Dec (NodeCD[Node], NodeBet[Node]*NodeDD2[Node]);
      If NodeInsure[Node] then Dec (NodeCD[Node], Round(NodeBet[Node]/2));
      NodeStat[NodeData.From+1] := 0;
      BroadCastBut (9,'','^^0F'+CStr(NodeData.Alias)+' ^^0Dis dealt out of the ^^0Fblackjack^^0D game!',
                    NodeData.From+1);
      Writeln (CStr(NodeData.Alias), ' is dealt out of the blackjack game.');
      If (NodeData.From+1) = NodeTurn then NextPlayer;
    end;
    NodeCD[NodeData.From+1] := ConfigData.StartCD;
    NodeBet[NodeData.From+1] := ConfigData.BetCD;
  end;
  If (NodeData.Kind = 1) then
  begin
    s := UpStr(CStr(NodeData.Data));
    For J := 1 to 16 do
      If s = ComList[J] then GiveMSG (6, '^^0FThe blackjack dealer', 'You missed a "^^0A/^^0E" in your blackjack command.'
                                      , NodeData.From+1);
  end;
  If (NodeData.Kind = 10) and (NodeData.DoneTo+1 = Node) then
  begin
    s := CStr(NodeData.Data);
    If Pos ('just barfed ALL OVER', s) <> 0 then Retal (NodeData.From+1);
    If Pos ('just bonked',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('is booting',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('with a disruptor!',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('just tossed a grenade!',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('is hitting',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('is kicking the life out of',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('with a mushroom cloud!',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('a phaser and blasted',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('is punching',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('just shot',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('just slapped',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('across the back of the head!',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('just spit on',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('is stripping',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('just zapped',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('just bashed',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('just stapled',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('just killed',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('just elbowed',s) <> 0 then Retal (NodeData.From+1);
    If Pos ('just poked',s) <> 0 then Retal (NodeData.From+1);
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
  Assign (f, IPCDir + 'MIDX'+NodeStr+'.RAP');
  Assign (NodeFile, IPCDir + 'NODE'+NodeStr+'.RAP');
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
  Assign (f, IPCDir + 'CHGIDX.RAP');
  Reset (f);
  Seek (f, Node - 1);
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
  {$IFDEF OLDRAP}
  {$ELSE}
  NodeData2.TTTStart := -1;
  {$ENDIF}
  NodeData2.DoneTo := -1;
  NodeData2.From := Node - 1;
  NodeData2.Gender := 0;
  NodeData2.Alias := '';
  NodeData2.Kind := 9;
  SomeoneIn := False;
  { Get dealer's hand }
  DealerHand := GetCard + GetCard;
  If (Random(5) = 0) and (not registered) then RegRemind;
  For I := 1 to 255 do
    If NodeStat[I] > 0 then
    begin
      SomeoneIn := True;
      GiveMSG (9, '', #0, I);
      GiveMSG (9, '', '^^0DA new ^^0Fblackjack ^^0Dgame has begun!', I);
      NodeStat[I] := 2;
    end;
  { Deal first cards }
  For I := 1 to 255 do
    If NodeStat[I] = 2 then
    begin
      NodeHand[I] := GetCard + GetCard;
      GiveMSG (9, '', '^^0DDealer''s cards: '+ColorHand(DealerHand[1]+DealerHand[2])+'^^70  ^^0D  (^^0F'
               +ToStr(HandValue(DealerHand[1]+DealerHand[2]))+'^^0D)', I);
      GiveMSG (9, '', '^^0DYour cards:     '+ColorHand(NodeHand[I])+' ^^0D(^^0F'+ToStr(HandValue(NodeHand[I]))+'^^0D)', I);
      If DealerHand[1] = 'A' then GiveMSG (9, '',
        '^^0DDealer has possible blackjack, you may want to make an insurance bet.', I);
    end;
  If SomeoneIn then
  begin
    For I := 255 downto 1 do If NodeStat[I] = 2 then NodeTurn := I;
    GamePlaying := True;
    GiveMSG (9, '', '^^0DIt is your turn in ^^0Fblackjack^^0D!', NodeTurn);
    Writeln ('It is ', NodeUser(NodeTurn), '''s turn in blackjack.');
  end
  else Writeln ('No players.');
  SecPassed := 0;
end;

Procedure WarnUser;
begin
  GiveMSG (9, '', #7'^^8DIt is still your turn in ^^8Fblackjack^^8D!', NodeTurn);
  WaitSec;
end;

Procedure DealerPlay;
Var
  I : Word;
  DealerValue : Word;
begin
  BroadCast (9, '', '');
  BroadCast (9, '', '^^0DThe ^^0FBlackjack dealer ^^0Dnow plays.');
  WaitSec;
  BroadCast (9, '', '^^0DDealer''s cards: '+ColorHand(DealerHand)+' ^^0D(^^0F'+ToStr(HandValue(DealerHand))+'^^0D)');
  WaitSec;
  While HandValue (DealerHand) < 17 do
  begin
    Writeln ('Dealer hits.');
    BroadCast (9, '', '^^0FDealer ^^0Dhits.');
    DealerHand := DealerHand + GetCard;
    WaitSec;
    BroadCast (9, '', '^^0DDealer''s cards: '+ColorHand(DealerHand)+' ^^0D(^^0F'+ToStr(HandValue(DealerHand))+'^^0D)');
    WaitSec;
  end;
  WaitSec;
  If HandValue (DealerHand) < 22 then BroadCast (9, '', '^^0FDealer ^^0Dstays.'#13#10)
  else BroadCast (9, '', '^^0FDealer ^^0Dbusts!'#13#10);
  BroadCast (9, '', '^^0DThe ^^0Fblackjack ^^0Dgame is over!');

  { Pay the winners }
  DealerValue := HandValue (DealerHand);
  If DealerValue > 21 then DealerValue := 0;
  If (DealerValue = 21) and (Length (DealerHand) = 4) then
  begin
    { Blackjack for dealer }
    BroadCast (9, '', '^^0DThe ^^0Fdealer ^^0Dhas a ^^8Fblackjack^^0D!');
    For I := 1 to 255 do if NodeStat[I] = 2 then
    begin
      If NodeInsure[I] then
      begin
        Inc (NodeCD[I], Round (NodeBet[I]));
        GiveMSG (9, '', '^^0FYou^^8D win ^^0Dyour insurance bet and get ^^0F'+ToStr(Round(NodeBet[I]))
                 +' ^^0DCyberdollars!', I);
        If NodeGender (I) then
          BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D wins ^^0Dher insurance bet and gets ^^0F'
                        +ToStr(Round(NodeBet[I]))+' ^^0DCyberdollars!', I)
        else
          BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D wins ^^0Dhis insurance bet and gets ^^0F'
                        +ToStr(Round(NodeBet[I]))+' ^^0DCyberdollars!', I)
      end;
      If (HandValue (NodeHand[I]) = 21) and (Length(NodeHand[I]) = 4) then
      begin
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^0D''s cards: '+ColorHand(NodeHand[I])+
                   ' ^^0D(^^0F'+ToStr(HandValue(NodeHand[I]))+'^^0D)', I);
        GiveMSG (9, '', '^^0FYour^^0D cards: '+ColorHand(NodeHand[I])+
                   ' ^^0D(^^0F'+ToStr(HandValue(NodeHand[I]))+'^^0D)', I);
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^0D loses nothing.',I);
        GiveMSG (9, '', '^^0FYou^^0D lose nothing.',I);
      end
      else
      begin
        If HandValue (NodeHand[I]) < 22 then
        begin
          BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^0D''s cards: '+ColorHand(NodeHand[I])+
                        ' ^^0D(^^0F'+ToStr(HandValue(NodeHand[I]))+'^^0D)', I);
          GiveMSG (9, '', '^^0FYour^^0D cards: '+ColorHand(NodeHand[I])+
                  ' ^^0D(^^0F'+ToStr(HandValue(NodeHand[I]))+'^^0D)', I);
          BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D loses ^^0F'+ToStr(NodeBet[I]*NodeDD[I])+' ^^0DCyberdollars!',I);
          GiveMSG (9, '', '^^0FYou^^8D lose ^^0F'+ToStr(NodeBet[I]*NodeDD[I])+' ^^0DCyberdollars!',I);
          Dec (NodeCD[I], NodeBet[I]*NodeDD[I]);
        end;
      end;
      If (HandValue (NodeHand2[I]) = 21) and (Length(NodeHand2[I]) = 4) and
         (NodeHand2[I] <> '') then
      begin
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^0D''s second hand: '+ColorHand(NodeHand2[I])+
                   ' ^^0D(^^0F'+ToStr(HandValue(NodeHand2[I]))+'^^0D)', I);
        GiveMSG (9, '', '^^0FYour^^0D second hand: '+ColorHand(NodeHand2[I])+
                   ' ^^0D(^^0F'+ToStr(HandValue(NodeHand2[I]))+'^^0D)', I);
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^0D loses nothing.',I);
        GiveMSG (9, '', '^^0FYou^^0D lose nothing.',I);
      end
      else
      begin
        If (NodeHand2[I] <> '') and (HandValue (NodeHand2[I]) < 22) then
        begin
          BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^0D''s second hand: '+ColorHand(NodeHand2[I])+
                     ' ^^0D(^^0F'+ToStr(HandValue(NodeHand2[I]))+'^^0D)', I);
          GiveMSG (9, '', '^^0FYour^^0D second hand: '+ColorHand(NodeHand2[I])+
                     ' ^^0D(^^0F'+ToStr(HandValue(NodeHand2[I]))+'^^0D)', I);
          BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D loses ^^0F'+ToStr(NodeBet[I]*NodeDD2[I])+' ^^0DCyberdollars!',I);
          GiveMSG (9, '', '^^0FYou^^8D lose ^^0F'+ToStr(NodeBet[I]*NodeDD2[I])+' ^^0DCyberdollars!',I);
          Dec (NodeCD[I], NodeBet[I]*NodeDD2[I]);
        end;
      end;
    end;
  end
  else
  For I := 1 to 255 do if NodeStat[I] = 2 then
  begin
    If HandValue (NodeHand[I]) < 22 then
    begin
      BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^0D''s cards: '+ColorHand(NodeHand[I])+
                 ' ^^0D(^^0F'+ToStr(HandValue(NodeHand[I]))+'^^0D)', I);
      GiveMSG (9, '', '^^0FYour^^0D cards: '+ColorHand(NodeHand[I])+
                 ' ^^0D(^^0F'+ToStr(HandValue(NodeHand[I]))+'^^0D)', I);
      If (HandValue (NodeHand[I]) = 21) and (Length (NodeHand[I]) = 4) then
      begin
        { Blackjack }
        GiveMSG (9, '', '^^8DYou got a ^^8Fblackjack^^8D!', I);
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+' ^^0Dhas a ^^0Fblackjack^^0D!', I);
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D wins ^^0F'+ToStr(Round(NodeBet[I]*(3/2)))
                          +' ^^0DCyberdollars!', I);
        GiveMSG (9, '', '^^0FYou^^8D win ^^0F'+ToStr(Round(NodeBet[I]*(3/2)))+' ^^0DCyberdollars!', I);
        Inc (NodeCD[I], Round(NodeBet[I]*(3/2)));
        Writeln (NodeUser(I), ' gets a blackjack.');
      end
      else
      If HandValue (NodeHand[I]) > DealerValue then
      begin
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D wins ^^0F'+ToStr(NodeBet[I]*NodeDD[I])+' ^^0DCyberdollars!',I);
        GiveMSG (9, '', '^^0FYou^^8D win ^^0F'+ToStr(NodeBet[I]*NodeDD[I])+' ^^0DCyberdollars!',I);
        Inc (NodeCD[I], NodeBet[I]*NodeDD[I]);
      end
      else
      If HandValue (NodeHand[I]) < DealerValue then
      begin
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D loses ^^0F'+ToStr(NodeBet[I]*NodeDD[I])+' ^^0DCyberdollars!',I);
        GiveMSG (9, '', '^^0FYou^^8D lose ^^0F'+ToStr(NodeBet[I]*NodeDD[I])+' ^^0DCyberdollars!',I);
        Dec (NodeCD[I], NodeBet[I]*NodeDD[I]);
      end
      else
      If HandValue (NodeHand[I]) = DealerValue then
      begin
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^0D loses nothing.',I);
        GiveMSG (9, '', '^^0FYou^^0D lose nothing.',I);
      end;
      If NodeInsure[I] then
      begin
        Dec (NodeCD[I], Round (NodeBet[I]/2));
        GiveMSG (9, '', '^^0FYou^^8D lose ^^0Dyour insurance bet of ^^0F'+ToStr(Round(NodeBet[I]/2))
                 +' ^^0DCyberdollars!', I);
        If NodeGender (I) then
          BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D loses ^^0Dher insurance bet of ^^0F'
                        +ToStr(Round(NodeBet[I]/2))+' ^^0DCyberdollars!', I)
        else
          BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D loses ^^0Dhis insurance bet of ^^0F'
                        +ToStr(Round(NodeBet[I]/2))+' ^^0DCyberdollars!', I)
      end;
    end;
    If (NodeHand2[I] <> '') and (HandValue (NodeHand2[I]) < 22) then
    begin
      BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^0D''s second hand: '+ColorHand(NodeHand2[I])+
                 ' ^^0D(^^0F'+ToStr(HandValue(NodeHand2[I]))+'^^0D)', I);
      GiveMSG (9, '', '^^0FYour^^0D second hand: '+ColorHand(NodeHand2[I])+
                 ' ^^0D(^^0F'+ToStr(HandValue(NodeHand2[I]))+'^^0D)', I);
      If (HandValue (NodeHand2[I]) = 21) and (Length (NodeHand2[I]) = 4) then
      begin
        { Blackjack }
        GiveMSG (9, '', '^^8DYou got a ^^8Fblackjack^^8D!', I);
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+' ^^0Dhas a ^^0Fblackjack^^0D!', I);
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D wins ^^0F'+ToStr(Round(NodeBet[I]*(3/2)))
                          +' ^^0DCyberdollars!', I);
        GiveMSG (9, '', '^^0FYou^^8D win ^^0F'+ToStr(Round(NodeBet[I]*(3/2)))+' ^^0DCyberdollars!', I);
        Inc (NodeCD[I], Round(NodeBet[I]*(3/2)));
        Writeln (NodeUser(I), ' gets a blackjack.');
      end
      else
      If HandValue (NodeHand2[I]) > DealerValue then
      begin
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D wins ^^0F'+ToStr(NodeBet[I]*NodeDD2[I])+' ^^0DCyberdollars!',I);
        GiveMSG (9, '', '^^0FYou^^8D win ^^0F'+ToStr(NodeBet[I]*NodeDD2[I])+' ^^0DCyberdollars!',I);
        Inc (NodeCD[I], NodeBet[I]*NodeDD2[I]);
      end
      else
      If HandValue (NodeHand2[I]) < DealerValue then
      begin
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D loses ^^0F'+ToStr(NodeBet[I]*NodeDD2[I])+' ^^0DCyberdollars!',I);
        GiveMSG (9, '', '^^0FYou^^8D lose ^^0F'+ToStr(NodeBet[I]*NodeDD2[I])+' ^^0DCyberdollars!',I);
        Dec (NodeCD[I], NodeBet[I]*NodeDD2[I]);
      end
      else
      If HandValue (NodeHand2[I]) = DealerValue then
      begin
        BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^0D loses nothing.',I);
        GiveMSG (9, '', '^^0FYou^^0D lose nothing.',I);
      end;
      If NodeInsure[I] then
      begin
        Dec (NodeCD[I], Round (NodeBet[I]/2));
        GiveMSG (9, '', '^^0FYou^^8D lose ^^0Dyour insurance bet of ^^0F'+ToStr(Round(NodeBet[I]/2))
                 +' ^^0DCyberdollars!', I);
        If NodeGender (I) then
          BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D loses ^^0Dher insurance bet of ^^0F'
                        +ToStr(Round(NodeBet[I]/2))+' ^^0DCyberdollars!', I)
        else
          BroadCastBut (9, '', '^^0F'+NodeUser(I)+'^^8D loses ^^0Dhis insurance bet of ^^0F'
                        +ToStr(Round(NodeBet[I]/2))+' ^^0DCyberdollars!', I)
      end;
    end;
  end;
  GamePlaying := False;
  SecPassed := 0;
  For I := 1 to 255 do
  begin
    If NodeStat[I] = 2 then NodeStat[I] := 1;
    NodeDD[I] := 1;
    NodeDD2[I] := 1;
    NodeDoneH1[I] := False;
    NodeDoneH2[I] := False;
    NodeHand2[I] := '';
    NodeInsure[I] := False;
  end;
end;

Procedure BootUser;
begin
  GiveMSG (9, '', #7'^^8DYou have been thrown out of the ^^8Fblackjack^^8D game for taking too long!', NodeTurn);
  BroadCastBut (9, '', '^^0DThe ^^0FBlackjack dealer^^0D has thrown ^^0F'+NodeUser(NodeTurn)
                +'^^0D out of the game for taking too long!', NodeTurn);
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
    Delay (10);
    If (NodeTurn = -1) and (GamePlaying) then DealerPlay;
    If Keypressed then ch := Readkey;
  Until ch = #27;
end;

begin
  InitProgram;
  InitRAP;
  InitScreen;
  InitVars;
  RunBJ;
  DeInit;
end.
