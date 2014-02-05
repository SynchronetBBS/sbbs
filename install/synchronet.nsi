!include "winmessages.nsh"
!include "ZipDLL.nsh"
!define env_hklm 'HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"'
!define env_hkcu 'HKCU "Environment"'

Name "Synchronet Nightly"

OutFile "synchronet-install.exe"
InstallDir '$PROGRAMFILES\Synchronet'
ShowInstDetails show

RequestExecutionLevel highest

Page directory
Page instfiles

Function RollBack
	DetailPrint "Installation failed.  Cleaning up."
	SetOutPath $TEMP
	RMDir /r $INSTDIR
	Abort
FunctionEnd

Function CVSCheckout
	pop $0
	DetailPrint "-Checking '$0' out of CVS ..."
	nsExec::Exec '"$INSTDIR\temp\cvs.exe" -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout $0'
	IfFileExists '$INSTDIR\$0' PathGood
		DetailPrint "CVS checkout of '$0' failed.  Aborting installation."
		Call RollBack
	PathGood:
FunctionEnd

Section ""

	IfFileExists '$INSTDIR' ask true
	ask:
		MessageBox MB_YESNO '$INSTDIR exists. Files will be overwritten. Proceed?' IDYES true
		DetailPrint "Installation aborted."
		Quit
	true:
	
	IfFileExists $SYSDIR\MSVCR100.dll ok_msvcr100 install_msvcr100
	install_msvcr100:
		DetailPrint 'Installing MSVCR100.dll to $SYSDIR:'
		SetOutPath $SYSDIR
		File MSVCR100.dll
	ok_msvcr100:

	SetOutPath $INSTDIR\temp
	File cvs.exe

	SetOutPath $INSTDIR
	
	DetailPrint "Downloading files.  Be patient ..."
	push 'ctrl'
	Call CVSCheckout
	push 'docs'
	Call CVSCheckout
	push 'exec'
	Call CVSCheckout
	push 'node1'
	Call CVSCheckout
	push 'node2'
	Call CVSCheckout
	push 'node3'
	Call CVSCheckout
	push 'node4'
	Call CVSCheckout
	push 'text'
	Call CVSCheckout
	push 'web'
	Call CVSCheckout
	push 'xtrn'
	Call CVSCheckout

	DetailPrint "-Fetching nightly build from ftp.synchro.net ..."
	inetc::get ftp://ftp.synchro.net/sbbs_dev.zip '$INSTDIR\sbbs_dev.zip'
	IfFileExists '$INSTDIR\sbbs_dev.zip' FileGood
		DetailPrint "File sbbs_dev.zip wasn't downloaded.  Aborting."
		Call RollBack
	FileGood:

	DetailPrint "Extracting executables into '$INSTDIR\exec' ..."
	!insertmacro ZIPDLL_EXTRACT "$INSTDIR\sbbs_dev.zip" "$INSTDIR\exec" "<ALL>"
	Pop $0
	StrCmp $0 "success" UnzipGood
		DetailPrint "Failed to unzip sbbs_dev.zip.  Aborting."
		Call RollBack
	UnzipGood:
	Delete '$INSTDIR\sbbs_dev.zip'

	DetailPrint "Setting SBBSCTRL environment variable to '$INSTDIR\ctrl' ..."
	WriteRegExpandStr ${env_hklm} SBBSCTRL '$INSTDIR\ctrl'
	SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

	DetailPrint "Copying 'sbbs.dll' to '$SYSDIR' ..."
	CopyFiles '$INSTDIR\exec\sbbs.dll' $SYSDIR

	DetailPrint "Compiling BAJA scripts ..."
	SetOutPath $INSTDIR\exec
	FindFirst $0 $1 $INSTDIR\exec\*.src
	baja_compile_loop:
		StrCmp $1 "" done_baja_compile_loop
		DetailPrint "  -Compiling $1"
		nsExec::Exec '"$INSTDIR\exec\baja.exe" $1'
		FindNext $0 $1
		Goto baja_compile_loop
	done_baja_compile_loop:
	FindClose $0
	
	DetailPrint "Cleaning up ..."
	RMDir /r '$INSTDIR\temp'

	CreateShortCut '$SMPROGRAMS\Synchronet Control Panel.lnk' '$INSTDIR\exec\sbbsctrl.exe'
	
SectionEnd