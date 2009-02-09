@echo=off
echo DOSFONT EXAMPLES
echo CHOOSE ONE OF EXAMPLES (REQUIRES VGA OR BETTER VIDEO ADAPTER)
echo 1 - LINEDRAW.FNT : Allows to draw menu lines with triple outline
echo 2 - HANDSCR.FNT  : Handscript font.
echo 3 - VOLTA.COM    : Restore default screen font.
echo ESC - Exit
choice /c:123 /n
if errorlevel 4 goto end
if errorlevel 3 goto volta
if errorlevel 2 goto handscr
if errorlevel 1 goto linedraw
:linedraw
fontsel linedraw /r /h16 /8 /a
pause
type moldura2.txt
goto end
:handscr
fontsel handscr /r /h16 /8 /a
goto end
:volta
volta
:end
