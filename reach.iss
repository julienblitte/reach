#dim Version[4]
#define ApplicationName 'Reach'
#define Application32Exe 'Release32\reach32.exe'
#define Application64Exe 'Release64\reach64.exe'
#expr ParseVersion(Application64Exe, Version[0], Version[1], Version[2], Version[3])

[Setup]
AppName={#ApplicationName}
AppVersion={#Version[0]}.{#Version[1]}
VersionInfoVersion={#Version[0]}.{#Version[1]}.{#Version[2]}.{#Version[3]}
VersionInfoCopyright=Julien Blitte
DisableDirPage=yes
DefaultDirName="{sysnative}"
DisableProgramGroupPage=yes
DefaultGroupName={#ApplicationName}
Compression=lzma2
SolidCompression=yes
OutputBaseFilename={#ApplicationName} Setup
OutputDir=.

[Types]
Name: "default"; Description: "64 bits"
Name: "x86compatible"; Description: "32 bits"

[Components]
Name: "x64exe"; Description: "Main Files (64 bits)"; Types: default; Flags: exclusive
Name: "x86exe"; Description: "Main Files (32 bits)"; Types: x86compatible; Flags: exclusive

[Files]
Source: {#Application64Exe}; DestDir: "{sysnative}"; DestName: "reach.exe"; Components: x64exe; 
Source: {#Application32Exe}; DestDir: "{syswow64}"; Components: x64exe;
Source: {#Application32Exe}; DestDir: "{sysnative}"; DestName: "reach.exe"; Components: x86exe;
