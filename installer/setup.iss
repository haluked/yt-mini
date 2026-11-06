; Inno Setup 6 Scripti - ff-dlp-viewer
; Türkçe arayüzlü, %LOCALAPPDATA% altına yönetici izni gerektirmeyen temiz kurulum

#define MyAppName "ff-dlp-viewer"
#define MyAppVersion "2.0.0"
#define MyAppPublisher "Haluk"
#define MyAppURL "https://github.com/haluked/ff-dlp-viewer"
#define MyAppExeName "ff-dlp-viewer.exe"

[Setup]
AppId={{D37E8492-9C81-4B2E-A1E4-7832B6E1F091}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
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
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "autostart"; Description: "Windows açıldığında sistem tepsisinde (tray) otomatik başlat"; GroupDescription: "Başlangıç Ayarları:"

[Files]
; Native C11 Win32 derlenmiş ana uygulama (< 1 MB)
Source: "..\dist\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
; İsteğe bağlı bağımlılıklar (varsa bin/ altına kopyalanır)
Source: "..\bin\ffmpeg.exe"; DestDir: "{app}\bin"; Flags: ignoreversion external; Check: FileExists(ExpandConstant('{src}\..\bin\ffmpeg.exe'))
Source: "..\bin\yt-dlp.exe"; DestDir: "{app}\bin"; Flags: ignoreversion external; Check: FileExists(ExpandConstant('{src}\..\bin\yt-dlp.exe'))

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Windows açılışında otomatik başlatma
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#MyAppName}"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: autostart

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
