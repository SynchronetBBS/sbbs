@echo off
rem make sure LIB environment variable is set!
cl /I\msvc15\include /I.. /I\ntddk\inc /I..\..\xpdev /AS /Fc /Fm /Gs dosxtrn.c