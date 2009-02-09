call compile s %1 %2 %3 %4 %5 %6 %7 %8 %9
call compile m %1 %2 %3 %4 %5 %6 %7 %8 %9
call compile c %1 %2 %3 %4 %5 %6 %7 %8 %9
call compile l %1 %2 %3 %4 %5 %6 %7 %8 %9
call compile h %1 %2 %3 %4 %5 %6 %7 %8 %9
@echo ÿ
@echo If you wish to create libraries for 186 or 286 Protected Mode Instructions
@echo use the lines below
@echo tcc -c -1 [options] *.c
@echo tlib video186 @videolib.dat
@echo tcc -c -2 [options] *.c
@echo tlib video286 @videolib.dat