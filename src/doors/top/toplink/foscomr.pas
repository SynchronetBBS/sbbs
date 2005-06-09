{
                     Version 1.2  26-August-1989

‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
€±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±€
€±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±€
€±±±±±±±±€€€€€€€€€€€€€€€€€€€€€€€€€€€€€±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±€
€±±±±±±± €€€                         ±±±±±±±±±±±±±€€€±±±±⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø±€
€±±±±±±± €€€±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±± €€€±±±±≥   Ronen Magid's  ≥±€
€±±±±±±± €€€±±±±±€€€€€€€€±±€€€€€€€±±€€€€€€€±±€€€± €€€±±±±≥                  ≥±€
€±±±±±±± €€€±±±± €€€  €€€± €€€   ±± €€€   ±± €€€± €€€±±±±≥      Fossil      ≥±€
€±±±±±±± €€€€€€± €€€± €€€± €€€±±±±± €€€±±±±± €€€± €€€±±±±≥      support     ≥±€
€±±±±±±± €€€  ±± €€€± €€€± €€€€€€€± €€€€€€€± €€€± €€€±±±±≥     unit for     ≥±€
€±±±±±±± €€€±±±± €€€± €€€±     €€€±     €€€± €€€± €€€±±±±≥                  ≥±€
€±±±±±±± €€€±±±± €€€± €€€±±±±  €€€±±±±  €€€± €€€± €€€±±±±≥   TURBO PASCAL   ≥±€
€±±±±±±± €€€±±±± €€€€€€€€±±€€€€€€€±±€€€€€€€± €€€± €€€±±±±≥     versions     ≥±€
€±±±±±±±   ±±±±±        ±±       ±±       ±±   ±±   ±±±±±≥       4,5        ≥±€
€±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ±€
€±±±€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€±±±±±±±±±±±±±±±±±±±±±±€
€±±                                                    ±±±±±±±±±±±±±±±±±±±±±±±€
€±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±€
ﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂ

          Copyright (c) 1989 by Ronen Magid. Tel (972)-52-917663 VOICE
                             972-52-27271 2400 24hrs


}

UNIT FOSCOMR;

INTERFACE

Uses Dos,CRT;

  VAR
   REGS: Registers;                    {Registers used by Intr and Ms-Dos}



{             Available user Procedures and Functions                     }

Procedure Fos_Init       (Comport: Byte);
Procedure Fos_Close      (Comport: Byte);
Procedure Fos_Parms      (Comport: Byte; Baud: Integer; DataBits: Byte;
                          Parity: Char; StopBit: Byte);
Procedure Fos_Dtr        (Comport: Byte; State: Boolean);
Procedure Fos_Flow       (Comport: Byte; State: Boolean);
Function  Fos_CD         (Comport: Byte) : Boolean;
Procedure Fos_Kill_Out   (Comport: Byte);
Procedure Fos_Kill_In    (Comport: Byte);
Procedure Fos_Flush      (Comport: Byte);
Function  Fos_Avail      (Comport: Byte) : Boolean;
Function  Fos_OkToSend   (Comport: Byte) : Boolean;
Function  Fos_Empty      (Comport: Byte) : Boolean;
Procedure Fos_Write      (Comport: Byte; Character: Char);
Procedure Fos_String     (Comport: Byte; OutString: String; Del : byte);
Procedure Fos_StringCRLF (Comport: Byte; OutString: String);
Procedure Fos_Ansi       (Character: Char);
Procedure Fos_Bios       (Character: Char);
Procedure Fos_WatchDog   (Comport: Byte; State: Boolean);
Function  Fos_Receive    (Comport: Byte) : Char;
function  Fos_Hangup     (Comport: Byte) : Boolean;
Procedure Fos_Reboot;
Function  Fos_CheckModem (Comport: Byte) : Boolean;
Function  Fos_AtCmd      (Comport: Byte; Command: String)  : Boolean;
Procedure Fos_Clear_Regs;


IMPLEMENTATION

Procedure Fos_Clear_Regs;
Begin
  Fillchar (Regs, SizeOf (Regs), 0);
end;






Procedure Fos_Init  (Comport: Byte);
Begin
 Fos_Clear_Regs;
 With Regs Do
 Begin
    AH := 4;
    DX := (ComPort-1);
    Intr ($14, Regs);
    If AX <> $1954 then
    begin
    writeln;
      Writeln (' Fossil driver is not loaded.');
      halt (1);
    end;
  end;
end;

Procedure Fos_Close (Comport: Byte);
Begin
  Fos_Clear_Regs;
{  Fos_Dtr(Comport,False);}

  With Regs Do
  Begin
    AH := 5;
    DX := (ComPort-1);
    Intr ($14, Regs);
  end;
end;


Procedure Fos_Parms (ComPort: Byte; Baud: Integer; DataBits: Byte; Parity: Char;
                     StopBit: Byte);
Var
 Code: Integer;

Begin
  Code:=0;
  Fos_Clear_Regs;
    Case Baud of
      0 : Exit;
    100 : code:=code+000+00+00;
    150 : code:=code+000+00+32;
    300 : code:=code+000+64+00;
    600 : code:=code+000+64+32;
    1200: code:=code+128+00+00;
    2400: code:=code+128+00+32;
    4800: code:=code+128+64+00;
    9600: code:=code+128+64+32;
  end;

  Case DataBits of
    5: code:=code+0+0;
    6: code:=code+0+1;
    7: code:=code+2+0;
    8: code:=code+2+1;
  end;

  Case Parity of
    'N','n': code:=code+00+0;
    'O','o': code:=code+00+8;
    'E','e': code:=code+16+8;
  end;

  Case StopBit of
    1: code := code + 0;
    2: code := code + 4;
  end;

  With Regs do
  begin
    AH := 0;
    AL := Code;
    DX := (ComPort-1);
    Intr ($14, Regs);
  end;
end;

Procedure Fos_Dtr   (Comport: Byte; State: Boolean);
Begin
  Fos_Clear_Regs;
  With Regs do
  begin
    AH := 6;
    DX := (ComPort-1);
    Case State of
    True : AL := 1;
    False: AL := 0;
    end;
    Intr ($14, Regs);
  end;
end;


Function  Fos_CD    (Comport: Byte) : Boolean;
Begin
  Fos_Clear_Regs;
  With Regs do
  Begin
    AH := 3;
    DX := (ComPort-1);
    Intr ($14, Regs);
    Fos_Cd := ((AL AND 128) = 128);
  end;
end;


Procedure Fos_Flow  (Comport: Byte; State: Boolean);
Begin
  Fos_Clear_Regs;
    With Regs do
    Begin
    AH := 15;
    DX := ComPort-1;
    Case State of
      TRUE:  AL := 255;
      FALSE: AL := 0;
    end;
    Intr ($14, Regs);
  end;
end;

Procedure Fos_Kill_Out (Comport: Byte);
Begin
  Fos_Clear_Regs;
    With Regs do
    begin
    AH := 9;
    DX := ComPort-1;
    Intr ($14, Regs);
  end;
end;


Procedure Fos_Kill_In  (Comport: Byte);
Begin
  Fos_Clear_Regs;
  With Regs do
  begin
    AH := 10;
    DX := ComPort-1;
    Intr ($14, Regs);
  end;
end;

Procedure Fos_Flush    (Comport: Byte);
Begin
  Fos_Clear_Regs;
  With Regs do
  Begin
    AH := 8;
    DX := ComPort-1;
    Intr ($14, Regs);
  end;
end;

Function  Fos_Avail    (Comport: Byte) : Boolean;
Begin
  Fos_Clear_Regs;
  With Regs do
  Begin
    AH := 3;
    DX := ComPort-1;
    Intr ($14, Regs);
    Fos_Avail:= ((AH AND 1) = 1);
  end;
end;

Function  Fos_OkToSend (Comport: Byte) : Boolean;
Begin
  Fos_Clear_Regs;
  With Regs do
  Begin
    AH := 3;
    DX := ComPort-1;
    Intr ($14, Regs);
    Fos_OkToSend := ((AH AND 32) = 32);
  end;
end;


Function  Fos_Empty (Comport: Byte) : Boolean;
Begin
  Fos_Clear_Regs;
  With Regs do
  Begin
    AH := 3;
    DX := ComPort-1;
    Intr ($14, Regs);
    Fos_Empty := ((AH AND 64) = 64);
  end;
end;

Procedure Fos_Write     (Comport: Byte; Character: Char);
Begin
  Fos_Clear_Regs;
  With Regs do
  Begin
    AH := 1;
    DX := ComPort-1;
    AL := ORD (Character);
    Intr ($14, Regs);
  end;
end;


Procedure Fos_String (Comport: Byte; OutString: String; Del : byte);
Var
 Pos: Byte;

begin
  For Pos := 1 to Length(OutString) do
  begin
    Fos_Write(Comport,OutString[Pos]);
    Delay (Del);
   end;
end;


Procedure Fos_StringCRLF  (Comport: Byte; OutString: String);
Var
 Pos: Byte;
begin
  For Pos := 1 to Length(OutString) do
  begin
     Fos_Write(ComPort,OutString[Pos]);
   end;
   Fos_Write(ComPort,Char(13));
   Fos_Write(ComPort,Char(10));
   OutString:='';
end;

Procedure Fos_Bios     (Character: Char);
 Begin
   Fos_Clear_Regs;
   With Regs do
   begin
     AH := 21;
     AL := ORD (Character);
     Intr ($14, Regs);
  end;
end;


Procedure Fos_Ansi     (Character: Char);
begin
  Fos_Clear_Regs;
  With Regs do
  Begin
    AH := 2;
    DL := ORD (Character);
    Intr ($21, Regs);
  end;
end;


Procedure Fos_WatchDog (Comport: Byte; State: Boolean);
Begin
  Fos_Clear_Regs;
  With Regs do
  Begin
    AH := 20;
    DX := ComPort-1;
    Case State of
      TRUE  : AL := 1;
      FALSE : AL := 0;
    end;
    Intr ($14, Regs);
  end;
end;


Function Fos_Receive  (Comport: Byte) : Char;
Begin
  Fos_Clear_Regs;
  With Regs do
  Begin
    AH := 2;
    DX := ComPort-1;
    Intr ($14, Regs);
    Fos_Receive := Chr(AL);
  end;
end;


Function Fos_Hangup   (Comport: Byte) : Boolean;
var
  Tcount : Integer;

begin
  Fos_Dtr(Comport,FALSE);
  delay (600);
  Fos_Dtr(Comport,TRUE);
  if FOS_CD (ComPort)=true then begin
    Tcount:=1;
      repeat
        Fos_String (Comport,'+++',0);
        delay (3000);
        Fos_StringCRLF (Comport,'ATH0');
        delay(3000);
        if Fos_CD (ComPort)=false then tcount:=3;
        Tcount:=Tcount+1;
      until Tcount=4;
  end;

  if Fos_Cd (ComPort)=true then Fos_Hangup:=False Else Fos_Hangup:=True;
end;


Procedure Fos_Reboot;
Begin
  Fos_Clear_Regs;
  With Regs do
  Begin
    AH := 23;
    AL := 1;
    Intr ($14, Regs);
  end;
end;

Function Fos_CheckModem (Comport: Byte) : Boolean;
Var
  Ch     :   Char;
  Result :   String[10];
  I,Z    :   Integer;

Begin
  Fos_CheckModem:=FALSE;
  Result:='';
  For Z:=1 to 3 do begin
    Delay(500);
    Fos_Write (Comport,Char(13));
    Delay(1000);
    Fos_StringCRLF (Comport,'AT');
    Delay(1000);
    Repeat
      If Fos_Avail (Comport) then Begin
        Ch:=Fos_Receive(Comport);
        Result:=Result+Ch;
      end;
    Until Fos_Avail(1)=FALSE;
    For I:=1 to Length(Result) do Begin
      If Copy(Result,I,2)='OK' then Begin
        Fos_CheckModem:=TRUE;
       Exit;
      end;
    end;
  end;
End;


Function Fos_AtCmd (Comport: Byte; Command: String) : Boolean;
Var
  Ch     :   Char;
  Result :   String[10];
  I,Z    :   Integer;

Begin
  Fos_AtCmd:=FALSE;
  Result:='';
  For Z:=1 to 3 do begin
    Delay(500);
    Fos_Write (Comport,Char(13));
    Delay(1000);
    Fos_StringCRLF (Comport,Command);
    Delay(1000);
    Repeat
      If Fos_Avail (Comport) then Begin
        Ch:=Fos_Receive(Comport);
        Result:=Result+Ch;
      end;
    Until Fos_Avail(1)=FALSE;
    For I:=1 to Length(Result) do Begin
      If Copy(Result,I,2)='OK' then Begin
        Fos_AtCmd:=TRUE;
       Exit;
      end;
    end;
  end;
End;

END.
