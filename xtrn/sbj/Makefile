##################################################################
# Makefile for SBJ (Synchronet Blackjack External Program) 	 #
# For use with Borland C++ or C++ Builder (for Win32)  		 #
# Tabstop=8				 	  		 #
##################################################################

# Macros
CC		=	bcc32
LD		=	ilink32
XSDK	= ..\sdk
CFLAGS	= -WM -I$(XSDK)
CFLAGS	= $(CFLAGS) -q -d -H -X- -w-csu -w-pch -w-ccc -w-rch -w-par -w-8004
OBJS    = xsdk.obj xsdkvars.obj xsdkwrap.obj

.path.c = .;$(XSDK)

# Enable auto-dependency checking
.autodepend
.cacheautodepend	

# Implicit C Compile Rule
.c.obj:
	@$(CC) $(CFLAGS) -c $<

all: sbj.exe sbjclean.exe

# Main EXE Link Rule
sbj.exe: $(OBJS) sbj.obj
    	@echo Linking $@
	@$(CC) $(CFLAGS) -e$@ $**

# Main EXE Link Rule
sbjclean.exe: $(OBJS) sbjclean.obj
    	@echo Linking $@
	@$(CC) $(CFLAGS) -e$@ $**

clean:
	del $(OBJS)