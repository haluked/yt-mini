; Inno Setup 6 script - ff-dlp-viewer
; English interface, clean per-user install under %LOCALAPPDATA% without admin rights

#define MyAppName "ff-dlp-viewer"
#define MyAppVersion "2.4.0"
#define MyAppPublisher "Haluk"
#define MyAppURL "https://github.com/haluked/yt-mini"
#define MyAppExeName "ff-dlp-viewer.exe"

[Setup]
AppId={{D37E8492-9C81-4B2E-A1E4-7832B6E1F091}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
LicenseFile=..\LICENSE
InfoBeforeFile=..\THIRD_PARTY_NOTICES.md
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\dist
OutputBaseFilename=ff-dlp-viewer-Setup-v{#MyAppVersion}
SetupIconFile=app.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "autostart"; Description: "Start ff-dlp-viewer with Windows"; GroupDescription: "Startup settings:"

[Files]
; Main application compiled as native C11 Win32 (< 1 MB)
Source: "..\dist\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
; License and third-party notices
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
; Optional dependencies (copied into bin/ when present)
Source: "..\bin\ffmpeg.exe"; DestDir: "{app}\bin"; Flags: ignoreversion external; Check: FileExists(ExpandConstant('{src}\..\bin\ffmpeg.exe'))
Source: "..\bin\yt-dlp.exe"; DestDir: "{app}\bin"; Flags: ignoreversion external; Check: FileExists(ExpandConstant('{src}\..\bin\yt-dlp.exe'))

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Auto-start at Windows login
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#MyAppName}"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: autostart
; Application settings (download folder / tray hint)
Root: HKCU; Subkey: "Software\ff-dlp-viewer"; Flags: uninsdeletekey

[UninstallDelete]
; Tools downloaded by the first-run bootstrapper (not part of [Files])
Type: filesandordirs; Name: "{localappdata}\ff-dlp-viewer"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
