; SPCT installer (Inno Setup 6). Built by build.bat, which stages the files
; in dist\SPCT and passes the version:
;   ISCC /DAppVersion=0.4.0 installer\spct.iss
;
; Installs spct.exe with its data\ folder (spct finds data\ next to the exe),
; the README, LICENSE and NOTICE, a Start-menu command prompt, and
; optionally adds the install folder to PATH. Installs per user by default
; (no admin rights); the first page offers an all-users install instead.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#define StageDir "..\dist\SPCT"

[Setup]
AppId={{8031429E-FE23-41E2-9C0C-424DE9364880}
AppName=SPCT - Stefan Pohl Chess Tools (C port)
AppVersion={#AppVersion}
AppVerName=SPCT {#AppVersion}
AppPublisher=SPCT (C port of Stefan Pohl's tools)
AppPublisherURL=https://github.com/ianrastall/sp-c-tools
AppSupportURL=https://github.com/ianrastall/sp-c-tools
AppComments=Tools (C) 2024-2025, Stefan Pohl, www.sp-cc.de. C port under GPLv3 or later.
DefaultDirName={autopf}\SPCT
DefaultGroupName=SPCT
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#StageDir}\LICENSE.txt
OutputDir=..\dist
OutputBaseFilename=SPCT-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ChangesEnvironment=yes
UninstallDisplayName=SPCT {#AppVersion}
UninstallDisplayIcon={app}\spct.exe
WizardStyle=modern

[Tasks]
Name: "addtopath"; Description: "Add SPCT to the PATH (so ""spct"" works in any command prompt)"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\SPCT Command Prompt"; Filename: "{cmd}"; \
    Parameters: "/k ""set ""PATH={app};%PATH%"" && spct --help"""; WorkingDir: "{userdocs}"
Name: "{group}\SPCT README"; Filename: "{app}\README.txt"
Name: "{group}\Uninstall SPCT"; Filename: "{uninstallexe}"

[Code]
const
  UserEnvKey = 'Environment';
  SystemEnvKey = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';

function EnvRoot: Integer;
begin
  if IsAdminInstallMode then Result := HKEY_LOCAL_MACHINE
  else Result := HKEY_CURRENT_USER;
end;

function EnvKey: String;
begin
  if IsAdminInstallMode then Result := SystemEnvKey
  else Result := UserEnvKey;
end;

{ Add Dir to the PATH of this install mode's scope, unless it is there. A
  PATH that ends in ';' keeps doing so, so that RemoveFromPath, which takes
  out Dir with the separator before it, gives back exactly the old value. }
procedure AddToPath(Dir: String);
var
  Paths: String;
begin
  if not RegQueryStringValue(EnvRoot, EnvKey, 'Path', Paths) then Paths := '';
  if Pos(';' + Uppercase(Dir) + ';', ';' + Uppercase(Paths) + ';') > 0 then exit;
  if Paths = '' then Paths := Dir
  else if Paths[Length(Paths)] = ';' then Paths := Paths + Dir + ';'
  else Paths := Paths + ';' + Dir;
  RegWriteExpandStringValue(EnvRoot, EnvKey, 'Path', Paths);
end;

{ Remove Dir and the separator before it from that PATH, leaving every
  other entry as it was. }
procedure RemoveFromPath(Dir: String);
var
  Paths, S: String;
  P: Integer;
begin
  if not RegQueryStringValue(EnvRoot, EnvKey, 'Path', Paths) then exit;
  S := ';' + Paths + ';';
  P := Pos(';' + Uppercase(Dir) + ';', Uppercase(S));
  if P = 0 then exit;
  Delete(S, P, Length(Dir) + 1);
  RegWriteExpandStringValue(EnvRoot, EnvKey, 'Path', Copy(S, 2, Length(S) - 2));
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('addtopath') then
    AddToPath(ExpandConstant('{app}'));
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    RemoveFromPath(ExpandConstant('{app}'));
end;
