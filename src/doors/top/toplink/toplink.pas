Program TOPLink;
{$V-}
{$M 16384,0,0}
Uses Crt, Dos, FosComR, Ansi_Drv, TOPErrT, TOPLSupp, MulAware, UnixDate, Reg;

Const
   fmReadOnly  = $00;
   fmWriteOnly = $01;
   fmReadWrite = $02;

   fmDenyAll   = $10;
   fmDenyWrite = $20;
   fmDenyRead  = $30;
   fmDenyNone  = $40;

   RecLock = 0;
   RecUnlock = 1;

   ProgVerStr = ' v2.00';

   FC : char = '[';
   BC : char = ']';

Type
  Str30 = String[30];
  Str255 = String[255];

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
             Actions : Boolean;
           end;

Type
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

Type
  ConfigRec = Record
                SystemName : String[31];
                GiveTimeSlice : Boolean;
                StripAnsi : Boolean;
                StripTOP : Boolean;
                ShowBBSName : Boolean;
                SendDelay : byte;
                CheckingDelay : byte;
                DCommandsOn : Boolean;
                BBSCommandsOn : Boolean;
                Channel : LongInt;
                MinSecurity : Word;
                MaxNodes : Word;
                CrashProt : Word;
              end;
Const
  ConfigData : ConfigRec
               = ( SystemName : 'An Unknown System';
                   GiveTimeSlice : True;
                   StripAnsi : False;
                   StripTOP : True;
                   ShowBBSName : True;
                   SendDelay : 6;
                   CheckingDelay : 1;
                   DCommandsOn : True;
                   BBSCommandsOn : True;
                   Channel : 1;
                   MinSecurity : 0;
                   MaxNodes : 10;
                   CrashProt : 30);
Type
  RegisterRec = record
                  TheReg : string;
                  TheName : string;
                end;
Const
  RegisterData : RegisterRec
               = ( TheReg : '00000000000';
                   TheName : 'UNREGISTERED');

Var
  Com : byte;
  BBSName : String;
  IPCDir : String;
  Node : Word;
  NodeData : NodeRec;
  NodeStr : String;
  LastTime : LongInt;
  Skips : Array [1..50] of String[80];
  LinesRCVD : Word;
  SavedScr : Array [1..4000] of byte;
  LastPoll : LongInt;
  Registered : Boolean;

Procedure Check_Register;
begin
  Registered := ValidateKey(RegisterData.TheName+#0, RegisterData.TheReg,
                            17476, 38162);
end;

Procedure Deinit;
Var
  B : byte;
  f : File of byte;
begin
  Assign (F, IPCDir + 'NODEIDX2.TCH');
  Reset (f);
  Repeat Until FLock(RecLock, FileRec(f).Handle, Node, 1) = 0;
  Seek (f, Node);
  b := 0;
  {$I-}
  Repeat
    Write (f,b);
  Until IOResult = 0;
  {$I+}
  Repeat Until FLock(RecUnlock, FileRec(f).Handle, Node, 1) = 0;
  Close (f);
end;

Procedure WriteField (Num : byte);
begin
  TextAttr := $1E;
  With ConfigData do
  Case Num of
    1 :
    begin
      GotoXY (25,4);
      Write (' '+SystemName+Spaces(31-Length (SystemName)));
    end;
    2 :
    begin
      GotoXY (25,5);
      If StripAnsi then Write (' On  ') else Write (' Off ');
    end;
    3 :
    begin
      GotoXY (25,6);
      If StripTOP then Write (' Off ') else Write (' On  ');
    end;
    4 :
    begin
      GotoXY (25,7);
      If ShowBBSName then Write (' On  ') else Write (' Off ');
    end;
    5 :
    begin
      GotoXY (25,8);
      Write (' ',SendDelay,'ms ');
    end;
    6 :
    begin
      GotoXY (25,9);
      Write (' ',CheckingDelay,'s ');
    end;
    7 :
    begin
      GotoXY (25,10);
      If DCommandsOn then Write (' On  ') else Write (' Off ');
    end;
    8 :
    begin
      GotoXY (25,11);
      If BBSCommandsOn then Write (' On  ') else Write (' Off ');
    end;
    9 :
    begin
      GotoXY (25,12);
      If GiveTimeSlice then Write (' On  ') else Write (' Off ');
    end;
    10:
    begin
      GotoXY(25,13);
      Write(' ',Channel:10,' ');
    end;
    11:
    begin
      GotoXY(25,14);
      Write(' ',MinSecurity:5, ' ');
    end;
    12:
    begin
      GotoXY(25,15);
      Write(' ',MaxNodes:3,' ');
    end;
    13:
    begin
      GotoXY(25,16);
      Write(' ',CrashProt:5,' ');
    end;
  end;
end;

Procedure InitCFGScr;
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
  Write (' TOPLink'+ProgVerStr);
  Textcolor (0);
  Write (' Configuration - Copyright 1996 Paul Sidorsky, ISMWare');
  ClrEol;
  GotoXY (1,25);
  Textcolor (Red);
  Write (' Help: ');
  Textcolor (0);
  Write ('Use cursor keys to select a choice.');
  ClrEol;
  WindowBorder (4,3,60,17,White,7);
  TextAttr := $7F;
  GotoXY (27,3);
  Write ('´ Config Ã');
  GotoXY (6,4);
  Textcolor (0);
  Writeln ('Your BBS Name:');
  GotoXY (6,5);
  Writeln ('Ansi Stripping:');
  GotoXY (6,6);
  Writeln ('TOP Code Sending: ');
  GotoXY (6,7);
  Writeln ('Show BBS Name:');
  GotoXY (6,8);
  Writeln ('Sending Delay:');
  GotoXY (6,9);
  Writeln ('Checking Delay:');
  GotoXY (6,10);
  Writeln ('Direct Text:');
  GotoXY (6,11);
  Writeln ('BBS Commands:');
  GotoXY (6,12);
  Writeln ('Give Time Slices:');
  GotoXY(6, 13);
  Writeln('Channel Number:');
  GotoXY(6, 14);
  Writeln('Minimum Security:');
  GotoXY(6, 15);
  Writeln('MaxNodes Setting:');
  GotoXY(6, 16);
  TextAttr := $78;
  Writeln('Crash Prot. Delay:');
  For Loop := 1 to 14 do WriteField (Loop);
end;

Procedure GetField (Num : Byte);
var
  S : String[30];
  I, Err : Integer;
  L : LongInt;
begin
  With ConfigData do
  Case Num of
    1 :
    begin
      Textcolor (15);
      GotoXY (26,4);
      CursorNorm;
      GetString (SystemName,30, SystemName);
      CursorOff;
    end;
    2 : StripAnsi := not StripAnsi;
    3 : StripTOP := not StripTOP;
    4 : ShowBBSName := not ShowBBSName;
    5 : If SendDelay <> 15 then Inc (SendDelay) else SendDelay := 0;
    6 : If CheckingDelay <> 5 then Inc (CheckingDelay) else CheckingDelay := 0;
    7 : DCommandsOn := not DCommandsOn;
    8 : BBSCommandsOn := not BBSCommandsOn;
    9 : GiveTimeSlice := not GiveTimeSlice;
    10:
    begin
      Repeat
        Textcolor (15);
        GotoXY (26,13);
        CursorNorm;
        Str(ConfigData.Channel, S);
        GetString (S, 10, S);
        CursorOff;
        Val(S, L, Err);
      Until (L >= 0);
      ConfigData.Channel := L;
    end;
    11:
    begin
      Repeat
        Textcolor (15);
        GotoXY (26,14);
        CursorNorm;
        Str(ConfigData.MinSecurity, S);
        GetString (S, 5, S);
        CursorOff;
        Val(S, I, Err);
      Until (I >= 0) AND (I <= 32767);
      ConfigData.MinSecurity := I;
    end;
    12:
    begin
      Repeat
        Textcolor (15);
        GotoXY (26,15);
        CursorNorm;
        Str(ConfigData.MaxNodes, S);
        GetString (S, 3, S);
        CursorOff;
        Val(S, I, Err);
      Until (I >= 0) AND (I <= 255);
      ConfigData.MaxNodes := I;
    end;
    13:
    begin
{      Repeat
        Textcolor (15);
        GotoXY (26,16);
        CursorNorm;
        Str(ConfigData.CrashProt, S);
        GetString (S, 5, S);
        CursorOff;
        Val(S, I, Err);
      Until (I >= 0) AND (I <= 32767);
      ConfigData.CrashProt := I;}
    end;
  end;
  WriteField (Num);
end;

Procedure GetCFGChoice;
Const
  HelpLine : Array [1..13] of String[70] = (
    'The name of your BBS system',
    'Controls if incoming ANSI is stripped',
    'Whether or not TOP codes in outgoing text is stripped',
    'Controls if the BBS name is displayed in front of incoming text',
    'Delay between each character sent to the com port in milliseconds',
    'Delay between checking for incoming messages',
    'Whether or not users are allowed to use direct text',
    'Whether or not the linked BBS can send TOPLink commands',
    'Whether or not time slices should be given to the operating system',
    'Channel number that TOPLink will use, or 0 for global',
    'Minimum security need to see and use TOPLink',
    'MaxNodes setting from TOP.CFG.  BOTH SETTINGS MUST BE THE SAME!',
    'Unused.  Be sure to set CrashProtDelay in TOP.CFG to 0!'
    );

  Procedure WriteBottom (message : String);
  Var
    X,Y : byte;
    Save : Word;
  begin
    X := WhereX; Y := WhereY;
    Save := TextAttr;
    GotoXY (1,25);
    TextAttr := $74;  Write (' ESC');
    TextAttr := $70;  Write (' Exit ³');
    TextAttr := $70;
    Write (#32+Message);
    ClrEol;
    GotoXY (X,Y);
    TextAttr := Save;
  end;

Var
  Loc : Byte;
  VideoMem : Array[1..4000] of byte absolute $B800:0000;
  Done : Boolean;
  Key : Word;

  Procedure HighLight (Num : byte);
  Var
    L : byte;
  begin
    For l := 0 to 18 do VideoMem [(Num)*160 + 330 + 2*l] := $2E;
  end;
  Procedure UnHighLight (Num : byte);
  Var
    L : byte;
  begin
    For l := 0 to 18 do VideoMem [(Num)*160 + 330 + 2*l] := $70;
  end;

begin
  CursorOff;
  Loc := 1;
  Done := False;
  Repeat
    HighLight (Loc);
    WriteBottom (HelpLine[Loc]);
    Key := Get_Key;
    Case Key of
      328 :
      begin
        UnHighLight (Loc);
        Dec(Loc);
        If Loc = 0 then Loc := 13;
      end;
      336 :
      begin
        UnHighLight(Loc);
        Inc(Loc);
        If Loc = 14 then Loc := 1;
      end;
      13 : GetField (Loc);
      27 : Done := True;
    end;
  Until Done;
end;

Procedure SaveConfig;
Var
  ExeFile    : file;
  HeaderSize : word;
  FileName : String;

begin
  FileName := ParamStr (0);
  Writeln ('Writing configuration to ', FileName);
  Assign (ExeFile, FileName);
  Reset (ExeFile, 1);
  Seek      (ExeFile, 8);
  Blockread (ExeFile, HeaderSize, Sizeof (HeaderSize));
  Seek      (ExeFile, Longint(16) * (seg(ConfigData)
            - PrefixSeg + HeaderSize) + ofs (ConfigData) - 256);
  Blockwrite (ExeFile, ConfigData, sizeof (ConfigData));
  Close      (ExeFile);
end;

Procedure SaveReg;
Var
  ExeFile    : file;
  HeaderSize : word;
  FileName : String;

begin
  FileName := ParamStr (0);
  Writeln ('Writing registration info. to ', FileName, '.');
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
  Writeln ('TOPLink Registration'); Writeln;
  Writeln('Your registration code and named will be saved to the file');
  Writeln(ParamStr(0), '.');
  Writeln;
  Write ('Your name (please note it is case sensitive): ');
  Readln (RegisterData.TheName);
  Write ('Registration Code: ');
  Readln (RegisterData.TheReg);
  Writeln;
  Check_Register;
  If Registered then Writeln ('The registration code checks out.  Thank you for registering!')
  else
  begin
    Writeln ('The registration code doesn''t check out.  Make sure you''ve entered');
    Writeln ('everything correctly!'+#7);
    Writeln;
    Writeln ('Name and registration code not saved.');
    Halt (1);
  end;
  Writeln;
  If Not FExists (ParamStr (0)) then Writeln ('Error: The file '+ParamStr(0)+' could not be written to.'+#7)
  else SaveReg;
end;

Procedure ConfigureTOPLink;
begin
  InitCFGScr;
  GetCFGChoice;
  If Ask2Save then
  begin
    SaveConfig;
  end;
  Writeln('TOPLink'+ProgVerStr+' Configuration ended.');
  Writeln('Copyright 1996 Paul Sidorsky, ISMWare.  All Rights Reserved.');
  Writeln('Original Configuration Copyright 1994 by David Ong.');
end;

Procedure Init_Program;
Var
  Error : Integer;
begin
  Writeln;
  Writeln ('TOPLink'+ProgVerStr+' - Links TOP to another teleconference over the modem!');
  Writeln ('Copyright 1996 Paul Sidorsky, ISMWare.');
  Writeln ('Original program Copyright 1994 by David Ong.');
  Writeln;
  If UpStr(ParamStr (1)) = '/CONFIG' then
  begin
    ConfigureTOPLink;
    Halt;
  end;
  If UpStr(ParamStr (1)) = '/REGISTER' then
  begin
    Enter_Reg;
    Halt;
  end;
  If ParamCount < 3 then
  begin
    Writeln ('Command line parameter(s) missing!');
    Writeln;
    Writeln ('Usage: TOPLINK <comport> <BBS Name> <work path> <node>');
    Writeln;
    Writeln ('<comport>    Is the com port you are going to use.');
    Writeln ('             COM1 = 1, COM2 = 2, etc.');
    Writeln ('<BBS Name>   Is the name of the BBS you are linking too');
    Writeln ('<work path>  Is the path to TOP''s work directory.');
    Writeln ('<node>       Is the node number that TOPLink should use (0-255)');
    Writeln;
    Writeln ('Eg: TOPLINK 2 ISMWare d:\top\workdir 4');
    Writeln ('For further help refer to TOPLINK.DOC');
    Halt;
  end;
  Val (ParamStr(1), Com, Error);
  If Error <> 0 then
  begin
    Writeln ('Invalid COM port passed!');
    Halt;
  end;
  BBSName := ParamStr(2);
  If BBSName[0] > #30 then BBSName[0] := #30;
  IPCDir := ParamStr(3);
  If IPCDir[Length(IPCDir)] <> '\' then IPCDir := IPCDir + '\';
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

Function CmdLineNode : byte;
Var
  L : Integer;
  Error : Integer;
begin
  Val (ParamStr(4), L, Error);
  If (Error <> 0) or (L <= 0) or (L > 255) then
  begin
    Writeln ('Invalid node passed!');
    Writeln ('Node must be from 0 to 255');
    Halt;
  end;
  CmdLineNode := L;
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
  Repeat Until FLock(RecLock, FileRec(f).Handle, Node, 1) = 0;
  Seek (f, Node);
  b := 1;
  {$I-}
  Repeat
    Write (f,b);
  Until IOResult = 0;
  {$I+}
  Repeat Until FLock(RecUnlock, FileRec(f).Handle, Node, 1) = 0;
  Close (f);
  Assign (IDXFile, IPCDir + 'NODEIDX.TCH');
  Reset (IDXFile);
  Repeat Until FLock(RecLock, FileRec(IDXFile).Handle, Node * SizeOf(IDXData), SizeOf(IDXData)) = 0;
  Seek (IDXFile, Node);
  FillChar (IDXData, SizeOf(IDXData), #0);
  IDXData.StructLength := SizeOf(IDXData);
  IDXData.Alias := 'APLink'+ProgVerStr+#0;
  IDXData.Alias[0] := 'R';
  IDXData.RealName := 'APLink'#0;
  IDXData.RealName[0] := 'R';
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
  IDXData.Quiet := False;
  IDXData.Task := 0;
  IDXData.LastAccess := TodayInUnix;
  IDXData.Channel := ConfigData.Channel;
  IDXData.ChannelListed := True;
  IDXData.Security := 65535;
  IDXData.Actions := True;
  {$I-}
  Repeat
    Write (IDXFile, IDXData);
  Until IOResult = 0;
  {$I+}
  Repeat Until FLock(RecUnlock, FileRec(IDXFile).Handle, Node * SizeOf(IDXData), SizeOf(IDXData)) = 0;
  Close (IDXFile);
end;

Procedure SendMSG (N : Word; NodeData : NodeRec);
Var
  b : byte;
  f : File of byte;
  NodeFile : File of NodeRec;
  X : Integer;
begin
  Assign (f, IPCDir + 'MIX' + PaddedNum(N)+'.TCH');
  Reset (f);
  b := 0;
  X := 0;
  If not Eof (f) then
  begin
    Repeat
      {$I-}
      Repeat Until FLock(RecLock, FileRec(f).Handle, X, 1) = 0;
      Repeat
        Read (f,b);
      Until IOResult = 0;
      {$I+}
      Repeat Until FLock(RecUnlock, FileRec(f).Handle, X, 1) = 0;
      Inc(X);
      If Eof (f) then
      begin
        Repeat Until FLock(RecLock, FileRec(f).Handle, X, 1) = 0;
        b := 0;
        Write (f,b);
        Repeat Until FLock(RecUnlock, FileRec(f).Handle, X, 1) = 0;
      end;
    Until B = 0;
  end
  else
  begin
    Repeat Until FLock(RecLock, FileRec(f).Handle, X, 1) = 0;
    Write (f,b);
    Repeat Until FLock(RecUnlock, FileRec(f).Handle, X, 1) = 0;
    Inc(X);
  end;
  b := 1;
  Dec(X);
  Repeat Until FLock(RecLock, FileRec(f).Handle, X, 1) = 0;
  Seek (f, FilePos(f) - 1);
  {$I-}
  Repeat
    Write (f,b);
  Until IOResult = 0;
  {$I+}
  Assign (NodeFile, IPCDir + 'MSG' + PaddedNum(N)+'.TCH');
  Reset (NodeFile);
  Repeat Until FLock(RecLock, FileRec(NodeFile).Handle, X * SizeOf(NodeData), SizeOf(NodeData)) = 0;
  Seek (NodeFile, FilePos(f) - 1);
  {$I-}
  Repeat
    Write (NodeFile, NodeData);
  Until IOResult = 0;
  {$I+}
  Repeat Until FLock(RecUnlock, FileRec(NodeFile).Handle, X * SizeOf(NodeData), SizeOf(NodeData)) = 0;
  Close (NodeFile);
  Repeat Until FLock(RecUnlock, FileRec(f).Handle, X, 1) = 0;
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

Procedure BroadCast (T : Integer; Alias : Str30; Data : Str255);
Var
  IDX2 : File of byte;
  L : Word;
  b : byte;
begin
  FillChar (NodeData, SizeOf(NodeData), #0);
  Alias := Alias + #0;
  Data := Data + #0;
  NodeData.StructLength := SizeOf(NodeData);
  NodeData.Kind := T;
  NodeData.From := Node;
  NodeData.Doneto := -1;
  NodeData.Gender := 0;
  NodeData.Alias := Minus1 (Alias);
  NodeData.Alias[0] := Alias[1];
  NodeData.Data := Minus1 (Data);
  NodeData.Data[0] := Data[1];
  NodeData.Channel := ConfigData.Channel;
  NodeData.MinSec := ConfigData.MinSecurity;
  NodeData.MaxSec := 65535;
  NodeData.Data1 := 0;
  Assign (IDX2, IPCDir + 'NODEIDX2.TCH');
  {$I-}
  Repeat
    Reset (IDX2);
  Until IOResult = 0;
  {$I+}
  For L := 0 to (ConfigData.MaxNodes - 1) do
  begin
    {$I-}
    Repeat Until FLock(RecLock, FileRec(IDX2).Handle, FilePos(IDX2), 1) = 0;
    Repeat
      Read (IDX2, b);
    Until IOResult = 0;
    {$I+}
    Repeat Until FLock(RecUnlock, FileRec(IDX2).Handle, FilePos(IDX2), 1) = 0;
    If (L <> Node) and (b=1) then SendMSG (L, NodeData);
  end;
  Close (IDX2);
end;

Procedure Init_TOP;
begin
  Node := CmdLineNode;
  Writeln ('Logging onto TOP as node ', Node);
  Str (Node, NodeStr);
  NodeStr := PaddedNum (Node);
  TakeNode;
  BroadCast (2, 'TOPLink'+ProgVerStr, ''); { TOPLink enters Pub }
  Fos_Init (Com);
  Delay (900);
end;

Procedure Init_Screen;
Var
  L : Word;
begin
  TextAttr := $71;  { Grey on blue }
  ClrScr;
  { Put background in, skipping the bottom line to create a status bar }
  For L := 1 to 1920 do Write ('°');
  TextAttr := $74;
  GotoXY (1,1);   ClrEol;
  GotoXY (1,1);  Writeln ('  TOPLink'+ProgVerStr+' Copyright 1996 Paul Sidorsky, ISMWare.  All Rights Reserved.');
  WindowBorder (2,3,77,19, White, Blue);
  GotoXY (5,3);
  Write (' Com Port: ', Com, ' ');
  GotoXY (60,3);
  Write (' Node: ', Node, ' ');
  GotoXY (72,3); Write ('Â');
  For L := 4 to 18 do
  begin
    GotoXY (72,L);
    Write ('³');
  end;
  WindowBorder (2,19,77,23, White, Blue);
  GotoXY (2,19); Write ('Ã');
  GotoXY (72,19); Write ('Á');
  GotoXY (77,19); Write ('´');
  GotoXY (3,20);
  Textcolor (lightcyan);  Write ('L');
  Textcolor (15); Write (')inked to: ');
  Textcolor (Yellow); Write (BBSName);
  GotoXY (3,21);
  Textcolor (lightcyan);  Write ('A');
  Textcolor (15); Write (')nsi Stripping is: ');
  Textcolor (Yellow);
  If ConfigData.StripAnsi then Write ('On ') else Write ('Off');
  GotoXY (40,21);
  Textcolor (lightcyan);  Write ('B');
  Textcolor (15); Write (')BS Name Displaying is: ');
  Textcolor (Yellow);
  If ConfigData.ShowBBSName then Write ('On ') else Write ('Off');
  GotoXY (3,22);
  Textcolor (lightcyan);  Write ('S');
  Textcolor (15); Write (')end TOP codes: ');
  Textcolor (Yellow);
  If ConfigData.StripTOP then Write ('Off ') else Write ('On ');
  GotoXY (40,22);
  Textcolor (lightcyan);  Write ('D');
  Textcolor (15); Write (')irect Text is: ');
  Textcolor (Yellow);
  If ConfigData.DCommandsOn then Write ('On ') else Write ('Off');
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
  GotoXY (2,25);
  Textcolor (0);
  Write ('<ESC> Exits TOPLink and disengages the link');
end;

Procedure Turn2Link;
Var
  IDXFile : File of IDXRec;
  IDXData : IDXRec;
begin
  Assign (IDXFile, IPCDir + 'NODEIDX.TCH');
  Reset (IDXFile);
  Repeat Until FLock(RecLock, FileRec(IDXFile).Handle, Node * SizeOf(IDXData), SizeOf(IDXData)) = 0;
  Seek (IDXFile, Node);
  FillChar (IDXData, SizeOf(IDXData), #0);
  IDXData.StructLength := SizeOf(IDXData);
  IDXData.Alias := Minus1 (BBSName)+#0;
  IDXData.Alias[0] := BBSName[1];
  IDXData.RealName := 'OPLink'#0;
  IDXData.RealName[0] := 'T';
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
  IDXData.Quiet := False;
  IDXData.Task := 0;
  IDXData.LastAccess := TodayInUnix;
  IDXData.Channel := ConfigData.Channel;
  IDXData.ChannelListed := True;
  IDXData.Security := 65535;
  IDXData.Actions := True;
  {$I-}
  Repeat
    Write (IDXFile, IDXData);
  Until IOResult = 0;
  {$I+}
  Repeat Until FLock(RecUnlock, FileRec(IDXFile).Handle, Node * SizeOf(IDXData), SizeOf(IDXData)) = 0;
  Close (IDXFile);
  BroadCast (22, 'TOPLink'+ProgVerStr, BBSName);
end;

Function MSGWait : boolean;
Var
  b : byte;
  f : File of byte;
begin
  Assign (f, IPCDir + 'CHGIDX.TCH');
  Reset (f);
  Repeat Until FLock(RecLock, FileRec(f).Handle, Node, 1) = 0;
  Seek (f, Node);
  {$I-}
  Repeat
    Read (f,b);
  Until IOResult = 0;
  {$I+}
  Repeat Until FLock(RecUnlock, FileRec(f).Handle, Node, 1) = 0;
  MSGWait := b = 1;
  Close (f);
end;

Function NodeUser (I : Integer) : String;
Var
  IDXFile : File of IDXRec;
  IDXData : IDXRec;
begin
  If I >= 0 then
  begin
    Assign (IDXFile, IPCDir + 'NODEIDX.TCH');
    Reset (IDXFile);
    Repeat Until FLock(RecLock, FileRec(IDXFile).Handle, I * SizeOf(IDXData), SizeOf(IDXData)) = 0;
    Seek (IDXFile, I);
    {$I-}
    Repeat
      Read (IDXFile, IDXData);
    Until IOResult = 0;
    {$I+}
    NodeUser := CStr (IDXData.Alias);
    Repeat Until FLock(RecUnlock, FileRec(IDXFile).Handle, I * SizeOf(IDXData), SizeOf(IDXData)) = 0;
    Close (IDXFile);
  end
  else NodeUser := 'everyone';
end;

Function NodeGender (I : Integer) : Boolean;
Var
  IDXFile : File of IDXRec;
  IDXData : IDXRec;
begin
  If I >= 0 then
  begin
    Assign (IDXFile, IPCDir + 'NODEIDX.TCH');
    Reset (IDXFile);
    Repeat Until FLock(RecLock, FileRec(IDXFile).Handle, I * SizeOf(IDXData), SizeOf(IDXData)) = 0;
    Seek (IDXFile, I);
    {$I-}
    Repeat
      Read (IDXFile, IDXData);
    Until IOResult = 0;
    {$I+}
    Repeat Until FLock(RecUnlock, FileRec(IDXFile).Handle, I * SizeOf(IDXData), SizeOf(IDXData)) = 0;
    Close (IDXFile);
    NodeGender := IDXData.Gender = 1;
  end
  else NodeGender := False;
end;

Function ReplaceAll (s : String) : String;
Var
  Replace : String;
  p : byte;
begin
  While Pos ('%m', s) <> 0 do
  begin
    Replace := Cstr(NodeData.Alias);
    p := Pos ('%m',s);
    Delete (s, p, 2);
    Insert (Replace, s, p);
  end;
  While Pos ('%y', s) <> 0 do
  begin
    If NodeData.DoneTo = Node then Replace := 'you' else
    Replace := NodeUser(NodeData.DoneTo);
    p := Pos ('%y',s);
    Delete (s, p, 2);
    Insert (Replace, s, p);
  end;
  While Pos ('%h', s) <> 0 do
  begin
    If NodeGender (NodeData.From) then Replace := 'her'
    else Replace := 'his';
    p := Pos ('%h',s);
    Delete (s, p, 2);
    Insert (Replace, s, p);
  end;
  While Pos ('%s', s) <> 0 do
  begin
    If NodeData.DoneTo = Node then Replace := 'r'
    else Replace := '''s';
    p := Pos ('%s',s);
    Delete (s, p, 2);
    Insert (Replace, s, p);
  end;
  While Pos ('%f', s) <> 0 do
  begin
    If NodeGender (NodeData.From) then Replace := 'herself'
    else Replace := 'himself';
    p := Pos ('%f',s);
    Delete (s, p, 2);
    Insert (Replace, s, p);
  end;
  While Pos ('%e', s) <> 0 do
  begin
    If NodeGender (NodeData.From) then Replace := 'she'
    else Replace := 'he';
    p := Pos ('%e',s);
    Delete (s, p, 2);
    Insert (Replace, s, p);
  end;
  ReplaceAll := s;
end;

Procedure Send2Scr (MSG : String; LocalMSG : String; SendFos : Boolean);
Var
  X, Y : byte;
  L : Word;
  Save : Word;
begin
  Save := TextAttr;
  Textbackground (0); Textcolor (15);
  X := WhereX; Y := WhereY;
  If SendFos then Fos_String (Com, MSG, ConfigData.SendDelay);
  For L := 1 to Length (msg) do Ansi_Write (Msg[L]);
  If Y <> 15 then
  begin
    Ansi_Write (#13);   Ansi_Write (#10);
  end;
  Window (73,4,77,18);
  GotoXY (1, Y);
  Textbackground (1); Textcolor (Yellow);
  Write (LocalMSG);
  Window (3,4,71,18);
  GotoXY (X,Y+1);
  If Y = 15 then
  begin
    asm
    Mov ah,6
    mov al,1
    mov ch,3
    mov cl,2
    mov dh,17
    mov dl,70
    mov bh,$0F
    int $10     { Scroll Screen }
    end;
    asm
    Mov ah,6
    mov al,1
    mov ch,3
    mov cl,72
    mov dh,17
    mov dl,75
    mov bh,$1F
    int $10     { Scroll Screen }
    end;
    GotoXY (1,15);
  end;
  TextAttr := Save;
end;

Procedure Send (NodeData : NodeRec);
Var
  s : string;
  L : byte;
  NodeData2 : NodeRec;
begin
  s := '';
  If (NodeData.Kind = 6) and (NodeData.Data[0] = '*') then
  begin
    For L := 1 to 255 do
      s[l-1] := NodeData.Data[l];
    s := CStr (s);
    If ConfigData.DCommandsOn then Send2Scr (S+#13,'DTxt',True);
    FillChar (NodeData2, SizeOf(NodeData2), #0);
    NodeData2.StructLength := SizeOf(NodeData2);
    NodeData2.Kind := 35;
    NodeData2.From := Node;
    NodeData2.Doneto := -1;
    NodeData2.Gender := 0;
    NodeData2.Alias := Minus1 (BBSName)+#0;
    NodeData2.Alias[0] := BBSName[1];
    If ConfigData.DCommandsOn then
    begin
      NodeData2.Data := '- Direct Text Sent --'+#0;
      NodeData2.Data[0] := '-';
    end
    else
    begin
      NodeData2.Data := '- Direct Text is ^^0FOFF^^0C --'+#0;
      NodeData2.Data[0] := '-';
    end;
    NodeData2.Channel := ConfigData.Channel;
    NodeData2.MinSec := ConfigData.MinSecurity;
    NodeData2.MaxSec := 65535;
    SendMSG (NodeData.From, NodeData2);
  end
  else if NodeData.Kind = 48 then
  begin
    NodeData2.Kind := 49;
    NodeData2.From := Node;
    NodeData2.DoneTo := -1;
    NodeData2.Gender := 0;
    NodeData2.Alias := Minus1(BBSName)+#0;
    NodeData2.Alias[0] := BBSName[1];
    NodeData2.Data := '';
    NodeData2.Data[0] := #0;
    NodeData2.Channel := ConfigData.Channel;
    NodeData2.MinSec := 0;
    NodeData2.MaxSec := 65535;
    SendMsg(NodeData.From, NodeData2);
  end
  else
  begin
    Case NodeData.Kind of
      1 : s := 'From '+CStr(NodeData.Alias)+ ': '+ CStr(NodeData.Data);
      2 : s := CStr(NodeData.Alias) + ' just entered the teleconference.';
      3 : s := CStr(NodeData.Alias) + ' just left the teleconference.';
      4 : s := CStr (NodeData.Alias) + ' has entered the Profile Editor.';
      5 : s := CStr (NodeData.Alias) + ' has returned from the Profile Editor.';
      7 : s := CStr(NodeData.Alias) + ' '+CStr (NodeData.Data);
      8 : s := CStr(NodeData.Alias) + '''s ' + CStr (NodeData.Data);
      9,10,11,12 :
      begin
        if (NodeData.Data[0] <> '%') AND ((NodeData.Data[1] <> 'P') AND (NodeData.Data[1] <> 'p')) then
          s := ReplaceAll(CStr(NodeData.Data));
      end;
      16 : s := CStr (NodeData.Alias)+' has changed genders and is now '
           +CStr(NodeData.Data)+'.';
      19 : s := CStr(NodeData.Alias)+' has just vanished from the teleconference.';
      22 : s := CStr(NodeData.Alias)+' has just transformed into a link to '
           +Cstr(NodeData.Data)+'.';
      23 : s := '('+ NodeUser(NodeData.From)+'): '+CStr(NodeData.Data);
      24 : s := 'The link to '+CStr(NodeData.Data)+' has just disintegrated.';
      26 : s := CStr(NodeData.Alias)+' has just been tossed out of the teleconference.';
      27 : s := CStr(NodeData.Alias)+' has just been zapped off of the system.';
      35 : s := CStr(NodeData.Data);
      36 : s := CStr(NodeData.Alias)+' has just entered this channel.';
      37 : s := CStr(NodeData.Alias)+' has just changed to a different channel.';
      39 : s := 'From '+NodeUser(NodeData.From)+', to '+NodeUser(NodeData.DoneTo)+': '+CStr(NodeData.Data);
      46 : s := CStr(NodeData.Alias)+' has just entered a private chat.';
      47 : s := CStr(NodeData.Alias)+' has just returned from a private chat.';
      50 : s := CStr(NodeData.Alias)+' has just changed the channel topic to "'+CStr(NodeData.Data)+'".';
      55 : s := CStr(NodeData.Alias)+' has just appointed '+NodeUser(NodeData.DoneTo)+' as a channel moderator.';
    end;
    If s <> '' then
    begin
      s := FC + s + BC;
      If ConfigData.StripTOP then s := KillTOPCodes (s);
      Send2Scr (S+#13, 'Sent',True);
    end;
  end;
end;

Procedure GetMSGs;
Var
  b : byte;
  z : byte;
  f : file of byte;
  X : Integer;
  NodeFile : File of NodeRec;
begin
  z := 0;
  X := 0;
  Assign (f, IPCDir + 'MIX'+NodeStr+'.TCH');
  Assign (NodeFile, IPCDir + 'MSG'+NodeStr+'.TCH');
  Reset (f);
  Reset (NodeFile);
  Repeat
    Repeat Until FLock(RecLock, FileRec(f).Handle, X, 1) = 0;
    {$I-}
    Repeat
      Read (f,b);
    Until IOResult = 0;
    if b = 1 then
    begin
      Seek (f, FilePos (f) - 1);
      Repeat
        Write (f,z);
      Until IOResult = 0;
      Repeat Until FLock(RecLock, FileRec(NodeFile).Handle, X * SizeOf(NodeData), SizeOf(NodeData)) = 0;
      Seek(NodeFile, X);
      Repeat
        Read (NodeFile, NodeData);
      Until IOResult = 0;
      Repeat Until FLock(RecUnlock, FileRec(NodeFile).Handle, X * SizeOf(NodeData), SizeOf(NodeData)) = 0;
      Send (NodeData);
    end;
    {$I+}
    Repeat Until FLock(RecUnlock, FileRec(f).Handle, X, 1) = 0;
    Inc(X);
  Until Eof (f);
  Close (f);
  Close (NodeFile);
  b := 0;
  Assign (f, IPCDir + 'CHGIDX.TCH');
  Reset (f);
  Seek (f, Node);
  {$I-}
  Repeat
    Write (f,b);
  Until IOResult = 0;
  {$I+}
  Close (f);
end;

Procedure Process (ch : Char);
Var
  Save : Word;
  X,Y : byte;
  Old : String;
begin
  Save := TextAttr;
  X := WhereX; Y := WhereY;
  Window (1,1,80,25);
  Case ch of
    'A' :
    begin
      ConfigData.StripAnsi := not ConfigData.StripAnsi;
      Textbackground (blue);
      GotoXY (3,21);
      Textcolor (lightcyan);  Write ('A');
      Textcolor (15); Write (')nsi Stripping is: ');
      Textcolor (Yellow);
      If ConfigData.StripAnsi then Write ('On ') else Write ('Off');
    end;
    'B' :
    begin
      ConfigData.ShowBBSName := Not ConfigData.ShowBBSName;
      Textbackground (blue);
      GotoXY (40,21);
      Textcolor (lightcyan);  Write ('B');
      Textcolor (15); Write (')BS Name Displaying is: ');
      Textcolor (Yellow);
      If ConfigData.ShowBBSName then Write ('On ') else Write ('Off');
    end;
    'L':
    begin
      Old := BBSName;
      Textbackground (blue); Textcolor (LightCyan);
      GotoXY (15,20);
      GetString (BBSName, 29, BBSName);
      If Old <> BBSName then Turn2Link;
      GotoXY (3,20);
      Textcolor (lightcyan);  Write ('L');
      Textcolor (15); Write (')inked to: ');
      Textcolor (Yellow); Write (BBSName, Spaces (31 - Length(BBSName)));
    end;
    'S':
    begin
      ConfigData.StripTOP := not ConfigData.StripTOP;
      Textbackground (blue);
      GotoXY (3,22);
      Textcolor (lightcyan);  Write ('S');
      Textcolor (15); Write (')end TOP codes: ');
      Textcolor (Yellow);
      If ConfigData.StripTOP then Write ('Off ') else Write ('On ');
    end;
    '$' :
    begin
      SaveScreen (SavedScr);
      TextAttr := $07;
      ClrScr;
      Writeln;
      Writeln ('Type EXIT to return to TOPLink...');
      SwapVectors;
      Exec (GetEnv ('COMSPEC'), '');
      SwapVectors;
      RestoreScreen (SavedScr);
    end;
    'D' :
    begin
      ConfigData.DCommandsOn := not ConfigData.DCommandsOn;
      GotoXY (40,22);
      Textbackground (blue);
      Textcolor (lightcyan);  Write ('D');
      Textcolor (15); Write (')irect Text is: ');
      Textcolor (Yellow);
      If ConfigData.DCommandsOn then Write ('On ') else Write ('Off');
    end;
  end;
  Window (3,4,71,18);
  TextAttr := Save;
  GotoXY (X,Y);
end;

Function InSkips (s : String) : Boolean;
Var
  L : byte;
  IsIn : Boolean;
begin
  IsIn := False;
  For L := 1 to 50 do
    If s = Skips[L] then IsIn := True;
  InSkips := IsIn;
end;

Procedure TOPCommands (s : string);
Var
  F : File of byte;
  L : Word;
  b : byte;
  Names : String;

begin
  s := UpStr (s);
  If Pos ('(TOP:WHO)', s) <> 0 then
  begin
    Send2Scr (FC+'Looking around '+ConfigData.SystemName+' you see:'+BC+#13,'-Who',True);
    Assign (F, IPCDir + 'NODEIDX2.TCH');
    Reset (f);
    Names := '';
    For L := 0 to (ConfigData.MaxNodes - 1) do
    begin
      Repeat Until FLock(RecLock, FileRec(f).Handle, L, 1) = 0;
      {$I-}
      Repeat
        Read (f,b);
      Until IOResult = 0;
      {$I+}
      If (b = 1) and (L <> Node) then Names := Names + NodeUser (L) + ', ';
      If Length (Names) > 220 then
      begin
        Names := Names + ',';
        Send2Scr (FC+Names+BC+#13,'-Who', True);
        Names := '';
      end;
      Repeat Until FLock(RecUnlock, FileRec(f).Handle, L, 1) = 0;
    end;
    Dec (Names[0],2);
    Names := Names + '.';
    Send2Scr (FC+Names+BC+#13,'-Who', True);
    Close (f);
  end;
  If Pos ('(TOP:VER)', s) <> 0 then
  begin
    Send2Scr (FC+' TOPLink'+ProgVerStr+' (c) 1994 by David Ong, All Rights Reserved '+BC+#13,'-Ver',True);
  end;
end;

Procedure LinkMode;
Var
  s : String;
  ch : Char;
  L : Word;
  key : Char;

begin
  Window (3,4,71,18);
  s := '';
  LastTime := MemL[$40:$6C];
  TextAttr := $0F;
  ClrScr;
  Fos_String (Com, #13,0);
  Delay (100);
  Send2Scr (FC+' TOPLink'+ProgVerStr+' Activated '+BC+#13, 'Sent', True);
  Repeat
    While Fos_Avail(Com) do
    begin
      Ch:=Fos_Receive(Com);
      If not (ch in [#0,#10,#13]) then s := s + ch
      else
      begin
        While s[Length(s)] = #32 do Dec (s[0]);
        If ConfigData.StripANSI then s := KillAnsi (s);
        If not (s[1] in [#13,#10])
        then
        begin
          If (KillAnsi(s) <> '') and (s[Ord(s[0])] <> BC)
          and not InSkips (KillAnsi(s))
          then
          begin
            If ConfigData.ShowBBSName then BroadCast (23, BBSName, s)
            else BroadCast (9,'',s);
            Send2Scr (s,'Brod',False);
            If ConfigData.BBSCommandsOn then TOPCommands (s);
            Inc (LinesRCVD);
            If LinesRCVD > 110 then
            begin
              If not Registered then
                BroadCast (9,'','^^8FThis version of TOPLink is UNREGISTERED!^^0E');
              LinesRCVD := 0;
            end;
          end
          else
            If KillAnsi(s) <> '' then Send2Scr (s, 'Rcvd',False);
        end;
        s := '';
      end;
    end;
    If MSGWait then GetMSGs;
    Repeat
      If ConfigData.GiveTimeSlice then TimeSlice;
      If Keypressed then
      begin
        key := Upcase(Readkey);
        Process (Key);
      end;
    Until ((MemL[$40:$6C] - LastTime) / 18.2) > ConfigData.CheckingDelay;
    LastTime := MemL[$40:$6C];
  Until (key = #27) {or not (Fos_Carrier(Com))};
  Fos_String (Com, #13,0);
  Send2Scr (FC+' TOPLink'+ProgVerStr+' Deactivated '+BC+#13,'Sent', True);
  BroadCast (24, 'TOPLink'+ProgVerStr, '');
  Delay (1000);
  Window (1,1,80,25);
  TextAttr := $07;
  ClrScr;
  Writeln ('TOPLink'+ProgVerStr+' ended.');
end;

Procedure Init_Other;
Var
  F : Text;
  FileName : String;
  Count : byte;
begin
  FileName := ParamStr (0);
  While (FileName[length (FileName)] <> '\') And (Length (FileName) > 0) do Dec (FileName[0]);
  FileName := FileName + 'DONTSHOW.TXT';
  If not FExists (FileName) then
  begin
    Writeln;
    Writeln ('The file: ', FileName, ' could not be found/read');
    Writeln ('All text will be shown.');
    Writeln;
    Delay (5000);
  end
  else
  begin
    Assign (f, FileName);
    Reset (f);
    Count := 1;
    While not (Eof (f)) and (Count <> 51) do
    begin
      Readln (f, Skips[Count]);
      Inc (Count);
    end;
    Close (f);
  end;
end;

Procedure InitVars;
Var
  L : byte;
begin
  Check_Register;
  FileMode:=fmReadWrite+FmDenyNone;
  For L := 1 to 50 do Skips[L] := '';
  LinesRCVD := 0;
end;

Procedure KeepAlive;
Var
  F : File of IDXRec;
  NodeTmp : IDXRec;
  X : Word;
begin
  if TodayInUnix >= (LastPoll + ConfigData.CrashProt) then
  begin
    Assign (F, IPCDir + 'NODEIDX.TCH');
    Reset (F);
    {$I-}
    Repeat Until FLock(RecLock, FileRec(F).Handle, (Node * SizeOf(NodeTmp)), SizeOf(NodeTmp)) = 0;
    Seek(F, Node);
    Read(F, NodeTmp);
    NodeTmp.LastAccess := TodayInUnix;
    Seek(F, Node);
    Write(F, NodeTmp);
    Repeat Until FLock(RecUnlock, FileRec(F).Handle, (Node * Sizeof(NodeTmp)), SizeOf(NodeTmp)) = 0;
    {$I+}
    Close(F);
  end;
end;

begin
  InitVars;
  Init_Program;
  Init_TOP;
  Init_Other;
  Init_Screen;
  Turn2Link;
  LinkMode;
  DeInit;
end.
