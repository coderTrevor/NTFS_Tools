~^t::
text := Clipboard
Clipboard = if (!NT_SUCCESS(Status))`n{`n`tDPRINT1("ERROR: \n");`n}`n
Send ^v
Sleep 100
Send {Up}{Up}{End}{Left}{Left}{Left}{Left}{Left}
Clipboard := text