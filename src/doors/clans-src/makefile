# CC = tcc
# FLAGS = -ml -wall -C
# DELETE = del
# EXEFILE = .exe

OS	:=	$(shell uname | tr "[A-Z]" "[a-z]")
ifeq ($(OS),netbsd)
FLAGS	+=	-D__unix__
endif
# Somebody make this compile under gcc. :-)  I'll get you started:
CC ?= gcc
FLAGS += -O2 -Wall -std=c99 -pedantic -I../../odoors -L../../odoors/libs-`uname`
ifdef PROFILE
FLAGS	+=	-pg
endif
ifdef DEBUG
FLAGS	+=	-g
endif
# FLAGS = -O -Wall
DELETE = rm -f
EXEFILE = .${OS}
#
# See convert.txt for more suggestions.

OBJS = clans.o video.o system.o\
	parsing.o ibbs.o myopen.o language.o\
	door.o village.o input.o help.o game.o user.o\
	misc.o maint.o spells.o clansini.o class.o menus.o\
	crc.o items.o mail.o fight.o scores.o pawn.o reg.o\
	npc.o quests.o news.o empire.o alliance.o menus2.o\
	voting.o trades.o myibbs.o wb_fapnd.o

#all: clans.exe reset.exe pcedit.exe langcomp.exe mcomp.exe
all: clans$(EXEFILE) langcomp$(EXEFILE) mcomp$(EXEFILE) reset$(EXEFILE) pcedit$(EXEFILE)

clean:
	$(DELETE) *.o
	$(DELETE) clans$(EXEFILE)
	$(DELETE) langcomp$(EXEFILE)
	$(DELETE) mcomp$(EXEFILE)
	$(DELETE) reset$(EXEFILE)
	$(DELETE) pcedit$(EXEFILE)

clans$(EXEFILE):      $(OBJS) unix_wrappers.o
ifdef PROFILE
	$(CC) $(FLAGS) $(OBJS) -lODoors_p unix_wrappers.o -o clans$(EXEFILE)
else
	$(CC) $(FLAGS) $(OBJS) -lODoors unix_wrappers.o -o clans$(EXEFILE)
endif
#	$(CC) $(FLAGS) @comp.lst

unix_wrappers.o : unix_wrappers.c unix_wrappers.h
	$(CC) $(FLAGS) -c unix_wrappers.c

clans.o:      clans.c
	$(CC) $(FLAGS) -c clans.c

alliance.o: alliance.c alliance.h
	$(CC) $(FLAGS) -c alliance.c

tasker.o:     tasker.c
	$(CC) $(FLAGS) -c tasker.c

video.o:      video.c
	$(CC) $(FLAGS) -c video.c

system.o:     system.c
	$(CC) $(FLAGS) -c system.c

parsing.o:    parsing.c
	$(CC) $(FLAGS) -c parsing.c

ibbs.o:       ibbs.c
	$(CC) $(FLAGS) -c ibbs.c

# 12/23/2001 [au] dropped from use
#tslicer.o:    tslicer.c
#	$(CC) $(FLAGS) -c tslicer.c

myopen.o:     myopen.c
	$(CC) $(FLAGS) -c myopen.c

language.o:   language.c
	$(CC) $(FLAGS) -c language.c

door.o:       door.c
	$(CC) $(FLAGS) -c door.c

village.o:    village.c
	$(CC) $(FLAGS) -c village.c

input.o:      input.c
	$(CC) $(FLAGS) -c input.c

help.o:       help.c
	$(CC) $(FLAGS) -c help.c

game.o:       game.c
	$(CC) $(FLAGS) -c game.c

user.o:       user.c
	$(CC) $(FLAGS) -c user.c

misc.o:       misc.c
	$(CC) $(FLAGS) -c misc.c

maint.o:      maint.c
	$(CC) $(FLAGS) -c maint.c

spells.o:     spells.c
	$(CC) $(FLAGS) -c spells.c

clansini.o:   clansini.c
	$(CC) $(FLAGS) -c clansini.c

class.o:      class.c
	$(CC) $(FLAGS) -c class.c

menus.o:      menus.c
	$(CC) $(FLAGS) -c menus.c

crc.o:        crc.c
	$(CC) $(FLAGS) -c crc.c

items.o:      items.c
	$(CC) $(FLAGS) -c items.c

mail.o:       mail.c
	$(CC) $(FLAGS) -c mail.c

fight.o:      fight.c
	$(CC) $(FLAGS) -c fight.c

scores.o:     scores.c
	$(CC) $(FLAGS) -c scores.c

pawn.o:       pawn.c
	$(CC) $(FLAGS) -c pawn.c

reg.o:        reg.c
	$(CC) $(FLAGS) -c reg.c

npc.o:        npc.c
	$(CC) $(FLAGS) -c npc.c

quests.o:     quests.c
	$(CC) $(FLAGS) -c quests.c

news.o:       news.c
	$(CC) $(FLAGS) -c news.c

empire.o:     empire.c
	$(CC) $(FLAGS) -c empire.c

menus2.o:     menus2.c
	$(CC) $(FLAGS) -c menus2.c

voting.o:     voting.c
	$(CC) $(FLAGS) -c voting.c

trades.o:     trades.c
	$(CC) $(FLAGS) -c trades.c

myibbs.o:     myibbs.c
	$(CC) $(FLAGS) -c myibbs.c

wb_fapnd.o:     wb_fapnd.c
	$(CC) $(FLAGS) -c wb_fapnd.c

langcomp$(EXEFILE):      langcomp.c
	$(CC) $(FLAGS) langcomp.c -o langcomp$(EXEFILE)

reset$(EXEFILE): reset.c myopen.h parsing.h crc.h structs.h k_config.h unix_wrappers.o 
	$(CC) $(FLAGS) reset.c myopen.o parsing.o crc.o unix_wrappers.o -o reset$(EXEFILE) -lcurses


mcomp$(EXEFILE):      mcomp.c
	$(CC) $(FLAGS) mcomp.c -o mcomp$(EXEFILE)

pcedit$(EXEFILE):      pcedit.c myopen.o unix_wrappers.o myopen.h unix_wrappers.h
	$(CC) $(FLAGS) pcedit.c myopen.o unix_wrappers.o -o pcedit$(EXEFILE)

