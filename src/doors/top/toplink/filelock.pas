{
 >MM> Is there a way to LOCK specific records or areas in a binary file so
 >MM> that one program can access the 1st byte of file and another program
 >MM> access the 2nd byte of the program at the same time?

 TC> Here's something from my own tool
 TC> box of tricks:
}
function FLock(Lock:byte; Handle: Word; Pos,Len: LongInt): Word; Assembler;
  ASM
      mov   AL,Lock   { subfunction 0: lock region   }
                      { subfunction 1: unlock region }
      mov   AH,$5C    { DOS function $5C: FLOCK    }
      mov   BX,Handle { put FileHandle in BX       }
      les   DX,Pos
      mov   CX,ES     { CX:DX begin position       }
      les   DI,Len
      mov   SI,ES     { SI:DI length lockarea      }
      int   $21       { Call DOS ...               }
      jb    @End      { if error then return AX    }
      xor   AX,AX     { else return 0              }
  @End:
end {FLock};

Comes in handy when descending TDosStream.

TLockStream = object(TDosStream)
                procedure write(var buf;count:word); virtual;
              end;
Procedure TLockStream.write(var buf;count:word);
   var isLocked : integer;
       curpos   : longint;   
   Begin
     curpos := getpos;
     isLocked := Flock(0,handle,curpos,count);
     if   isLocked < 2
     then begin
            inherited write(buf,count);
            if   isLocked = 0
            then Flock(1,handle,curpos,count);
          end
     else status := isLocked;
  End;
