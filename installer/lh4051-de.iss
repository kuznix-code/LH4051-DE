; LH4051-DE Installer (Inno Setup 6)
; Inputs come from environment variables (ISPP /D values are parsed as
; expressions, so bare strings like "x64" or "0.1.0.2026..." would not compile):
;   LH_VERSION (default 0.1.0)  LH_ARCH (x64|arm64)  LH_BUNDLE (bundle dir)  LH_OUT (output dir)
#define AppVersion GetEnv("LH_VERSION")
#if AppVersion == ""
  #define AppVersion "0.1.0"
#endif
#define Arch GetEnv("LH_ARCH")
#if Arch == ""
  #define Arch "x64"
#endif
#define BundleDir GetEnv("LH_BUNDLE")
#if BundleDir == ""
  #define BundleDir "..\dist\lh4051-de-" + AppVersion + "-win-" + Arch
#endif
#define OutputFolder GetEnv("LH_OUT")
#if OutputFolder == ""
  #define OutputFolder "..\dist"
#endif

[Setup]
AppId={{6E0C1B0A-4051-4B1D-9C42-4C48344F5330}
AppName=LH4051-DE
AppVersion={#AppVersion}
AppVerName=LH4051-DE {#AppVersion}
AppPublisher=kuznix-code
AppPublisherURL=https://github.com/kuznix-code/LH4051-DE
AppSupportURL=https://github.com/kuznix-code/LH4051-DE/issues
DefaultDirName={autopf}\LH4051-DE
DefaultGroupName=LH4051-DE
DisableProgramGroupPage=yes
OutputDir={#OutputFolder}
OutputBaseFilename=LH4051-DE-Installer-{#AppVersion}-win-{#Arch}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\lh4051-de.exe
SetupWindowTitle=LH4051-DE Installer
#if Arch == "arm64"
ArchitecturesAllowed=arm64
ArchitecturesInstallIn64BitMode=arm64
#else
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#endif

[Messages]
SetupAppTitle=LH4051-DE Installer
SetupWindowTitle=LH4051-DE Installer
WelcomeLabel1=Welcome to the LH4051-DE Installer
WelcomeLabel2=This installs LH4051-DE {#AppVersion}, a Longhorn Build 4051 style desktop, including its GTK4 runtime.

[Tasks]
Name: desktopicon; Description: "Create a &desktop shortcut"; Flags: unchecked

[Files]
Source: "{#BundleDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\LH4051-DE"; Filename: "{app}\lh4051-de.exe"; Parameters: "--windowed"
Name: "{group}\LH4051-DE (full screen)"; Filename: "{app}\lh4051-de.exe"
Name: "{group}\Uninstall LH4051-DE"; Filename: "{uninstallexe}"
Name: "{autodesktop}\LH4051-DE"; Filename: "{app}\lh4051-de.exe"; Parameters: "--windowed"; Tasks: desktopicon

[Run]
Filename: "{app}\lh4051-de.exe"; Parameters: "--windowed"; Description: "Launch LH4051-DE"; Flags: nowait postinstall skipifsilent
