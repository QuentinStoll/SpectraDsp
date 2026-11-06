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
  #define Vst3Dir "..\..\build\bin\lib\VST3\Profiler.vst3"
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
WizardStyle=modern
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

[Files]
Source: "{#StandaloneDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: standalone
Source: "{#Vst3Dir}\*"; DestDir: "{commoncf64}\VST3\Profiler.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

[Icons]
Name: "{autoprograms}\Spectra"; Filename: "{app}\Profiler.exe"; Components: standalone
Name: "{autodesktop}\Spectra"; Filename: "{app}\Profiler.exe"; Components: standalone; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked; Components: standalone

[Run]
Filename: "{app}\Profiler.exe"; Description: "Launch Spectra"; Flags: nowait postinstall skipifsilent; Components: standalone
