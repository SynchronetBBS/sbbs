project (SBBSCtrl C CXX)

cmake_minimum_required(VERSION 2.8)

macro(makelib name)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}//${name}.lib
		COMMAND implib ${CMAKE_CURRENT_SOURCE_DIR}/${name}.lib ${SBBS3_BINARY_DIR}/${name}.dll
	)
	add_custom_target(${name}_OMF_lib ALL)
	add_dependencies(${name}_OMF_lib ${name})
endmacro()

if(WIN32)
	file(TO_NATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR} SOURCE_DIR)
	file(TO_NATIVE_PATH ${CMAKE_CURRENT_BINARY_DIR} BINARY_DIR)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/sbbsctrl.exe
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build.bat
		MAIN_DEPENDENCY sbbsctrl.cpp
		DEPENDS 
			AboutBoxFormUnit.cpp
			AboutBoxFormUnit.dfm
			AboutBoxFormUnit.h
			ClientFormUnit.cpp
			ClientFormUnit.dfm
			ClientFormUnit.h
			CodeInputFormUnit.cpp
			CodeInputFormUnit.dfm
			CodeInputFormUnit.h
			ConfigWizardUnit.cpp
			ConfigWizardUnit.dfm
			ConfigWizardUnit.h
			CtrlPathDialogUnit.cpp
			CtrlPathDialogUnit.dfm
			CtrlPathDialogUnit.h
			emulvt.hpp
			emulvt.pas
			EventsFormUnit.cpp
			EventsFormUnit.dfm
			EventsFormUnit.h
			FtpCfgDlgUnit.cpp
			FtpCfgDlgUnit.dfm
			FtpCfgDlgUnit.h
			FtpFormUnit.cpp
			FtpFormUnit.dfm
			FtpFormUnit.h
			LoginAttemptsFormUnit.cpp
			LoginAttemptsFormUnit.dfm
			LoginAttemptsFormUnit.h
			MailCfgDlgUnit.cpp
			MailCfgDlgUnit.dfm
			MailCfgDlgUnit.h
			MailFormUnit.cpp
			MailFormUnit.dfm
			MailFormUnit.h
			MainFormUnit.cpp
			MainFormUnit.dfm
			MainFormUnit.h
			NodeFormUnit.cpp
			NodeFormUnit.dfm
			NodeFormUnit.h
			PreviewFormUnit.cpp
			PreviewFormUnit.dfm
			PreviewFormUnit.h
			PropertiesDlgUnit.cpp
			PropertiesDlgUnit.dfm
			PropertiesDlgUnit.h
			sbbsctrl.bpr
			sbbsctrl.cpp
			sbbsctrl.res
			ServicesCfgDlgUnit.cpp
			ServicesCfgDlgUnit.dfm
			ServicesCfgDlgUnit.h
			ServicesFormUnit.cpp
			ServicesFormUnit.dfm
			ServicesFormUnit.h
			SpyFormUnit.cpp
			SpyFormUnit.dfm
			SpyFormUnit.h
			StatsFormUnit.cpp
			StatsFormUnit.dfm
			StatsFormUnit.h
			StatsLogFormUnit.cpp
			StatsLogFormUnit.dfm
			StatsLogFormUnit.h
			TelnetCfgDlgUnit.cpp
			TelnetCfgDlgUnit.dfm
			TelnetCfgDlgUnit.h
			TelnetFormUnit.cpp
			TelnetFormUnit.dfm
			TelnetFormUnit.h
			TextFileEditUnit.cpp
			TextFileEditUnit.dfm
			TextFileEditUnit.h
			UserListFormUnit.cpp
			UserListFormUnit.dfm
			UserListFormUnit.h
			UserMsgFormUnit.cpp
			UserMsgFormUnit.dfm
			UserMsgFormUnit.h
			WebCfgDlgUnit.cpp
			WebCfgDlgUnit.dfm
			WebCfgDlgUnit.h
			WebFormUnit.cpp
			WebFormUnit.dfm
			WebFormUnit.h
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
	add_custom_target(sbbsctrl ALL
		COMMAND if not exist \"${BINARY_DIR}\" mkdir \"${BINARY_DIR}\"
		COMMAND copy \"${SOURCE_DIR}\\sbbsctrl.exe\" \"${BINARY_DIR}\\\"
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sbbsctrl.exe
		SOURCES 
			AboutBoxFormUnit.cpp
			AboutBoxFormUnit.dfm
			AboutBoxFormUnit.h
			ClientFormUnit.cpp
			ClientFormUnit.dfm
			ClientFormUnit.h
			CodeInputFormUnit.cpp
			CodeInputFormUnit.dfm
			CodeInputFormUnit.h
			ConfigWizardUnit.cpp
			ConfigWizardUnit.dfm
			ConfigWizardUnit.h
			CtrlPathDialogUnit.cpp
			CtrlPathDialogUnit.dfm
			CtrlPathDialogUnit.h
			emulvt.hpp
			emulvt.pas
			EventsFormUnit.cpp
			EventsFormUnit.dfm
			EventsFormUnit.h
			FtpCfgDlgUnit.cpp
			FtpCfgDlgUnit.dfm
			FtpCfgDlgUnit.h
			FtpFormUnit.cpp
			FtpFormUnit.dfm
			FtpFormUnit.h
			LoginAttemptsFormUnit.cpp
			LoginAttemptsFormUnit.dfm
			LoginAttemptsFormUnit.h
			MailCfgDlgUnit.cpp
			MailCfgDlgUnit.dfm
			MailCfgDlgUnit.h
			MailFormUnit.cpp
			MailFormUnit.dfm
			MailFormUnit.h
			MainFormUnit.cpp
			MainFormUnit.dfm
			MainFormUnit.h
			NodeFormUnit.cpp
			NodeFormUnit.dfm
			NodeFormUnit.h
			PreviewFormUnit.cpp
			PreviewFormUnit.dfm
			PreviewFormUnit.h
			PropertiesDlgUnit.cpp
			PropertiesDlgUnit.dfm
			PropertiesDlgUnit.h
			sbbsctrl.bpr
			sbbsctrl.cpp
			sbbsctrl.res
			ServicesCfgDlgUnit.cpp
			ServicesCfgDlgUnit.dfm
			ServicesCfgDlgUnit.h
			ServicesFormUnit.cpp
			ServicesFormUnit.dfm
			ServicesFormUnit.h
			SpyFormUnit.cpp
			SpyFormUnit.dfm
			SpyFormUnit.h
			StatsFormUnit.cpp
			StatsFormUnit.dfm
			StatsFormUnit.h
			StatsLogFormUnit.cpp
			StatsLogFormUnit.dfm
			StatsLogFormUnit.h
			TelnetCfgDlgUnit.cpp
			TelnetCfgDlgUnit.dfm
			TelnetCfgDlgUnit.h
			TelnetFormUnit.cpp
			TelnetFormUnit.dfm
			TelnetFormUnit.h
			TextFileEditUnit.cpp
			TextFileEditUnit.dfm
			TextFileEditUnit.h
			UserListFormUnit.cpp
			UserListFormUnit.dfm
			UserListFormUnit.h
			UserMsgFormUnit.cpp
			UserMsgFormUnit.dfm
			UserMsgFormUnit.h
			WebCfgDlgUnit.cpp
			WebCfgDlgUnit.dfm
			WebCfgDlgUnit.h
			WebFormUnit.cpp
			WebFormUnit.dfm
			WebFormUnit.h
	)
	makelib(sbbs)
	makelib(mailsrvr)
	makelib(ftpsrvr)
	makelib(websrvr)
	makelib(services)
	add_dependencies(sbbsctrl sbbs_OMF_lib mailsrvr_OMF_lib ftpsrvr_OMF_lib websrvr_OMF_lib services_OMF_lib)

	install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/sbbsctrl.exe DESTINATION bin)
endif()
