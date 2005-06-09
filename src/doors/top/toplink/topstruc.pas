Type
  IDXRec = Record
             Alias : String[30];
             Name : String[42];
             Location : String[30];
             Buf : Array [1..407] of byte;
           end;

Type
  TOPRec = Record
             Buf : Array [1..8] of byte;
             Alias : String[30];
             Data : String[255];
           end;
           Buf[5,6] = Integer = Who action is done to -1;

1 = Normal MSG
2 = User Entry
3 = User Exit!
4 = Profile Editor
5 = Return from PE
6 = whisper
7 = GA
8 = GA's
9 = Yellow MSG
13 = Secret MSG
14 = Handle change
15 = "<msg>"
16 = SC
18 = Personal actions updated.
19 = Tossed out of pub
22 = Transformed into a link
23 = <name>: <msg>
24 = Link turned off.
25 = Entry/Exit messages updated.
