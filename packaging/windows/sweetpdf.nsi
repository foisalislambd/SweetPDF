; Windows installer for SweetPDF.
;
; Packages the *deployed* staging folder - the executable plus Qt, MuPDF,
; Tesseract and the compiler runtime - so the installed program has everything it
; needs beside it. The CI job fills that folder before calling makensis; see
; "Package for Windows" in .github/workflows/build.yml.
;
; Build it from the repository root:
;   makensis -DVERSION=1.0.0 -DARCH=x86_64 -DSTAGE=dist/SweetPDF \
;            -DOUTFILE=build/sweetpdf-1.0.0-windows-x86_64-setup.exe \
;            packaging/windows/sweetpdf.nsi

Unicode true
SetCompressor /SOLID lzma

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"

!ifndef VERSION
    !define VERSION "0.0.0"
!endif
!ifndef ARCH
    !define ARCH "x86_64"
!endif
!ifndef STAGE
    !define STAGE "dist\SweetPDF"
!endif
!ifndef OUTFILE
    !define OUTFILE "sweetpdf-${VERSION}-windows-${ARCH}-setup.exe"
!endif
; How makensis resolves a relative path depends on the command it appears in, so
; the CI job passes these two as absolute paths. The defaults are relative to this
; script, which is what a hand-run from a checkout wants.
!ifndef ICON
    !define ICON "..\..\resources\icons\sweetpdf.ico"
!endif
!ifndef LICENSEFILE
    !define LICENSEFILE "..\..\LICENSE"
!endif

!define APPNAME "SweetPDF"
!define PUBLISHER "SweetPDF contributors"
!define HOMEPAGE "https://github.com/foisalislambd/sweetPDF"
!define EXENAME "sweetpdf.exe"
!define PROGID "SweetPDF.Document"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

Name "${APPNAME} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\${APPNAME}"
; Reinstalling into an existing location must not silently move the program.
InstallDirRegKey HKLM "Software\${APPNAME}" "InstallDir"
RequestExecutionLevel admin
ShowInstDetails show
ShowUninstDetails show

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "${APPNAME} installer"
VIAddVersionKey "CompanyName" "${PUBLISHER}"
VIAddVersionKey "LegalCopyright" "${PUBLISHER}. AGPL-3.0-or-later."

!define MUI_ABORTWARNING
!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXENAME}"
!define MUI_FINISHPAGE_RUN_TEXT "Open ${APPNAME} now"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${LICENSEFILE}"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ---------------------------------------------------------------------------

Function .onInit
    ; A 32-bit build has no business installing on top of a 64-bit one, and the
    ; registry views differ, so be explicit about which one we write.
    SetRegView 64

    ; Upgrading over an old tree would leave DLLs behind that no longer belong to
    ; any version, so remove the previous install first if we can find it.
    ReadRegStr $R0 HKLM "${UNINSTKEY}" "QuietUninstallString"
    ${If} $R0 != ""
        DetailPrint "Removing the previous version..."
        ClearErrors
        ExecWait '$R0 _?=$INSTDIR' $R1
    ${EndIf}
FunctionEnd

Section "${APPNAME}" SecMain
    SectionIn RO

    SetOutPath "$INSTDIR"
    File /r "${STAGE}\*.*"

    WriteRegStr HKLM "Software\${APPNAME}" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\${APPNAME}" "Version" "${VERSION}"

    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; Everything Add/Remove Programs shows. Without DisplayIcon and
    ; EstimatedSize the entry looks like it came from nowhere.
    WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${APPNAME}"
    WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\${EXENAME}"
    WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "${PUBLISHER}"
    WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
    WriteRegStr HKLM "${UNINSTKEY}" "URLInfoAbout" "${HOMEPAGE}"
    WriteRegStr HKLM "${UNINSTKEY}" "HelpLink" "${HOMEPAGE}#how-do-i"
    WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "${UNINSTKEY}" "EstimatedSize" "$0"

    CreateShortCut "$SMPROGRAMS\${APPNAME}.lnk" "$INSTDIR\${EXENAME}"
SectionEnd

Section "Desktop shortcut" SecDesktop
    CreateShortCut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\${EXENAME}"
SectionEnd

Section "Open PDF files with ${APPNAME}" SecAssoc
    ; Registers the program as a choice for PDFs rather than seizing the default:
    ; since Windows 10 only the user can pick the default handler, and an
    ; installer that tries to force it gets overridden anyway.
    WriteRegStr HKLM "Software\Classes\${PROGID}" "" "PDF Document"
    WriteRegStr HKLM "Software\Classes\${PROGID}\DefaultIcon" "" "$INSTDIR\${EXENAME},0"
    WriteRegStr HKLM "Software\Classes\${PROGID}\shell\open" "" "Open with ${APPNAME}"
    WriteRegStr HKLM "Software\Classes\${PROGID}\shell\open\command" "" '"$INSTDIR\${EXENAME}" "%1"'
    WriteRegStr HKLM "Software\Classes\.pdf\OpenWithProgids" "${PROGID}" ""

    ; Registered Applications is what the Settings app reads when it offers
    ; "Choose default apps by file type".
    WriteRegStr HKLM "Software\Classes\Applications\${EXENAME}\shell\open\command" "" '"$INSTDIR\${EXENAME}" "%1"'
    WriteRegStr HKLM "Software\Classes\Applications\${EXENAME}" "FriendlyAppName" "${APPNAME}"
    WriteRegStr HKLM "Software\Classes\Applications\${EXENAME}\SupportedTypes" ".pdf" ""

    ; Explorer caches associations; without this the entry appears after a reboot.
    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "The ${APPNAME} program and the libraries it needs."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "Put a shortcut on the desktop."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecAssoc} "Offer ${APPNAME} in the 'Open with' menu for PDF files."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ---------------------------------------------------------------------------

Function un.onInit
    SetRegView 64
FunctionEnd

Section "Uninstall"
    Delete "$SMPROGRAMS\${APPNAME}.lnk"
    Delete "$DESKTOP\${APPNAME}.lnk"

    DeleteRegKey HKLM "Software\Classes\${PROGID}"
    DeleteRegKey HKLM "Software\Classes\Applications\${EXENAME}"
    DeleteRegValue HKLM "Software\Classes\.pdf\OpenWithProgids" "${PROGID}"
    DeleteRegKey HKLM "${UNINSTKEY}"
    DeleteRegKey HKLM "Software\${APPNAME}"

    Delete "$INSTDIR\uninstall.exe"
    ; Only ever the install directory, and only after the uninstaller itself is
    ; gone, so a mistaken $INSTDIR cannot take someone's documents with it.
    RMDir /r "$INSTDIR"

    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
SectionEnd
