##################################################################
# Makefile for SBL (Synchronet BBS List Online External Program) #
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

# Main EXE Link Rule
sbl.exe: $(OBJS) sbl.obj
    	@echo Linking $@
	@$(CC) $(CFLAGS) -e$@ $**

clean:
	del $(OBJS)