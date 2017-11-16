!define VERSION "$%APPVEYOR_BUILD_VERSION%"

!include "MUI.nsh"
!include "FileFunc.nsh"

Name "anno"
OutFile "anno-installer-v${VERSION}.exe"

InstallDir "$PROGRAMFILES64\anno"

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_CHECKED
!define MUI_FINISHPAGE_RUN_TEXT "Launch anno"
!define MUI_FINISHPAGE_RUN_FUNCTION "Launch"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

# From: https://stackoverflow.com/a/28335075/19254
!ifndef QTDIR
!define QTDIR "$%QTDIR%"
!endif
!if ! /fileexists "${QTDIR}"
!error "QTDIR not valid"
!endif

Section "Main Section" MainSec

	CreateDirectory $INSTDIR
	CreateDirectory $INSTDIR\bin
	WriteUninstaller $INSTDIR\anno-uninstaller.exe

	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\anno" "DisplayName" "anno"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\anno" "UninstallString" "$\"$INSTDIR\anno-uninstaller.exe$\""
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\anno" "Publisher" "Tomaattinen Ltd (Juha Reunanen)"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\anno" "DisplayVersion" "${VERSION}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\anno" "DisplayIcon" "$INSTDIR\bin\anno.exe,0"

	DetailPrint "Copying files..."

	SetOutPath $INSTDIR\bin
	File release\anno.exe
	File ${QTDIR}\bin\Qt5Core.dll
	File ${QTDIR}\bin\Qt5Gui.dll
	File ${QTDIR}\bin\Qt5Widgets.dll
	SetOutPath $INSTDIR\bin\platforms
	File ${QTDIR}\plugins\platforms\qwindows.dll
	SetOutPath $INSTDIR\bin\imageformats
	File ${QTDIR}\plugins\imageformats\qjpeg.dll

	SetOutPath $INSTDIR # The working directory for the shortcuts - should perhaps be something else?

	CreateShortCut "$INSTDIR\anno.lnk" "$INSTDIR\bin\anno.exe" ""
	CreateShortCut "$SMPROGRAMS\anno.lnk" "$INSTDIR\bin\anno.exe" ""
	CreateShortCut "$DESKTOP\anno.lnk" "$INSTDIR\bin\anno.exe" ""

	# Estimate installed size
	${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
	IntFmt $0 "0x%08X" $0
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\anno" "EstimatedSize" "$0"

SectionEnd

Function Launch
	ExecShell "" "$INSTDIR\anno.lnk"
FunctionEnd

Section "Uninstall"

	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\anno"
	DeleteRegKey HKCU "Software\Tomaattinen\anno"

	Delete $INSTDIR\anno-uninstaller.exe
	Delete $INSTDIR\anno.lnk
	Delete $SMPROGRAMS\anno.lnk
	Delete $DESKTOP\anno.lnk
	Delete $INSTDIR\bin\anno.exe
	Delete $INSTDIR\bin\imageformats\*.dll
	Delete $INSTDIR\bin\platforms\*.dll
	Delete $INSTDIR\bin\*.dll
	RMDir $INSTDIR\bin\imageformats
	RMDir $INSTDIR\bin\platforms
	RMDir $INSTDIR\bin
	RMDir $INSTDIR

	Delete "$SMPROGRAMS\anno.lnk"
	Delete "$DESKTOP\anno.lnk"

SectionEnd