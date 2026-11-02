; =============================================================================
; СЭДМ — сценарий установщика Inno Setup 6 (п. 4.6.1 ТЗ: дистрибутивная версия)
;
; Один установщик для 32- и 64-разрядных Windows: на 64-разрядной системе
; устанавливается версия x64, на 32-разрядной — x86 (п. 4.3.3 ТЗ).
; Состав: программа администратора, программа дегустатора, руководства.
; Среда выполнения Visual C++ (для клиентской библиотеки PostgreSQL libpq)
; устанавливается из официальных пакетов Microsoft, только если её нет.
; Сборка: build_installer.ps1 (готовит каталоги stage\x64, stage\x86, redist).
; =============================================================================

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{8C1B7E4A-3F2D-4C5B-9E1A-5D2F7A6B9C10}
AppName=СЭДМ — экспертная дегустация мёда
AppVersion={#AppVersion}
AppVerName=СЭДМ {#AppVersion}
AppPublisher=МГТУ им. Н.Э. Баумана, кафедра ИУ6
AppComments=Программная система экспертной дегустации мёда
DefaultDirName={autopf}\SEDM
DefaultGroupName=СЭДМ
DisableProgramGroupPage=yes
OutputDir=output
OutputBaseFilename=SEDM-{#AppVersion}-setup
SetupIconFile=..\resources\sedm.ico
UninstallDisplayIcon={app}\sedm_taster.exe
UninstallDisplayName=СЭДМ {#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x86compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
; установка «только для меня» без прав администратора (среда Visual C++ должна быть уже установлена)
PrivilegesRequiredOverridesAllowed=commandline dialog
MinVersion=6.1sp1
VersionInfoVersion={#AppVersion}
VersionInfoDescription=Установка СЭДМ

[Languages]
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Types]
Name: "full"; Description: "Полная установка (администратор и дегустатор)"
Name: "taster"; Description: "Рабочее место дегустатора"
Name: "admin"; Description: "Рабочее место администратора"
Name: "custom"; Description: "Выборочная установка"; Flags: iscustom

[Components]
Name: "admin"; Description: "Программа администратора"; Types: full admin
Name: "taster"; Description: "Программа дегустатора"; Types: full taster
Name: "docs"; Description: "Руководства: пользователя, администратора, дегустатора, системного программиста, по установке (PDF, DOCX)"; Types: full admin taster

[Tasks]
Name: "desktopicon"; Description: "Создать значки на рабочем столе"; GroupDescription: "Дополнительно:"

[Files]
; --- 64-разрядная версия: библиотеки Qt, драйвер PostgreSQL, libpq
Source: "stage\x64\*"; DestDir: "{app}"; Excludes: "sedm_admin.exe,sedm_taster.exe"; Flags: recursesubdirs ignoreversion; Check: Is64BitInstallMode
Source: "stage\x64\sedm_admin.exe"; DestDir: "{app}"; Components: admin; Flags: ignoreversion; Check: Is64BitInstallMode
Source: "stage\x64\sedm_taster.exe"; DestDir: "{app}"; Components: taster; Flags: ignoreversion; Check: Is64BitInstallMode
; --- 32-разрядная версия
Source: "stage\x86\*"; DestDir: "{app}"; Excludes: "sedm_admin.exe,sedm_taster.exe"; Flags: recursesubdirs ignoreversion; Check: not Is64BitInstallMode
Source: "stage\x86\sedm_admin.exe"; DestDir: "{app}"; Components: admin; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: "stage\x86\sedm_taster.exe"; DestDir: "{app}"; Components: taster; Flags: ignoreversion; Check: not Is64BitInstallMode
; --- среда выполнения Visual C++ для libpq (устанавливается при необходимости)
Source: "redist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: Is64BitInstallMode and NeedsVc140
Source: "redist\vcredist_2013_x86.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: (not Is64BitInstallMode) and NeedsVc120
; --- документация и сведения о сторонних компонентах
Source: "docs\*.pdf"; DestDir: "{app}\docs"; Components: docs; Flags: ignoreversion skipifsourcedoesntexist
Source: "docs\*.docx"; DestDir: "{app}\docs"; Components: docs; Flags: ignoreversion skipifsourcedoesntexist
; --- сценарии ручной настройки сервера (руководство системного программиста, раздел В.3)
Source: "..\resources\sql\setup_manual.sql"; DestDir: "{app}\sql"; Flags: ignoreversion
Source: "..\resources\sql\schema.sql"; DestDir: "{app}\sql"; Flags: ignoreversion
Source: "THIRD_PARTY.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\СЭДМ — администратор"; Filename: "{app}\sedm_admin.exe"; Components: admin
Name: "{group}\СЭДМ — дегустатор"; Filename: "{app}\sedm_taster.exe"; Components: taster
Name: "{group}\Руководства"; Filename: "{app}\docs"; Components: docs
Name: "{group}\Удалить СЭДМ"; Filename: "{uninstallexe}"
Name: "{autodesktop}\СЭДМ — администратор"; Filename: "{app}\sedm_admin.exe"; Components: admin; Tasks: desktopicon
Name: "{autodesktop}\СЭДМ — дегустатор"; Filename: "{app}\sedm_taster.exe"; Components: taster; Tasks: desktopicon

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Установка среды выполнения Visual C++…"; Flags: waituntilterminated; Check: Is64BitInstallMode and NeedsVc140
Filename: "{tmp}\vcredist_2013_x86.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Установка среды выполнения Visual C++…"; Flags: waituntilterminated; Check: (not Is64BitInstallMode) and NeedsVc120
Filename: "{app}\sedm_admin.exe"; Description: "Запустить программу администратора"; Flags: nowait postinstall skipifsilent; Components: admin
Filename: "{app}\sedm_taster.exe"; Description: "Запустить программу дегустатора"; Flags: nowait postinstall skipifsilent unchecked; Components: taster

[UninstallDelete]
; настройки и автокопии пользователя (%LOCALAPPDATA%\SEDM) не удаляются —
; в автокопиях могут быть данные дегустации

[Code]
{ Установлена ли среда выполнения Visual C++ 2015–2022 (x64): нужна libpq PostgreSQL 14+ }
function NeedsVc140: Boolean;
var
  Installed: Cardinal;
begin
  Result := not (RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
                                    'Installed', Installed) and (Installed = 1));
end;

{ Установлена ли среда выполнения Visual C++ 2013 (x86): нужна libpq PostgreSQL 10 }
function NeedsVc120: Boolean;
begin
  Result := not FileExists(ExpandConstant('{sys}\msvcr120.dll'));
end;
