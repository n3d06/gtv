; Bản cài gotiengviet: cài theo người dùng, không cần quyền admin.
; Build: build.bat (gọi ISCC nếu đã cài Inno Setup 6; GTV_VERSION đặt số phiên bản).

#define AppName "gotiengviet"
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#define AppExe "gotiengviet.exe"

[Setup]
AppId={{B6FB87F2-B1DE-4F87-B22D-30D27A6F1C48}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=n3d06
AppPublisherURL=https://github.com/n3d06/gtv
AppSupportURL=https://github.com/n3d06/gtv/issues
DefaultDirName={localappdata}\Programs\{#AppName}
DisableProgramGroupPage=yes
DisableDirPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\build\installer
OutputBaseFilename={#AppName}-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\src\gotiengviet.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
CloseApplications=no

[Tasks]
Name: "autostart"; Description: "Khởi động cùng Windows"

[Files]
Source: "..\build\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{userprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#AppName}"; ValueData: """{app}\{#AppExe}"""; Tasks: autostart
; Gỡ cài đặt luôn xóa, kể cả khi bật tự khởi động từ menu khay sau này.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "{#AppName}"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\{#AppName}"; ValueType: none; Flags: uninsdeletekey

[UninstallDelete]
Type: filesandordirs; Name: "{userappdata}\{#AppName}"

[Run]
Filename: "{app}\{#AppExe}"; Description: "Chạy {#AppName}"; Flags: nowait postinstall skipifsilent

[Code]
function IsRunning(): Boolean;
begin
  Result := FindWindowByClassName('GotiengvietTray') <> 0;
end;

{ Tắt bộ gõ đang chạy; bản chạy quyền admin thì cần UAC để tắt. }
procedure StopApp();
var
  rc: Integer;
begin
  if not IsRunning() then Exit;
  Exec(ExpandConstant('{sys}\taskkill.exe'), '/IM {#AppExe} /F', '', SW_HIDE, ewWaitUntilTerminated, rc);
  Sleep(300);
  if IsRunning() then
    ShellExec('runas', ExpandConstant('{sys}\taskkill.exe'), '/IM {#AppExe} /F', '', SW_HIDE, ewWaitUntilTerminated, rc);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  StopApp();
  Result := '';
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  rc: Integer;
begin
  if CurUninstallStep <> usUninstall then Exit;
  StopApp();
  { Task tự khởi động của chế độ admin: chỉ xóa được bằng quyền admin. }
  if RegValueExists(HKCU, 'Software\{#AppName}', 'AdminTask') then
    ShellExec('runas', ExpandConstant('{sys}\schtasks.exe'), '/Delete /TN {#AppName} /F', '', SW_HIDE, ewWaitUntilTerminated, rc);
end;
