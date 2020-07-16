; Set the Title variable to name of your Virtual machine here. I've called my virtual machine "4GB"
Title = 4GB

; Set this to the name of the pipe you're redirecting Virtual Box's output to
PipeName = ros_pipe3

; Kill WinDbg
IfWinExist, ahk_class WinDbgFrameClass
{
	WinActivate, ahk_class WinDbgFrameClass
	WinWaitActive, ahk_class WinDbgFrameClass
	Send, !{F4}
	Sleep 2000
}


; Restart VM and pause
WinActivate, %Title%
WinWaitActive, %Title%
Sleep 100
WinActivate, %Title%
Send, {RControl Down}
Send, r
Send, {RControl Up}
Sleep 50
Send, {RControl Down}
Send, p
Send, {RControl Up}
;WinActivate, %Title%

; Build a livecd
SetTitleMatchMode 2
WinActivate, ahk_class ConsoleWindowClass
WinWaitActive, ahk_class ConsoleWindowClass
Sleep 100
send ninja livecd{Enter}
Sleep, 4000

; Resume vm
WinActivate, %Title%
WinWaitActive, %Title%
Send, {RControl Down}
Send, p
Send, {RControl Up}

; Press a key to boot from cd
Sleep, 1500
Send {Right}


Run, Windbg.exe, C:\Users\Administrator\Downloads\windbg_6.12.0002.633_x86\debugger_x86\, UseErrorLevel


Sleep, 900
WinActivate, %Title%
Send {Down}

WinActivate, ahk_class WinDbgFrameClass
WinWaitActive, ahk_class WinDbgFrameClass
Send, ^k
Sleep 500
Send {Return}
sleep 400

WinActivate, %Title%
Send {Enter}
WinActivate, ahk_class PuTTY
WinActivate, ahk_class ConsoleWindowClass
Sleep, 13000

IfWinExist, Command - Kernel 'com:port=\\.\pipe\%PipeName%
{
	WinActivate, Command - Kernel 'com:port=\\.\pipe\%PipeName%
	WinWaitActive, Command - Kernel 'com:port=\\.\pipe\%PipeName%
	Sleep 1500
	Send, ^{CtrlBreak}
	Sleep 5000
	WinActivate, Command - Kernel 'com:port=\\.\pipe\%PipeName%
	WinWaitActive, Command - Kernel 'com:port=\\.\pipe\%PipeName%
	;Send bp ntfs{!}NtfsWriteFile{Return}
	;Send bp ntfs{!}SetAttributeDataLength{Return}
	;Send bp ntfs{!}UpdateFileRecord{Return}
	;Send bp ntfs{!}NtfsFindMftRecord{Return}
	;Send bp ntfs{!}BrowseIndexEntries{+}0x85{Return}
	;Send bp ntfs{!}UpdateIndexEntrySize{Return}
	;Send bp ntfs{!}UpdateIndexEntrySize{+}0x97{Return}
	;Send bp ntfs{!}UpdateFileNameRecord{Return}
	;Send bp ntfs{!}DontAdvanceUpdateSequenceArray{Return}
	;Send bp ntfs{!}SetAttributeDataLength{Return}
	;Send bp ntfs{!}NtfsCreateFile{Return}
	;Send bp ntfs{!}NtfsAllocateClusters{Return}
	;Send bp ntfs{!}AddRun{Return}
	;Send bp ntfs{!}ConvertLargeMCBToDataRuns{Return}
	;Send bp ntfs{!}FreeClusters{Return}
	;Send bp ntfs{!}FreeClusters{+}0x6f9{Return}
	;Send bp ntfs{!}WriteAttribute{Return}
	;Send bp ntfs{!}CreateDummyKey{Return}
	;Send bp ntfs{!}NtfsCreateFileRecord{Return}
	;Send bp ntfs{!}NtfsSetInformation{Return}
	;Send bp ntfs{!}FreeClusters{Return}
	;Send bp ntfs{!}FindAttribute{Return}
	;Send bp ntfs{!}NtfsAddFilenameToDirectory{Return}
	;Send bp ntfs{!}NtfsFindMftRecord{Return}
	;Send bp ntfs{!}AddFileName{Return}
	;Send bp ntfs{!}IncreaseMftSize{Return}
	;Send bp ntfs{!}CreateBTreeFromIndex{Return}
	;Send bp ntfs{!}DestroyBTreeNode{Return}
	;Send bp ntfs{!}NtfsInsertKey{Return}
	;Send bp ntfs{!}CreateBTreeNodeFromIndexNode{Return}
	;Send bp ntfs{!}CreateBTreeNodeFromIndexNode{+}0x42a{Return}
	;Send bp ntfs{!}DumpBTreeNode{Return}
	;Send bp ntfs{!}InternalSetResidentAttributeLength{Return}
	;Send bp ntfs{!}UpdateIndexAllocation{Return}
	;Send bp ntfs{!}ntfsNtfsCreateFile{+}0x148{Return}
	;Send bp ntfs{!}SplitBTreeNode{Return}
	;Send bp ntfs{!}AddIndexAllocation{Return}
	;Send bp ntfs{!}UpdateMftMirror{Return}
	;Send bp ntfs{!}NtfsCreateDirectory{Return}
	
	Sleep 1000
	WinActivate, Command - Kernel 'com:port=\\.\pipe\%PipeName%
	WinWaitActive, Command - Kernel 'com:port=\\.\pipe\%PipeName%
	Send g{Return}
}

Sleep 5000
WinActivate, %Title%

/*
Click, 831, 567
*/
WinActivate, %Title%
Send {Enter}
Sleep 500
WinActivate, %Title%
Send {Enter}
ExitApp

Pause::
ExitApp
