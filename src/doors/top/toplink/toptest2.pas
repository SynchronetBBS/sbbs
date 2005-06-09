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
  IDXRec = Record
             Alias : String[30];
             Name : String[42];
             Location : String[30];
             Buf : Array [1..407] of byte;
           end;

Var
  IDXFile : File of IDXRec;
  IDXBlock : IDXRec;

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
  Writeln (SizeOf (IDXBlock));
  FileMode:=fmReadWrite+FmDenyNone;
  Assign (IDXFile, 'd:\nodeidx.rap');
  Reset (IDXFile);
  Seek (IDXFile, 8);
  Read (IDXFile, IDXBlock);
  WriteCString (IDXBlock.Alias, 30);
  WriteCString (IDXBlock.Name, 30);
  WriteCString (IDXBlock.Location,30);
end.
