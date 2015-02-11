;
; install.iss -- Inno Setup 4 install configuration file for Embedthis ESP
;
; Copyright (c) Embedthis Software. All Rights Reserved.
;

[Setup]
AppName=${settings.title}
AppVerName=${settings.title} ${settings.version}
DefaultDirName={pf}\${settings.title}
DefaultGroupName=${settings.title}
UninstallDisplayIcon={app}/${settings.name}.exe
LicenseFile=LICENSE.TXT
ChangesEnvironment=yes
ArchitecturesInstallIn64BitMode=x64

[Icons]
Name: "{group}\${settings.title} shell"; Filename: "{app}/bin/${settings.name}.exe"; Parameters: ""
Name: "{group}\${settings.title} documentation"; Filename: "{app}/doc/index.html"; Parameters: ""
Name: "{group}\ReadMe"; Filename: "{app}/README.TXT"

[Dirs]
Name: "{app}/bin"

[UninstallDelete]

[Tasks]
Name: addpath; Description: Add ${settings.title} to the system PATH variable;

[Code]
function IsPresent(const file: String): Boolean;
begin
  file := ExpandConstant(file);
  if FileExists(file) then begin
    Result := True;
  end else begin
    Result := False;
  end
end;

//
//	Initial sample by Jared Breland
//
procedure AddPath(keyName: String; dir: String);
var
	newPath, oldPath, key: String;
	paths:		TArrayOfString;
	i:			Integer;
	regHive:	Integer;
begin

  if (IsAdminLoggedOn or IsPowerUserLoggedOn) then begin
    regHive := HKEY_LOCAL_MACHINE;
    key := 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';
  end else begin
    regHive := HKEY_CURRENT_USER;
    key := 'Environment';
  end;

	i := 0;
	if RegValueExists(regHive, key, keyName) then begin
		RegQueryStringValue(regHive, key, keyName, oldPath);
		oldPath := oldPath + ';';

		while (Pos(';', oldPath) > 0) do begin
			SetArrayLength(paths, i + 1);
			paths[i] := Copy(oldPath, 0, Pos(';', oldPath) - 1);
			oldPath := Copy(oldPath, Pos(';', oldPath) + 1, Length(oldPath));
			i := i + 1;

			if dir = paths[i - 1] then begin
				continue;
			end;

			if i = 1 then begin
				newPath := paths[i - 1];
			end else begin
				newPath := newPath + ';' + paths[i - 1];
			end;
		end;
	end;

	if (IsUninstaller() = false) and (dir <> '') then begin
		if (newPath <> '') then begin
			newPath := newPath + ';' + dir;
		end else begin
	    	newPath := dir;
	  end;
  	end;
	RegWriteStringValue(regHive, key, keyName, newPath);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  bin: String;
begin
	if CurStep = ssPostInstall then
		if IsTaskSelected('addpath') then begin
			bin := ExpandConstant('{app}\bin');			
			AddPath('Path', bin);
	  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
	app:			String;
	bin:			String;
begin
	if CurUninstallStep = usUninstall then begin
	    bin := ExpandConstant('{app}\bin');			
		AddPath('Path', bin);
	end;
	if CurUninstallStep = usDone then begin
	    app := ExpandConstant('{app}');			
        RemoveDir(app);
    end;
end;

[Run]
Filename: "file:///{app}/doc/product/index.html"; Description: "View the Documentation"; Flags: skipifsilent waituntilidle shellexec postinstall; Check: IsPresent('{app}/doc/product/index.html');

[UninstallRun]
Filename: "{app}/bin/removeFiles.exe"; Parameters: "-r -s 5"; WorkingDir: "{app}"; Flags:

[Files]
