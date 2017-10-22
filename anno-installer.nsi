# TODO: The version should be set automatically by the build server
!define VERSION "1.0"

!include "MUI.nsh"

Name "anno"
OutFile "anno-installer-v${VERSION}.exe"

InstallDir "$PROGRAMFILES64\anno"

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

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
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\anno" "Publisher" "Tomaattinen Ltd"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\anno" "DisplayVersion" "${VERSION}"

	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\anno" "DisplayIcon" "$INSTDIR\bin\anno.exe"
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\anno" "EstimatedSize" "20000"

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

	CreateShortCut "$SMPROGRAMS\anno.lnk" "$INSTDIR\bin\anno.exe" ""
	CreateShortCut "$DESKTOP\anno.lnk" "$INSTDIR\bin\anno.exe" ""

SectionEnd

Section "Uninstall"

	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\anno"
	DeleteRegKey HKCU "Software\Tomaattinen\anno"

	Delete $INSTDIR\anno-uninstaller.exe
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