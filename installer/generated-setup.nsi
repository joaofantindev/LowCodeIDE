; Instalador grÃ¡fico da C++ Visual IDE (NSIS + MUI2)
; Gerado por installer/build-installer.ps1 â€” nÃ£o edite diretamente.

!include "MUI2.nsh"

Name "C++ Visual IDE"
OutFile "C:\Users\joaof\Desktop\code\cpp\IDE\installer\CppVisualIDE-Setup.exe"
InstallDir "$LOCALAPPDATA\Programs\CppVisualIDE"
InstallDirRegKey HKCU "Software\CppVisualIDE" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUnInstDetails show

!define APP_VERSION "0.1.0"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\CppVisualIDE.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Abrir a C++ Visual IDE agora"

VIProductVersion "0.1.0.0"
VIAddVersionKey "ProductName" "C++ Visual IDE"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"
VIAddVersionKey "FileVersion" "0.1.0.0"
VIAddVersionKey "LegalCopyright" "C++ Visual IDE"
VIAddVersionKey "FileDescription" "Instalador da C++ Visual IDE"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "PortugueseBR"

Section "C++ Visual IDE (obrigatÃ³rio)" SEC_MAIN
    SectionIn RO
    SetOutPath "$INSTDIR"

    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\CppVisualIDE.exe"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libb2-1.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libbrotlicommon.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libbrotlidec.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libbz2-1.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libdouble-conversion.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libfreetype-6.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libgcc_s_seh-1.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libglib-2.0-0.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libgraphite2.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libharfbuzz-0.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libiconv-2.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libicudt78.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libicuin78.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libicuuc78.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libintl-8.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libmd4c.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libpcre2-16-0.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libpcre2-8-0.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libpng16-16.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libstdc++-6.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libwinpthread-1.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libz-1.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\libzstd.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\Qt6Core.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\Qt6Gui.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\Qt6Network.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\Qt6Widgets.dll"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\zlib1.dll"
    SetOutPath "$INSTDIR\platforms"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\platforms\*.*"
    SetOutPath "$INSTDIR\styles"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\styles\*.*"
    SetOutPath "$INSTDIR\imageformats"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\imageformats\*.*"
    SetOutPath "$INSTDIR\generic"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\generic\*.*"
    SetOutPath "$INSTDIR\networkinformation"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\networkinformation\*.*"
    SetOutPath "$INSTDIR\tls"
    File "C:\Users\joaof\Desktop\code\cpp\IDE\build\tls\*.*"
    SetOutPath "$INSTDIR"

    WriteUninstaller "$INSTDIR\uninstall.exe"
    WriteRegStr HKCU "Software\CppVisualIDE" "InstallDir" "$INSTDIR"
    WriteRegStr HKCU "Software\CppVisualIDE" "Version" "${APP_VERSION}"
SectionEnd

Section "Atalho no Menu Iniciar" SEC_STARTMENU
    CreateDirectory "$SMPROGRAMS\C++ Visual IDE"
    CreateShortcut "$SMPROGRAMS\C++ Visual IDE\C++ Visual IDE.lnk" "$INSTDIR\CppVisualIDE.exe"
    CreateShortcut "$SMPROGRAMS\C++ Visual IDE\Desinstalar.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section "Atalho na Ãrea de Trabalho" SEC_DESKTOP
    CreateShortcut "$DESKTOP\C++ Visual IDE.lnk" "$INSTDIR\CppVisualIDE.exe"
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\CppVisualIDE.exe"
    Delete "$INSTDIR\*.dll"
    Delete "$INSTDIR\uninstall.exe"
    RMDir /r "$INSTDIR\platforms"
    RMDir /r "$INSTDIR\styles"
    RMDir /r "$INSTDIR\imageformats"
    RMDir /r "$INSTDIR\generic"
    RMDir /r "$INSTDIR\networkinformation"
    RMDir /r "$INSTDIR\tls"
    RMDir "$INSTDIR"

    Delete "$SMPROGRAMS\C++ Visual IDE\C++ Visual IDE.lnk"
    Delete "$SMPROGRAMS\C++ Visual IDE\Desinstalar.lnk"
    RMDir "$SMPROGRAMS\C++ Visual IDE"
    Delete "$DESKTOP\C++ Visual IDE.lnk"
    DeleteRegKey HKCU "Software\CppVisualIDE"
SectionEnd

