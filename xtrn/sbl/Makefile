##################################################################
# Makefile for SBL (Synchronet BBS List Online External Program) #
# For use with Borland C++ or C++ Builder (for Win32)  		 #
# Tabstop=8				 	  		 #
##################################################################

# Macros
CC		=	bcc32
LD		=	ilink32
XSDK	= ..\sdk
!ifndef XPDEV
 XPDEV		=	../../src/xpdev
!endif
!ifndef SBBS_SRC
 SBBS_SRC	=	../../src/sbbs3
!endif
CFLAGS	= -WM -I$(XSDK);$(XPDEV);$(SBBS_SRC)
CFLAGS	= $(CFLAGS) -q -d -H -X- -w-csu -w-pch -w-ccc -w-rch -w-par -w-8004
OBJS    = xsdk.obj xsdkvars.obj xsdkwrap.obj

.path.c = .;$(XSDK);$(XPDEV);$(SBBS_SRC)

# Enable auto-dependency checking
.autodepend
.cacheautodepend	

# Implicit C Compile Rule
.c.obj:
	@$(CC) $(CFLAGS) -c $<

all: sbl.exe smb2sbl.exe sbl2smb.exe

# Main EXE Link Rule
sbl.exe: $(OBJS) sbl.obj
    	@echo Linking $@
	@$(CC) $(CFLAGS) -e$@ $**

SMBIO	= genwrap.obj filewrap.obj dirwrap.obj smblib.obj lzh.obj

sbl2smb.exe: sbl2smb.obj $(SMBIO)
	@echo Linking $@
	@$(CC) $(CFLAGS) -e$@ $**

smb2sbl.exe: smb2sbl.obj $(SMBIO)
	@echo Linking $@
	@$(CC) $(CFLAGS) -e$@ $**

clean:
	del $(OBJS) $(SMBIO)