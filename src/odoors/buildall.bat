make -fDOS.mak -DTARGET=s > out.txt
make -fDOS.mak -DTARGET=c >> out.txt
make -fDOS.mak -DTARGET=m >> out.txt
make -fDOS.mak -DTARGET=l >> out.txt
make -fDOS.mak -DTARGET=h >> out.txt
set INCLUDE=c:\msdev\include;c:\msdev\mfc\include
nmake -fWin32.mak >> out.txt
