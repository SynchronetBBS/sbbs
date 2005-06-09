Program RAPLink;

Const
   fmReadOnly  = $00;  (* *)
   fmWriteOnly = $01;  (* Only one of these should be used *)
   fmReadWrite = $02;  (* *)

   fmDenyAll   = $10;  (* together With only one of these  *)
   fmDenyWrite = $20;  (* *)
   fmDenyRead  = $30;  (* *)
   fmDenyNone  = $40;  (* *)

Type
  RAPRec = Record
             Buf : Array [1..8] of byte;
             Alias : String[30];
             Data : String[245];
           end;

Var
  RAPFile : File of RAPRec;
  RAPBlock : RAPRec;

Procedure WriteCString (var str; len : byte);
Var
  s : string absolute str;
  l : byte;
begin
  l := 0;
  While (s[l] <> #0) and (l <= len) do
  begin
    Write (s[l]);
    Inc (l);
  end;
  Writeln;
end;

Var
  L : byte;
  F : File of byte;

begin
  FileMode:=fmReadWrite+FmDenyNone;
  Writeln (SizeOf (RAPBlock));
  Assign (RAPFile, 'd:\node001.rap');
  Reset (RAPFile);
{  Read (RAPFile, RAPBlock);
  Close (RAPFIle);
  WriteCString (RAPBlock.Alias,28);
  For L := 1 to 5 do Write (Ord(RAPBlock.Data[L]):5);
  Writeln;
  WriteCString (RAPBlock.Data,255);}
  RAPBlock.Buf[1] := 25;
  RAPBlock.Buf[2] := 0;
  RAPBlock.Buf[3] := 0;
  RAPBlock.Buf[4] := 0;
  RAPBlock.Buf[5] := $00;
  RAPBlock.Buf[6] := $00;
  RAPBlock.Buf[7] := 0;
  RAPBlock.Buf[8] := 0;
  RAPBlock.Alias := 'APLink'#0;
  RAPBlock.Alias[0] := 'R';
  RAPBlock.Data := 'est message'#0;
  RAPBlock.Data[0] := 'T';
  Write (RAPFile, RAPBlock);
  Close (RAPFile);
  Assign (f, 'D:\MIDX001.RAP');
  Reset (f);
  L := 1;
  {$I-}
  Repeat
    Write (f,l);
  Until IOResult = 0;
  {$I+}
  Close (f);
  Assign (f, 'D:\CHGIDX.RAP');
  Reset (f);
  L := 1;
  {$I-}
  Repeat
    Write (f,l);
  Until IOResult = 0;
  {$I+}
  Close (f);
end.
