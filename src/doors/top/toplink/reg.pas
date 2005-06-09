Unit Reg;

Interface

Uses TopLSupp;

Function ValidateKey(VKStr, VKCode : String; Seed1, Seed2 : Word) : Boolean;
Function GenerateKey(GKStr : String; GKS1, GKS2 : Word) : LongInt;

Implementation

Function ValidateKey(VKStr, VKCode : String; Seed1, Seed2 : Word) : Boolean;
Var
  VKey : LongInt;
  C : Word;
Begin
  Val(VKCode, VKey, C);
  ValidateKey := (VKey = GenerateKey(VKStr, Seed1, Seed2));
End;

Function GenerateKey(GKStr : String; GKS1, GKS2 : Word) : LongInt;
Var
  NewKey, GTmp : LongInt;
  GKD, Target : Integer;
  GK1, GK2 : LongInt;
Begin
  GKStr := UPStr(GKStr);

  NewKey := LongInt(0);
  GKD := 1;
  GK1 := GKS1;
  GK2 := GKS2;

  If GKStr[Length(GKStr)] = #0 then Target := Length(GKStr) - 1 else Target := Length(GKStr);
  While NewKey = 0 do
  Begin
    While GKD <= Target do
    Begin
      GTmp := (Ord(GKStr[GKD]) * GK1) +
              (Ord(GKStr[GKD + 1]) * GK2);
      NewKey := NewKey + GTmp;
      Inc(GKD, 2);
    End;

    NewKey := (NewKey MOD GK1) * (NewKey MOD GK2);
    NewKey := NewKey * (GK1 + GK2);

    If NewKey = 0 then
    Begin
      Inc(GKS1, 3);
      Inc(GKS2, 4);
    End;
  End;
GenerateKey := NewKey;
End;

End.

