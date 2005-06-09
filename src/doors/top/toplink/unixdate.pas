
(***************************************************************************)
(* UNIX DATE Version 1.01                                                  *)
(* This unit provides access to UNIX date related functions and procedures *)
(* A UNIX date is the number of seconds from January 1, 1970. This unit    *)
(* may be freely used. If you modify the source code, please do not        *)
(* distribute your enhancements.                                           *)
(* (C) 1991-1993 by Brian Stark.                                           *)
(* This is a programming release from Digital Illusions                    *)
(* FidoNet 1:289/27.2 + Columbia, MO - USA                                 *)
(* Revision History                                                        *)
(* ----------------------------------------------------------------------- *)
(* 06-13-1993 1.02 | Minor code cleanup                                    *)
(* 05-23-1993 1.01 | Added a few more routines for use with ViSiON BBS     *)
(* ??-??-1991 1.00 | First release                                         *)
(* ----------------------------------------------------------------------- *)
(***************************************************************************)

Unit UnixDate;

INTERFACE

Uses
   DOS;

Function  GetTimeZone : ShortInt;
  {Returns the value from the enviroment variable "TZ". If not found, UTC is
   assumed, and a value of zero is returned}
Function  IsLeapYear(Source : Word) : Boolean;
  {Determines if the year is a leap year or not}
Function  Norm2Unix(Y, M, D, H, Min, S : Word) : LongInt;
  {Convert a normal date to its UNIX date. If environment variable "TZ" is
   defined, then the input parameters are assumed to be in **LOCAL TIME**}
Procedure Unix2Norm(Date : LongInt; Var Y, M, D, H, Min, S : Word);
  {Convert a UNIX date to its normal date counterpart. If the environment
   variable "TZ" is defined, then the output will be in **LOCAL TIME**}

Function  TodayInUnix : LongInt;
  {Gets today's date, and calls Norm2Unix}
{
 Following returns a string and requires the TechnoJock totSTR unit.
Function  Unix2Str(N : LongInt) : String;
}
Const
  DaysPerMonth :
    Array[1..12] of ShortInt = (031,028,031,030,031,030,031,031,030,031,030,031);
  DaysPerYear  :
    Array[1..12] of Integer  = (031,059,090,120,151,181,212,243,273,304,334,365);
  DaysPerLeapYear :
    Array[1..12] of Integer  = (031,060,091,121,152,182,213,244,274,305,335,366);
  SecsPerYear      : LongInt  = 31536000;
  SecsPerLeapYear  : LongInt  = 31622400;
  SecsPerDay       : LongInt  = 86400;
  SecsPerHour      : Integer  = 3600;
  SecsPerMinute    : ShortInt = 60;

IMPLEMENTATION

Function GetTimeZone : ShortInt;
Var
  Environment : String;
  Index : Integer;
Begin
  GetTimeZone := 0;                            {Assume UTC}
  Environment := GetEnv('TZ');       {Grab TZ string}
  For Index := 1 To Length(Environment) Do
    Environment[Index] := Upcase(Environment[Index]);
(*
  NOTE: I have yet to find a complete list of the ISO table of time zone
        abbreviations. The following is excerpted from the Opus-Cbcs
        documentation files.
*)
  If Environment =  'EST05'    Then GetTimeZone := -05; {USA EASTERN}
  If Environment =  'EST05EDT' Then GetTimeZone := -06;
  If Environment =  'CST06'    Then GetTimeZone := -06; {USA CENTRAL}
  If Environment =  'CST06CDT' Then GetTimeZone := -07;
  If Environment =  'MST07'    Then GetTimeZone := -07; {USA MOUNTAIN}
  If Environment =  'MST07MDT' Then GetTimeZone := -08;
  If Environment =  'PST08'    Then GetTimeZone := -08;
  If Environment =  'PST08PDT' Then GetTimeZone := -09;
  If Environment =  'YST09'    Then GetTimeZone := -09;
  If Environment =  'AST10'    Then GetTimeZone := -10;
  If Environment =  'BST11'    Then GetTimeZone := -11;
  If Environment =  'CET-1'    Then GetTimeZone :=  01;
  If Environment =  'CET-01'   Then GetTimeZone :=  01;
  If Environment =  'EST-10'   Then GetTimeZone :=  10;
  If Environment =  'WST-8'    Then GetTimeZone :=  08; {Perth, Western Austrailia}
  If Environment =  'WST-08'   Then GetTimeZone :=  08;
End;

Function IsLeapYear(Source : Word) : Boolean;
Begin
(*
  NOTE: This is wrong!
*)
  If (Source Mod 4 = 0) Then
    IsLeapYear := True
  Else
    IsLeapYear := False;
End;

Function Norm2Unix(Y,M,D,H,Min,S : Word) : LongInt;
Var
  UnixDate : LongInt;
  Index    : Word;
Begin
  UnixDate := 0;                                                 {initialize}
  Inc(UnixDate,S);                                              {add seconds}
  Inc(UnixDate,(SecsPerMinute * Min));                          {add minutes}
  Inc(UnixDate,(SecsPerHour * H));                                {add hours}
  (*************************************************************************)
  (* If UTC = 0, and local time is -06 hours of UTC, then                  *)
  (* UTC := UTC - (-06 * SecsPerHour)                                      *)
  (* Remember that a negative # minus a negative # yields a positive value *)
  (*************************************************************************)
  UnixDate := UnixDate - (GetTimeZone * SecsPerHour);            {UTC offset}

  If D > 1 Then                                 {has one day already passed?}
    Inc(UnixDate,(SecsPerDay * (D-1)));

  If IsLeapYear(Y) Then
    DaysPerMonth[02] := 29
  Else
    DaysPerMonth[02] := 28;                             {Check for Feb. 29th}

  Index := 1;
  If M > 1 Then For Index := 1 To (M-1) Do    {has one month already passed?}
    Inc(UnixDate,(DaysPerMonth[Index] * SecsPerDay));

  While Y > 1970 Do
  Begin
    If IsLeapYear((Y-1)) Then
      Inc(UnixDate,SecsPerLeapYear)
    Else
      Inc(UnixDate,SecsPerYear);
    Dec(Y,1);
  End;

  Norm2Unix := UnixDate;
End;

Procedure Unix2Norm(Date : LongInt; Var Y, M, D, H, Min, S : Word);
{}
Var
  LocalDate : LongInt;
  Done      : Boolean;
  X         : ShortInt;
  TotDays   : Integer;
Begin
  Y   := 1970;
  M   := 1;
  D   := 1;
  H   := 0;
  Min := 0;
  S   := 0;
  LocalDate := Date + (GetTimeZone * SecsPerHour);         {Local time date}
 (*************************************************************************)
 (* Sweep out the years...                                                *)
 (*************************************************************************)
  Done := False;
  While Not Done Do
  Begin
    If LocalDate >= SecsPerYear Then
    Begin
      Inc(Y,1);
      Dec(LocalDate,SecsPerYear);
    End
    Else
      Done := True;

    If (IsLeapYear(Y+1)) And (LocalDate >= SecsPerLeapYear) And
       (Not Done) Then
    Begin
      Inc(Y,1);
      Dec(LocalDate,SecsPerLeapYear);
    End;
  End;
  (*************************************************************************)
  M := 1;
  D := 1;
  Done := False;
  TotDays := LocalDate Div SecsPerDay;
  If IsLeapYear(Y) Then
  Begin
    DaysPerMonth[02] := 29;
    X := 1;
    Repeat
      If (TotDays <= DaysPerLeapYear[x]) Then
      Begin
        M := X;
        Done := True;
        Dec(LocalDate,(TotDays * SecsPerDay));
        D := DaysPerMonth[M]-(DaysPerLeapYear[M]-TotDays) + 1;
      End
      Else
        Done := False;
      Inc(X);
    Until (Done) or (X > 12);
  End
  Else
  Begin
    DaysPerMonth[02] := 28;
    X := 1;
    Repeat
      If (TotDays <= DaysPerYear[x]) Then
      Begin
        M := X;
        Done := True;
        Dec(LocalDate,(TotDays * SecsPerDay));
        D := DaysPerMonth[M]-(DaysPerYear[M]-TotDays) + 1;
      End
      Else
        Done := False;
      Inc(X);
    Until Done = True or (X > 12);
  End;
  H := LocalDate Div SecsPerHour;
    Dec(LocalDate,(H * SecsPerHour));
  Min := LocalDate Div SecsPerMinute;
    Dec(LocalDate,(Min * SecsPerMinute));
  S := LocalDate;
End;

Function  TodayInUnix : LongInt;
Var
  Year, Month, Day, DayOfWeek: Word;
  Hour, Minute, Second, Sec100: Word;
Begin
  GetDate(Year, Month, Day, DayOfWeek);
  GetTime(Hour, Minute, Second, Sec100);
  TodayInUnix := Norm2Unix(Year,Month,Day,Hour,Minute,Second);
End;

{Function  Unix2Str(N : LongInt) : String;
Var
  Year, Month, Day, DayOfWeek  : Word;
  Hour, Minute, Second, Sec100 : Word;
  T : String;
Begin
  Unix2Norm(N, Year, Month, Day, Hour, Minute, Second);
  T := PadRight(IntToStr(Month),2,'0')+'-'+PadRight(IntToStr(Day),2,'0')+'-'+
       PadRight(IntToStr(Year),2,'0')+' '+ PadRight(IntToStr(Hour),2,'0')+':'+
       PadRight(IntToStr(Minute),2,'0')+':'+PadRight(IntToStr(Second),2,'0');
  If Hour > 12 Then
    T := T + ' PM'
  Else
    T := T + ' AM';
  Unix2Str := T;
End;}


END.
