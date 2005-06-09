Program MulTest;
{
 Copyright (c) 1992 ABSoft - All Rights Reserved

 1.00
 1.01  Minor Changes
 1.10  Updated for new MulAware
 2.00  Complete Rewrite
 2.10  Updated for new MulAware
 2.20  Updated for new MulAware
 3.00  Updated for TP 7.0 & BP 7.0
       To Compile in Protected Mode:
       BPC /CP /DPMODE MULTEST
}

{$A-,B-,D-,E-,F-,G-,I-,L-,N-,O-,R-,S-,V-,X-}

{$IFDEF VER70}
  {$P-,Q-,T-,Y-}
{$ENDIF}

{$IFDEF PMODE}
  {$G+}
{$ENDIF}

{$M 1024,0,0}

Uses
  MulAware;

Const
{$IFDEF PMODE}
  Version = '3.00p';
{$ELSE}
  Version = '3.00';
{$ENDIF}

Begin
  WriteLn;
  WriteLn('MulTest ', Version, ' Copyright (c) 1992 ABSoft');
  WriteLn;
  Case MultiTasker of
    None         : Write('No Supported MultiTasker');
    DESQview     : Write('DESQview v', Hi(MulVersion), '.', Lo(MulVersion));
    WinEnh       : Write('Windows v3.', Lo(MulVersion), ' in Enhanced Mode');
    OS2          : Write('OS/2 v', Hi(MulVersion), '.', Lo(MulVersion));
    DoubleDOS    : Write('DoubleDOS');
    MultiDos     : Write('MultiDos Plus');
    VMiX         : Write('VMiX v', Hi(MulVersion), '.', Lo(MulVersion));
    TopView      : begin
                     If MulVersion <> 0 then
                        Write('TopView v', Hi(MulVersion), '.', Lo(MulVersion))
                     Else
                        Write('TaskView, DESQview 2.00-2.25, OmniView, or Compatible');
                   end;
    TaskSwitcher : Write('DOS 5.0 Task Switcher or Compatible');
    WinStandard  : Write('Windows 2.xx or 3.x in Real or Standard Mode');
    WinNT        : Write('Windows NT');
  end;
  WriteLn(' Detected');
  WriteLn;
  TimeSlice;
  Halt(Ord(MultiTasker));
End.
