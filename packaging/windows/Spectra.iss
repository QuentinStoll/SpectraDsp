; Spectra installer. Built from CI via packaging/build_installer.py.
; Components let the user install the standalone app, the VST3 plugin, or both.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef Arch
  #define Arch "x64"
#endif
#ifndef StandaloneDir
  #define StandaloneDir "..\..\build\bin\Standalone"
#endif
#ifndef Vst3Dir
  #define Vst3Dir "..\..\build\bin\lib\VST3\Spectra.vst3"
#endif
#ifndef SetupIcon
  #define SetupIcon "..\..\Source\Assets\png\SpectraIcon.ico"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif
#ifndef OutputName
  #define OutputName "Spectra-windows-x64"
#endif
#ifndef LicenseFile
  #define LicenseFile "..\..\LICENSE"
#endif

[Setup]
AppId={{A7E6C4B1-8D2F-4F1A-9C3E-5B8A1D6E0F27}
AppName=Spectra
AppVersion={#AppVersion}
AppPublisher=Spectra DSP
AppPublisherURL=https://spectradsp.com
AppSupportURL=https://spectradsp.com
DefaultDirName={autopf}\Spectra
DefaultGroupName=Spectra
DisableProgramGroupPage=yes
LicenseFile={#LicenseFile}
OutputDir={#OutputDir}
OutputBaseFilename={#OutputName}
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
MinVersion=10.0
UninstallDisplayName=Spectra
UninstallDisplayIcon={app}\Spectra.ico
SetupIconFile={#SetupIcon}
WizardStyle=modern
AppMutex=SpectraStandaloneMutex
SetupMutex=SpectraSetupMutex
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
UninstallLogging=yes
UninstallFilesDir={app}
UsePreviousAppDir=yes
#if Arch == "arm64"
ArchitecturesAllowed=arm64
ArchitecturesInstallIn64BitMode=arm64
#else
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Standalone and VST3"
Name: "custom"; Description: "Choose components"; Flags: iscustom

[Components]
Name: "standalone"; Description: "Standalone application"; Types: full custom; Flags: checkablealone
Name: "vst3"; Description: "VST3 plugin"; Types: full custom; Flags: checkablealone

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked; Components: standalone

[Dirs]
Name: "{app}"; Flags: uninsalwaysuninstall
Name: "{commoncf64}\VST3\Spectra.vst3"; Flags: uninsalwaysuninstall; Components: vst3

[Files]
Source: "{#SetupIcon}"; DestDir: "{app}"; DestName: "Spectra.ico"; Flags: ignoreversion uninsrestartdelete
Source: "{#StandaloneDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs uninsrestartdelete; Components: standalone
Source: "{#Vst3Dir}\*"; DestDir: "{commoncf64}\VST3\Spectra.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs uninsrestartdelete; Components: vst3

[Icons]
Name: "{autoprograms}\Spectra"; Filename: "{app}\Spectra.exe"; Components: standalone
Name: "{autodesktop}\Spectra"; Filename: "{app}\Spectra.exe"; Components: standalone; Tasks: desktopicon

[InstallDelete]
Type: files; Name: "{app}\Profiler.exe"
Type: files; Name: "{app}\Profiler.bin"
Type: filesandordirs; Name: "{commoncf64}\VST3\Profiler.vst3"
Type: files; Name: "{autoprograms}\Profiler.lnk"
Type: files; Name: "{autodesktop}\Profiler.lnk"

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
Type: filesandordirs; Name: "{commoncf64}\VST3\Spectra.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Profiler.vst3"
Type: files; Name: "{autoprograms}\Spectra.lnk"
Type: files; Name: "{autodesktop}\Spectra.lnk"

[Run]
Filename: "{app}\Spectra.exe"; Description: "Launch Spectra"; Flags: nowait postinstall skipifsilent; Components: standalone
