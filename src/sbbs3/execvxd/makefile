# makefile for SBBSEXEC.VXD

DEVICENAME = SBBSEXEC
FRAMEWORK = CPP
XFLAGS = -I..
OBJECTS = sbbsexec.obj ringbuf.obj
SOURCEINCPATH = ..
DEBUG = 0

DYNAMIC = 1

!include $(VTOOLSD)\include\vtoolsd.mak
!include $(VTOOLSD)\include\vxdtarg.mak

sbbsexec.obj:	sbbsexec.cpp makefile
ringbuf.obj:	..\ringbuf.c ..\ringbuf.h
