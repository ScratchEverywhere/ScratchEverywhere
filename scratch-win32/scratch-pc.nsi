!define APPNAME "Scratch Everywhere!"
!define REGUINSTKEY "{1aa2d359-73a5-47b7-b3cd-153e59b4bbb4}"
!define REGUINST 'HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${REGUINSTKEY}"'
Name "${APPNAME}"
Outfile "scratch-pc.exe"
RequestExecutionLevel User
Unicode True
InstallDirRegKey ${REGUINST} UninstallString
!define MUI_ICON "scratch-pc\scratch-pc.ico"
!define MUI_COMPONENTSPAGE_NODESC
!include MUI2.nsh
!include LogicLib.nsh
!define /IfNDef SHPPFW_DIRCREATE 0x01
!define /IfNDef KF_FLAG_CREATE 0x00008000
!define /IfNDef FOLDERID_UserProgramFiles {5CD7AEE2-2219-4A67-B85D-6C9CE15660CB}


Function .onInit
StrCpy $0 "$LocalAppData\Programs"

${If} $InstDir == ""
	System::Call 'SHELL32::SHGetKnownFolderPath(g "${FOLDERID_UserProgramFiles}", i ${KF_FLAG_CREATE}, p 0, *p .r2)i.r1'
	${If} $1 == 0
		System::Call '*$2(&w${NSIS_MAX_STRLEN} .r1)'
		System::Call 'OLE32::CoTaskMemFree(p r2)'
		StrCpy $0 $1
	${EndIf}

	StrCpy $InstDir "$0\${APPNAME}"
${EndIf}
FunctionEnd


!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE English


Section "Required files"
SectionIn RO
System::Call 'SHELL32::SHPathPrepareForWrite(p $hwndParent, p 0, t d, i ${SHPPFW_DIRCREATE})'
SetOutPath $InstDir
WriteUninstaller "$InstDir\Uninstall.exe"
WriteRegStr ${REGUINST} UninstallString '"$InstDir\Uninstall.exe"'
WriteRegStr ${REGUINST} DisplayName "${APPNAME}"
WriteRegStr ${REGUINST} UrlInfoAbout "https://scratcheverywhere.github.io"

File "scratch-pc\scratch-pc.exe"
File "scratch-pc\scratch-pc.ico"
SectionEnd
 
Section "Start menu shortcut"
CreateShortcut /NoWorkingDir "$SMPrograms\${APPNAME}.lnk" "$Instdir\scratch-pc.exe"
SectionEnd

Section -Uninstall
Delete "$SMPrograms\${APPNAME}.lnk"
RMDir /r "$InstDir"
DeleteRegKey ${REGUINST}
SectionEnd
