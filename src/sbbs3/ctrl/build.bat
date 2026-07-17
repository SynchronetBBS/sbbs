@echo off
setlocal
call makelibs.bat
bpr2mak sbbsctrl.bpr
echo.
make %1 %2 %3 %4 %5 %6 -f sbbsctrl.mak

echo.
rem Mark sbbsctrl.exe /LARGEADDRESSAWARE (use up to ~4GB on 64-bit Windows) as a
rem stopgap for the GitLab #1185 JS-heap address-exhaustion crash. Borland's
rem ilink32 has no LAA option, so flip the PE-header bit post-build (toolchain-
rem agnostic; loader ignores the now-stale checksum for a normal user exe).
if exist sbbsctrl.exe (
	powershell -NoProfile -Command "$f='sbbsctrl.exe'; $b=[IO.File]::ReadAllBytes($f); $p=[BitConverter]::ToInt32($b,0x3C); $o=$p+4+18; $c=[BitConverter]::ToUInt16($b,$o); if(($c -band 0x20) -eq 0){ $n=$c -bor 0x20; $b[$o]=[byte]($n -band 0xFF); $b[$o+1]=[byte](($n -shr 8) -band 0xFF); [IO.File]::WriteAllBytes($f,$b); Write-Host ('  LARGEADDRESSAWARE set on '+$f) } else { Write-Host ('  '+$f+' already LARGEADDRESSAWARE') }"
) else (
	echo   WARNING: sbbsctrl.exe not found after build -- LAA bit NOT set.
)