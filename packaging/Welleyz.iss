#define Stage GetEnv("WELLEYZ_STAGE")
#define Version GetEnv("WELLEYZ_VERSION")

#if Stage == ""
  #error WELLEYZ_STAGE is required
#endif

[Setup]
AppId={{E89622F2-012D-4A76-86C9-19976496BE80}
AppName=welleyz
AppVersion={#Version}
AppPublisher=shadowave
VersionInfoVersion={#Version}
VersionInfoCompany=shadowave
VersionInfoDescription=welleyz Setup
DefaultDirName={autopf}\shadowave\welleyz
DefaultGroupName=shadowave
OutputDir={#Stage}
OutputBaseFilename=welleyz-{#Version}-setup
SetupIconFile={#Stage}\welleyz.ico
UninstallDisplayIcon={app}\welleyz.ico
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "{#Stage}\VST3\welleyz.vst3\*"; DestDir: "{code:GetVstDirectory}\welleyz.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Stage}\Standalone\welleyz.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\welleyz-uninstall.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\welleyz-update.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\release.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\welleyz.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\THIRD-PARTY-NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\WELLEYZ_USER_GUIDE_RU_EN.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\Syne-OFL.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\Outfit-OFL.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\Sarina-OFL.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\welleyz"; Filename: "{app}\welleyz.exe"; IconFilename: "{app}\welleyz.ico"
Name: "{autoprograms}\Uninstall welleyz"; Filename: "{app}\welleyz-uninstall.exe"; IconFilename: "{app}\welleyz.ico"
Name: "{autoprograms}\Update welleyz"; Filename: "{app}\welleyz-update.exe"; IconFilename: "{app}\welleyz.ico"

[Run]
Filename: "{app}\welleyz.exe"; Description: "Launch welleyz"; Flags: nowait postinstall skipifsilent

[Registry]
Root: HKLM; Subkey: "Software\shadowave\welleyz"; ValueType: string; ValueName: "VstDirectory"; ValueData: "{code:GetVstDirectory}"; Flags: uninsdeletevalue

[Code]
var
  VstDirectoryPage: TInputDirWizardPage;

procedure InitializeWizard;
var
  SavedDirectory: string;
begin
  VstDirectoryPage := CreateInputDirPage(wpSelectDir,
    'VST3 plugin location', 'Choose the VST3 directory',
    'Select the folder scanned by your DAW. The welleyz.vst3 bundle will be placed inside it.',
    False, '');
  VstDirectoryPage.Add('VST3 directory:');
  VstDirectoryPage.Values[0] := ExpandConstant('{commoncf}\VST3');
  if RegQueryStringValue(HKLM, 'Software\shadowave\welleyz', 'VstDirectory', SavedDirectory) then
    VstDirectoryPage.Values[0] := SavedDirectory;
end;

function GetVstDirectory(Param: string): string;
begin
  Result := VstDirectoryPage.Values[0];
end;
