CC=	 gcc
FLAGS=	 -O -Wall
EXECFILE= 
RM=	 rm -f


all: chew$(EXEFILE) ecomp$(EXEFILE) install$(EXEFILE) langcomp$(EXEFILE) makenpc$(EXEFILE) makepak$(EXEFILE) mclass$(EXEFILE) mcomp$(EXEFILE) mitems$(EXEFILE) mspells$(EXEFILE)

chew$(EXEFILE) : chew.c
	$(CC) $(FLAGS) chew.c -o chew$(EXEFILE)

ecomp$(EXEFILE) : ecomp.c
	${CC} ${FLAGS} ecomp.c -o ecomp$(EXEFILE)

install$(EXEFILE) : install.c
	$(CC) $(FLAGS) install.c -o install$(EXEFILE) -lcurses

langcomp$(EXEFILE) : langcomp.c
	$(CC) $(FLAGS) langcomp.c -o langcomp$(EXEFILE)

makenpc$(EXEFILE) : makenpc.c
	$(CC) $(FLAGS) makenpc.c -o makenpc$(EXEFILE)

makepak$(EXEFILE) : makepak.c
	$(CC) $(FLAGS) makepak.c -o makepak$(EXEFILE)

mclass$(EXEFILE) : mclass.c
	$(CC) $(FLAGS) mclass.c -o mclass$(EXEFILE)

mcomp$(EXEFILE) : mcomp.c
	$(CC) $(FLAGS) mcomp.c -o mcomp$(EXEFILE)

mitems$(EXEFILE) : mitems.c
	$(CC) $(FLAGS) mitems.c -o mitems$(EXEFILE)

mspells$(EXEFILE) : mspells.c
	$(CC) $(FLAGS) mspells.c -o mspells$(EXEFILE)

clean :
	$(RM) chew$(EXEFILE)
	$(RM) ecomp$(EXEFILE)
	$(RM) install$(EXEFILE)
	$(RM) langcomp$(EXEFILE)
	$(RM) makenpc$(EXEFILE)
	$(RM) makepak$(EXEFILE)
	$(RM) mclass$(EXEFILE)
	$(RM) mcomp$(EXEFILE)
	$(RM) mitems$(EXEFILE)
	$(RM) mspells$(EXEFILE)	
