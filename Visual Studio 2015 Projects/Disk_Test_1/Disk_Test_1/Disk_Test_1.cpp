/*
* PROJECT:        ReactOS Supplemental Tester
* LICENSE:        GNU GPLv2 only as published by the Free Software Foundation
* PURPOSE:        Tests read\write functionality (of NTFS driver)
* PROGRAMMER:     Trevor Thompson
*
*/

#include "stdafx.h"
#include "conio.h"
#include "2239.h"
#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>


#define SUCCESS_STRING	"SUCCESS! :)"
#define SUCCESS_STRING2 "ANOTHER SUCCESS! :)"
#define SUCCESS_STRING3 "WOOHOO SUCCESS! :)"
#define SUCCESS_STRING4 "WOOHOO SUCCESS! :3 Yaaaaaaaaaay SUCCESS! :)"
#define STREAM_STRING   "This file contains additional data in the following named streams:\n\rShakespeare\n\rJunk"
#define SUCCESS_WINDOWS "\r\n\r\nThis string is appended to the end of the file.\r\nIf you run the tester in ReactOS and see this string in Windows, it means extending file sizes is finally working! :)\r\n"

#define FRAG_FILE1      L"Fragmented_Shakespeare.txt"
#define FRAG_FILE2      L"Fragmented_Junk.txt"
#define FRAG_FILE3      L"Fragmented_Junk_X.txt"

#define SPARSE_FRAG1    L"Fragmented_Sparse1.bin"
#define SPARSE_FRAG2    L"Fragmented_Sparse2.bin"
#define SPARSE_FRAG3    L"Fragmented_Sparse3.bin"

const int FILE_SIZE = 1024 * 8;

using std::cout;
using std::wcout;

void PrintLastError();

// returns false on success
bool ConfirmByteCount(DWORD intended, DWORD actual, bool write)
{
    if (intended == actual)
        return false;

    if (write)
        cout << "Error! Tried to write " << intended << " bytes but WriteFile indicated\n"
        << actual << " bytes were written!\n";
    else
        cout << "Error! Tried to read " << intended << " bytes but ReadFile indicated\n"
        << actual << " bytes were read!\n";

    PrintLastError();

    return true;
}

void AnyKey()
{
	std::cout << "Press any key to continue.\n";

	while (true)
	{
		if (_kbhit())
			break;
	}	
}

void PrintLastError()
{
	std::cout << "GetLastError: " << GetLastError() << "\n";
}

// write test string near the end of the file, so that the file 
// size doesn't change but the string is the last thing there
// returns false on success
bool WriteNearEnd(HANDLE hFile)
{
    bool failed = false;
	cout << "Testing writes relative to FILE_END...\n";

	SetFilePointer(hFile, -sizeof(SUCCESS_STRING2), NULL, FILE_END);
	DWORD byteCount = sizeof(SUCCESS_STRING2);
    if (!WriteFile(hFile, SUCCESS_STRING2, byteCount, &byteCount, NULL))
    {
        cout << "Error! WriteFile failed after setting file pointer relative to end!\n";
        failed = true;
        PrintLastError();
    }

    failed |= ConfirmByteCount(sizeof(SUCCESS_STRING2), byteCount, true);

	// did the write succeed?
	if (!failed)
		cout << "Apparent success!\n";
	
    return failed;
}


// Confirm that we can read the test string written to the end of the file
// returns false if successful
bool ReadNearEnd(HANDLE hFile)
{
	char ReadBuffer[sizeof(SUCCESS_STRING2)];
	memset(ReadBuffer, 'X', sizeof(SUCCESS_STRING2));

	// look for the SUCCESS_STRING2 at the end of the file
	cout << "Testing reads relative to FILE_END...\n";

	// move near the end of the file
	SetFilePointer(hFile, -sizeof(SUCCESS_STRING2), NULL, FILE_END);
	
	DWORD byteCount = sizeof(SUCCESS_STRING2);

	// do the read
	if (ReadFile(hFile, (LPVOID)ReadBuffer, byteCount, &byteCount, NULL))
	{
		// Did we get the right amount of data?
		if (byteCount != sizeof(SUCCESS_STRING2))
		{
			cout << "Reading failed! Only read " << byteCount << " bytes, was expecting " << sizeof(SUCCESS_STRING2) << ".\n";
			PrintLastError();

			cout << "Press any key to see the ReadBuffer\n";
			AnyKey();
			cout << "\n" << ReadBuffer << "\n";

			return true;
		}

		// Did we read what we expected to?
		if (memcmp(ReadBuffer, SUCCESS_STRING2, byteCount) == 0)
		{
			cout << "Success!\n";
			return false;
		}
		else
		{
			cout << "String didn't match! Was expecting:\n" << SUCCESS_STRING2 << "About to show you contents of ReadBuffer:\n";
			AnyKey();
			cout << "\n" << ReadBuffer << "\n";
			return true;
		}
	}
	
	cout << "ReadFile failed to execute!\n";
	PrintLastError();
	return true;
}


// returns false on success
bool WriteShakespeare(HANDLE hFile)
{
    cout << "Writing Shakespeare play...\n";

    bool failed = false;

    // write out the contents of the file
    DWORD written = 0;
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    if (!WriteFile(hFile, Shakespeare, (DWORD)Shakespeare_size, &written, NULL))
    {
        cout << "Error writing to file! " << written << " bytes written\n";
        PrintLastError();
        failed = true;
    }
    else
        failed |= ConfirmByteCount((DWORD)Shakespeare_size, written, true);

    return failed;
}

// this is similar to the way notepad.exe writes data
// returns false on success
bool WriteShakespeareLineByLine(HANDLE hFile, bool explicitFilePointer)
{
    if( explicitFilePointer )
        cout << "Writing Shakespeare play, line by line, setting file pointer explicitly...\n";
    else
        cout << "Writing Shakespeare play, line by line...\n";
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    
    bool failed = false;
    DWORD totalWritten = 0;

    char *CurrentBuffer = (char*)Shakespeare;

    DWORD BytesLeft = Shakespeare_size;
    char *BufferEnd;

    while (BytesLeft)
    {
        BufferEnd = (char*)memchr(CurrentBuffer + 1, '\n', BytesLeft);
        DWORD ToWrite = 0;

        // are we out of new lines?
        if (BufferEnd == NULL)
            ToWrite = BytesLeft;
        else
            ToWrite = BufferEnd - CurrentBuffer;
        
        if (explicitFilePointer)
            SetFilePointer(hFile, totalWritten, NULL, FILE_BEGIN);

        // write out the line
        DWORD written = 0;
        if (WriteFile(hFile, CurrentBuffer, ToWrite, &written, NULL))
        {
            totalWritten += written;
        }
        else
        {
            cout << "Error writing to file! " << written << " bytes written\n";
            PrintLastError();
            return true;
        }

        if (written == 0)
        {
            cout << "0 byte write detected. Tester failure!\n";
            return true;
        }

        CurrentBuffer = BufferEnd;
        BytesLeft -= written;
    }

    if (totalWritten != Shakespeare_size)
    {
        cout << "Error! Total bytes written reported as " << totalWritten << ", was expecting " << Shakespeare_size << "\n";
        failed = true;
        PrintLastError();
    }

    return failed;
}


// returns false on success
bool WriteShakespeareWordByWord(HANDLE hFile)
{
    cout << "Writing Shakespeare play, word by word...\n";
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

    DWORD totalWritten = 0;

    char *CurrentBuffer = (char*)Shakespeare;

    DWORD BytesLeft = Shakespeare_size;
    char *BufferEnd;

    bool failed = false;

    while (BytesLeft)
    {
        BufferEnd = (char*)memchr(CurrentBuffer + 1, ' ', BytesLeft);
        DWORD ToWrite = 0;

        // are we out of new lines?
        if (BufferEnd == NULL)
            ToWrite = BytesLeft;
        else
            ToWrite = BufferEnd - CurrentBuffer;

        // write out the line
        DWORD written = 0;
        if (WriteFile(hFile, CurrentBuffer, ToWrite, &written, NULL))
        {
            totalWritten += written;
        }
        else
        {
            cout << "Error writing to file. " << written << " bytes written\n";
            PrintLastError();
            return true;
        }

        if (written == 0)
        {
            cout << "0 byte write detected. Tester failure!\n";
            return true;
        }

        CurrentBuffer = BufferEnd;
        BytesLeft -= written;
    }

    if (totalWritten != Shakespeare_size)
    {
        cout << "Error! Total bytes written reported as " << totalWritten << ", was expecting " << Shakespeare_size << "\n";
        failed = true;
        PrintLastError();
    }

    return failed;
}

DWORD AdvanceShakespeareByWords(char *StartBuffer, char **NewPointer, int words, DWORD BytesLeft)
{
    //    DWORD BytesLeft = Shakespeare_size;
    char *BufferEnd;

    bool failed = false;
    char *CurrentBuffer = StartBuffer;
    for (int i = 0; i < words; i++)
    {
        BufferEnd = (char*)memchr(CurrentBuffer + 1, ' ', BytesLeft);
        CurrentBuffer = BufferEnd;
    }

    DWORD ToWrite = 0;

    // are we out of new lines?
    if (BufferEnd == NULL)
        ToWrite = BytesLeft;
    else
        ToWrite = BufferEnd - StartBuffer;

    *NewPointer = CurrentBuffer;

    return ToWrite;
}
DWORD PickOutShortWord(char *StartBuffer, char **NewPointer, int &CharactersFound, int maxCharacters, DWORD BytesLeft)
{
    char *BufferEnd;

    char *CurrentBuffer = StartBuffer;
    bool done = false;
    while(!done)
    {
        BufferEnd = (char*)memchr(CurrentBuffer + 1, ' ', BytesLeft);
        CharactersFound = (ULONG_PTR)BufferEnd - (ULONG_PTR)CurrentBuffer;
        if (CharactersFound > maxCharacters + 1)
            CurrentBuffer = BufferEnd;
        else
            done = true;
    }

    DWORD ToWrite = 0;

    // are we out of new lines?
    if (BufferEnd == NULL)
        ToWrite = BytesLeft;
    else
        ToWrite = BufferEnd - CurrentBuffer;

    *NewPointer = CurrentBuffer;

    return ToWrite;
}

// returns false on success
bool CreateABunchOfFiles(int NewFiles)
{
    int FilesLeft = NewFiles;

    cout << "Creating like " << FilesLeft << " files...\n";

    DWORD totalWritten = 0;

    char *CurrentBuffer = (char*)Shakespeare;
    char *CurrentExtensionBuffer = (char*)Shakespeare;

    DWORD BytesLeft = Shakespeare_size;
    DWORD ExtensionBytesLeft = Shakespeare_size;
    char *BufferEnd;
    char *ExtensionBufferEnd;

    bool failed = false;

    HANDLE hDirectory = CreateFile(TEXT("Output"),
                                   GENERIC_READ,
                                   FILE_SHARE_READ,
                                   NULL,
                                   OPEN_EXISTING,
                                   FILE_FLAG_BACKUP_SEMANTICS,
                                   NULL);
    if (hDirectory == INVALID_HANDLE_VALUE)
    {
        if (!CreateDirectory(TEXT("Output"), NULL))
        {
            cout << "Failed to open or create Output directory!\n";
            return true;
        }
    }
    else
        CloseHandle(hDirectory);

    if (!SetCurrentDirectory(TEXT("Output")))
    {
        printf("SetCurrentDirectory failed (%d)\n", GetLastError());
        return true;
    }

    while (BytesLeft > 10 && FilesLeft && ExtensionBytesLeft > 10)
    {
        DWORD ToWrite = AdvanceShakespeareByWords(CurrentBuffer, &BufferEnd, 1, BytesLeft);
        int extensionLength;
        DWORD ExtToWrite = PickOutShortWord(CurrentExtensionBuffer, &CurrentExtensionBuffer, extensionLength, 4, ExtensionBytesLeft);

        // create the file
        DWORD written = 0;

        // create the name string
        CHAR NameBuffer[MAX_PATH];
        ZeroMemory(NameBuffer, MAX_PATH);

        memcpy(NameBuffer, CurrentBuffer + 1, ToWrite - 1);
        memset(NameBuffer + ToWrite - 1, '.', 1);
        memcpy(NameBuffer + ToWrite, CurrentExtensionBuffer + 1, ExtToWrite - 1);

        //cout << "Filename: '" << NameBuffer << "'\n";

        int length = ExtToWrite + ToWrite - 1;

        // filename can't end with .
        while (NameBuffer[strlen(NameBuffer) - 1] == '.')
        {
            NameBuffer[strlen(NameBuffer) - 1] = 0;
            length--;
        }

        bool skip = false;
        if (strstr(NameBuffer, "*"))
            skip = true;
        if (strstr(NameBuffer, "/"))
            skip = true;
        if (strstr(NameBuffer, "\\"))
            skip = true;
        if (strstr(NameBuffer, ":"))
            skip = true;
        if (strstr(NameBuffer, "?"))
            skip = true;
        if (strstr(NameBuffer, "\""))
            skip = true;
        if (strstr(NameBuffer, "<"))
            skip = true;
        if (strstr(NameBuffer, ">"))
            skip = true;
        if (strstr(NameBuffer, "\n"))
            skip = true;
        if (strcmp(NameBuffer, ".txt") == 0)
            skip = true;

        if (skip)
        {
            CurrentBuffer = BufferEnd;
            BytesLeft -= written;
            continue;
        }

        //cout << "About to write " << NameBuffer << "...\n";

        // check if the file exists already
        HANDLE hFile = CreateFileA(NameBuffer, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_WRITE, NULL,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            /*if (!WriteFile(hFile, NameBuffer, length, &written, NULL))
            {
            cout << "Error writing to file. " << written << " bytes written\n";
            PrintLastError();
            return true;
            }*/
            //FilesLeft--;
            CloseHandle(hFile);
            //cout << NameBuffer << " already exists...\n";
            CurrentBuffer = BufferEnd;
            BytesLeft -= written;
            continue;
        }

        hFile = CreateFileA(NameBuffer, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            /*if (!WriteFile(hFile, NameBuffer, length, &written, NULL))
            {
            cout << "Error writing to file. " << written << " bytes written\n";
            PrintLastError();
            return true;
            }*/
            //FilesLeft--;
            CloseHandle(hFile);
            //cout << NameBuffer << " already exists...\n";
            CurrentBuffer = BufferEnd;
            BytesLeft -= written;
            continue;
        }

        hFile = CreateFileA(NameBuffer, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_WRITE, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            failed = true;
            cout << "Failed to create " << NameBuffer << "!\n";
            CurrentBuffer = BufferEnd;
            BytesLeft -= written;
            PrintLastError();
            return true;
        }

        //cout << "Created " << NameBuffer << ".\n";

        if (!WriteFile(hFile, NameBuffer, length, &written, NULL))
        {
            cout << "Error writing to file. " << written << " bytes written\n";
            PrintLastError();
            return true;
        }
        FilesLeft--;

        if (written == 0)
        {
            cout << "0 byte write detected. Tester failure!\n";
            return true;
        }

        CloseHandle(hFile);
        CurrentBuffer = BufferEnd;
        CurrentExtensionBuffer = (char*)((ULONG_PTR)CurrentExtensionBuffer + extensionLength);
        BytesLeft -= written;
    }

    if (!SetCurrentDirectory(TEXT("..\\")))
    {
        printf("SetCurrentDirectory failed (%d)\n", GetLastError());
        return true;
    }

    if (FilesLeft != 0)
    {
        cout << "Error: " << FilesLeft << " files left\n";
        return true;
    }
    //cout << BytesLeft << " bytes left\n";

    return false;
}
// returns false on success
bool CreateABunchOfFolders(int NewFolders)
{
    int FilesLeft = NewFolders;

    cout << "Creating like " << FilesLeft << " folders...\n";

    DWORD totalWritten = 0;

    char *CurrentBuffer = (char*)Shakespeare;
    char *CurrentExtensionBuffer = (char*)Shakespeare;

    DWORD BytesLeft = Shakespeare_size;
    DWORD ExtensionBytesLeft = Shakespeare_size;
    char *BufferEnd;
    char *ExtensionBufferEnd;

    bool failed = false;

    HANDLE hDirectory = CreateFile(TEXT("Output"),
                                   GENERIC_READ,
                                   FILE_SHARE_READ,
                                   NULL,
                                   OPEN_EXISTING,
                                   FILE_FLAG_BACKUP_SEMANTICS,
                                   NULL);
    if (hDirectory == INVALID_HANDLE_VALUE)
    {
        if (!CreateDirectory(TEXT("Output"), NULL))
        {
            cout << "Failed to open or create Output directory!\n";
            return true;
        }
    }
    else
        CloseHandle(hDirectory);

    if (!SetCurrentDirectory(TEXT("Output")))
    {
        printf("SetCurrentDirectory failed (%d)\n", GetLastError());
        return true;
    }

    while (BytesLeft > 10 && FilesLeft && ExtensionBytesLeft > 10)
    {
        DWORD ToWrite = AdvanceShakespeareByWords(CurrentBuffer, &BufferEnd, 1, BytesLeft);
        int extensionLength;
        DWORD ExtToWrite = PickOutShortWord(CurrentExtensionBuffer, &CurrentExtensionBuffer, extensionLength, 4, ExtensionBytesLeft);

        // create the file
        DWORD written = 0;

        // create the name string
        CHAR NameBuffer[MAX_PATH];
        ZeroMemory(NameBuffer, MAX_PATH);

        memcpy(NameBuffer, CurrentBuffer + 1, ToWrite - 1);
        memset(NameBuffer + ToWrite - 1, '.', 1);
        memcpy(NameBuffer + ToWrite, CurrentExtensionBuffer + 1, ExtToWrite - 1);

        //cout << "Filename: '" << NameBuffer << "'\n";

        int length = ExtToWrite + ToWrite - 1;

        // filename can't end with .
        while (NameBuffer[strlen(NameBuffer) - 1] == '.')
        {
            NameBuffer[strlen(NameBuffer) - 1] = 0;
            length--;
        }

        bool skip = false;
        if (strstr(NameBuffer, "*"))
            skip = true;
        if (strstr(NameBuffer, "/"))
            skip = true;
        if (strstr(NameBuffer, "\\"))
            skip = true;
        if (strstr(NameBuffer, ":"))
            skip = true;
        if (strstr(NameBuffer, "?"))
            skip = true;
        if (strstr(NameBuffer, "\""))
            skip = true;
        if (strstr(NameBuffer, "<"))
            skip = true;
        if (strstr(NameBuffer, ">"))
            skip = true;
        if (strstr(NameBuffer, "\n"))
            skip = true;
        if (strcmp(NameBuffer, ".txt") == 0)
            skip = true;

        if (skip)
        {
            CurrentBuffer = BufferEnd;
            BytesLeft -= written;
            continue;
        }

        //cout << "About to write " << NameBuffer << "...\n";

        // check if the file exists already
        HANDLE hFile = CreateFileA(NameBuffer, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_WRITE, NULL,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            /*if (!WriteFile(hFile, NameBuffer, length, &written, NULL))
            {
            cout << "Error writing to file. " << written << " bytes written\n";
            PrintLastError();
            return true;
            }*/
            //FilesLeft--;
            CloseHandle(hFile);
            //cout << NameBuffer << " already exists...\n";
            CurrentBuffer = BufferEnd;
            BytesLeft -= written;
            continue;
        }

        hFile = CreateFileA(NameBuffer, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            /*if (!WriteFile(hFile, NameBuffer, length, &written, NULL))
            {
            cout << "Error writing to file. " << written << " bytes written\n";
            PrintLastError();
            return true;
            }*/
            //FilesLeft--;
            CloseHandle(hFile);
            //cout << NameBuffer << " already exists...\n";
            CurrentBuffer = BufferEnd;
            BytesLeft -= written;
            continue;
        }

        if( !CreateDirectoryA(NameBuffer, NULL) )
        {
            failed = true;
            cout << "Failed to create " << NameBuffer << " folder!\n";
            CurrentBuffer = BufferEnd;
            BytesLeft -= written;
            PrintLastError();
            return true;
        }

        //cout << "Created " << NameBuffer << ".\n";

        /*if (!WriteFile(hFile, NameBuffer, length, &written, NULL))
        {
            cout << "Error writing to file. " << written << " bytes written\n";
            PrintLastError();
            return true;
        }*/
        FilesLeft--;

        /*if (written == 0)
        {
            cout << "0 byte write detected. Tester failure!\n";
            return true;
        }*/

        //CloseHandle(hFile);
        CurrentBuffer = BufferEnd;
        CurrentExtensionBuffer = (char*)((ULONG_PTR)CurrentExtensionBuffer + extensionLength);
        BytesLeft -= written;
    }

    if (!SetCurrentDirectory(TEXT("..\\")))
    {
        printf("SetCurrentDirectory failed (%d)\n", GetLastError());
        return true;
    }

    if (FilesLeft != 0)
    {
        cout << "Error: " << FilesLeft << " files left\n";
        return true;
    }
    //cout << BytesLeft << " bytes left\n";

    return false;
}

// returns false on success
bool BuildVerificationFile()
{
    int Files = 0;

    DWORD totalWritten = 0;

    char *CurrentBuffer = (char*)Shakespeare;

    DWORD BytesLeft = Shakespeare_size;
    char *BufferEnd;

    bool failed = false;


    HANDLE hOutputFile = CreateFile(TEXT("OutputFile.txt"),
                                    GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_ARCHIVE,
                                    NULL);
    if (hOutputFile == INVALID_HANDLE_VALUE)
    {
        cout << "Failed to create OutputFile.txt!\n";
        return true;
    }

    if (!SetCurrentDirectory(TEXT("Output")))
    {
        printf("SetCurrentDirectory failed (%d)\n", GetLastError());
        return true;
    }

    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(TEXT(".\\*"), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        cout << "Couldn't find a single file!\n";
    }

    do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // _tprintf(TEXT("  %s   <DIR>\n"), ffd.cFileName);
        }
        else
        {
            LARGE_INTEGER filesize;
            filesize.LowPart = ffd.nFileSizeLow;
            filesize.HighPart = ffd.nFileSizeHigh;
            //_tprintf(TEXT("'%s'\n"), ffd.cFileName);

            // convert ffd.cFileName to string
            char FileName[MAX_PATH + 1];
            ZeroMemory(FileName, MAX_PATH + 1);
            wcstombs(FileName, ffd.cFileName, MAX_PATH);
            memset(FileName + strlen(FileName), '\n', 1);

            DWORD written;

            // write the file name to OutputFile.txt
            if (!WriteFile(hOutputFile, FileName, strlen(FileName), &written, NULL))
            {
                cout << "Unable to write to OutputFile.txt!\n";
                PrintLastError();
                return true;
            }


            Files++;
        }
    } while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);

    CloseHandle(hOutputFile);

    if (!SetCurrentDirectory(TEXT("..\\")))
    {
        printf("SetCurrentDirectory failed (%d)\n", GetLastError());
        return true;
    }

    cout << "Found " << Files << " files.\n";
    return false;
}



// returns false on success
bool VerifyABunchOfFiles()
{
    int Files = 0;

    DWORD totalWritten = 0;

    char *CurrentBuffer = (char*)Shakespeare;

    DWORD BytesLeft = Shakespeare_size;
    char *BufferEnd;

    bool failed = false;


    HANDLE hOutputFile = CreateFile(TEXT("OutputFile.txt"),
                                    GENERIC_READ,
                                    FILE_SHARE_READ,
                                    NULL,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_ARCHIVE,
                                    NULL);
    if (hOutputFile == INVALID_HANDLE_VALUE)
    {
        cout << "Failed to create OutputFile.txt!\n";
        return true;
    }

    if (!SetCurrentDirectory(TEXT("Output")))
    {
        printf("SetCurrentDirectory failed (%d)\n", GetLastError());
        return true;
    }

    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(TEXT(".\\*"), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        cout << "Couldn't find a single file!\n";
    }

    do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // _tprintf(TEXT("  %s   <DIR>\n"), ffd.cFileName);
        }
        else
        {
            LARGE_INTEGER filesize;
            filesize.LowPart = ffd.nFileSizeLow;
            filesize.HighPart = ffd.nFileSizeHigh;
            _tprintf(TEXT(".\\'%s'\n"), ffd.cFileName);

            // convert ffd.cFileName to string
            char FileName[MAX_PATH + 1];
            char LoggedFileName[MAX_PATH + 1];
            ZeroMemory(FileName, MAX_PATH + 1);
            ZeroMemory(LoggedFileName, MAX_PATH + 1);
            wcstombs(FileName, ffd.cFileName, MAX_PATH);
            //memset(FileName + strlen(FileName), '\n', 1);

            DWORD bytesRead;

            //memset(FileName + strlen(FileName) - 1, 0, 1);
            //

            if (!ReadFile(hOutputFile, LoggedFileName, filesize.LowPart + 1, &bytesRead, NULL))
            {
                cout << "Unable to read " << FileName << " from OutputFile.txt!\n";
            }

            memset(LoggedFileName + strlen(LoggedFileName) - 1, 0, 1);

            cout << "  '" << LoggedFileName << "'\n";

            if (bytesRead - 1 != strlen(FileName))
            {
                cout << "Error! BytesRead != strlen(FileName)!\n";
                cout << "BytesRead: " << bytesRead << "\n";
                cout << "strlen: " << strlen(FileName) << "\n";
                return true;
            }

            if (memcmp(FileName, LoggedFileName, bytesRead) != 0)
            {
                cout << "Error: Current Filename, '" << FileName << "' doesn't match '" << LoggedFileName << "'expected from log.\n";
            }


            // does FileName match ex

            // open the file being pointed to
            HANDLE hCurrentFile = CreateFileA(FileName, FILE_GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
            if (hCurrentFile == INVALID_HANDLE_VALUE)
            {
                cout << "Failed to open " << FileName << "!\n";
                return true;
            }

            if (GetFileSize(hCurrentFile, NULL) != strlen(FileName))
            {
                cout << "Size of " << FileName << " doesn't match expected value!\n";
                return true;
            }

            if(filesize.LowPart != strlen(FileName))
            {
                cout << "Size of " << FileName << " doesn't match expected value from directory!\n";
                return true;
            }

            char FileBuffer[MAX_PATH];

            DWORD bytesReadOfCurrentFile;
            if (!ReadFile(hCurrentFile, FileBuffer, strlen(FileName), &bytesReadOfCurrentFile, NULL))
            {
                cout << "Failed to read " << FileName << "!\n";
                return true;
            }

            if (memcmp(FileBuffer, FileName, strlen(FileName)) != 0)
            {
                cout << "Read unexpected data from " << FileName << "!\n";
                return true;
            }

            CloseHandle(hCurrentFile);

            Files++;
        }
    } while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);

    CloseHandle(hOutputFile);

    cout << "Found " << Files << " files.\n";
    return false;
}


// returns false on success
bool WriteShakespeareCharByChar(HANDLE hFile)
{
    cout << "Writing Shakespeare play, one character at a time...\n";
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

    DWORD totalWritten = 0;

    char *CurrentBuffer = (char*)Shakespeare;

    DWORD BytesLeft = Shakespeare_size;
    char *BufferEnd;

    while (BytesLeft)
    {
        BufferEnd = CurrentBuffer + 1;
        DWORD ToWrite = 1;

        // write out the character
        DWORD written = 0;
        if (WriteFile(hFile, CurrentBuffer, ToWrite, &written, NULL))
        {
            totalWritten += written;
        }
        else
        {
            cout << "Error writing to file! " << written << " bytes written\n";
            PrintLastError();
            return true;
        }

        if (written == 0)
        {
            cout << "0 byte write detected. Tester failure!\n";
            return true;
        }

        CurrentBuffer = BufferEnd;
        BytesLeft -= written;
    }


    if (totalWritten != Shakespeare_size)
    {
        cout << "Error! Total bytes written reported as " << totalWritten << ", was expecting " << Shakespeare_size << "\n";
        PrintLastError();
        return true;
    }

    return false;
}



// Creates a new file for writing, 2239.txt and returns the handle
HANDLE MakeShakeSpearFile()
{
    cout << "Opening Shakespeare play...\n";

    // open the file (or create it)
    HANDLE hFile = CreateFile(_T("2239.txt"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_WRITE, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Couldn't open 2239.txt, creating it...\n";

        hFile = CreateFile(_T("2239.txt"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_WRITE, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Couldn't create 2239.txt!\n";
        PrintLastError();
        AnyKey();
        return INVALID_HANDLE_VALUE;
    }

    // write out the contents of the file
    WriteShakespeare(hFile);

    return hFile;
}

bool RuinShakespeare(HANDLE hFile)
{
    bool failed = false;
    cout << "Writing junk data...\n";

    // get a buffer the same size as our text file
    char Buffer[Shakespeare_size];
    
    // fill it with ` (2239.txt doesn't have this character)
    memset(Buffer, '`', Shakespeare_size);

    // write it out
    DWORD written = 0;
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    if (WriteFile(hFile, Buffer, (DWORD)Shakespeare_size, &written, NULL))
    {
        if (!ConfirmByteCount((DWORD)Shakespeare_size, written, true))
        {
            //cout << "Apparent Success!\n";
            return false;
        }
    }
    else
    {
        cout << "Error. " << written << " bytes written\n";
        PrintLastError();
    }

    return true;
}

// ensures entire file has been overwritten with ` characters
// returns false if successful
bool ReadJunk(HANDLE hFile, int MemSize = Shakespeare_size)
{
    DWORD Read;
    char *ReadBuffer = (char*)malloc(MemSize);

    cout << "Ensuring file is currently junk...\n";

    // define a junk buffer for comparison
    char *junkBuffer = (char*)malloc(MemSize);
    memset(junkBuffer, '`', MemSize);

    // do the read
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    if (ReadFile(hFile, ReadBuffer, (DWORD)MemSize, &Read, NULL))
    {
        bool failed = ConfirmByteCount((DWORD)MemSize, Read, false);

        // do the comparison
        if (memcmp(ReadBuffer, junkBuffer, MemSize) == 0)
        {
            if (!failed)
                cout << "Success!\n";
            free(junkBuffer);
            free(ReadBuffer);
            return failed;
        }

        cout << "Failure! Didn't read expected data!\n";
        PrintLastError();
        free(junkBuffer);
        free(ReadBuffer);
        return true;
    }

    cout << "Error executing ReadFile()\n";
    PrintLastError();
    free(junkBuffer);
    free(ReadBuffer);

    return true;
}

// ensures entire file has been overwritten with X characters
// returns false if successful
bool ReadJunkX(HANDLE hFile, int MemSize = Shakespeare_size)
{
    DWORD Read;
    char *ReadBuffer = (char*)malloc(MemSize);

    cout << "Ensuring file is currently junk of X's...\n";

    // define a junk buffer for comparison
    char *junkBuffer = (char*)malloc(MemSize);
    memset(junkBuffer, 'X', MemSize);

    // do the read
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    ReadFile(hFile, ReadBuffer, (DWORD)MemSize, &Read, NULL);

    // do the comparison
    if (memcmp(ReadBuffer, junkBuffer, MemSize) == 0)
    {
        cout << "Success!\n";
        free(junkBuffer);
        free(ReadBuffer);
        return false;
    }

    cout << "Failure! Didn't read expected data!\n";

    free(junkBuffer);
    free(ReadBuffer);
    return true;
}



// ensures entire file has matches the Shakespeare play
// returns false if successful
bool ReadShakespeare(HANDLE hFile, int MemSize = Shakespeare_size)
{
    DWORD Read;
    BYTE *ReadBuffer = (BYTE *)malloc(MemSize);
    memset(ReadBuffer, '`', MemSize);

    cout << "Ensuring file is currently brilliant...\n";
    // do the read
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    if (ReadFile(hFile, ReadBuffer, (DWORD)MemSize, &Read, NULL))
    {

        // do the comparison
        if (memcmp(ReadBuffer, Shakespeare, MemSize) == 0)
        {
            if (!ConfirmByteCount(MemSize, Read, false))
            {
                cout << "Success!\n";
                return false;
            }
            return true;
        }

        cout << "Failure! Didn't read expected data!\n";
        return true;
    }
    
    cout << "Error! Couldn't execute ReadFile()\n";
    PrintLastError();

    return true;
}


// ensures entire file has matches the Shakespeare play, but reads a line at a time
// returns false if successful
bool ReadShakespeareLineByLine(HANDLE hFile, bool SetPointerExplicitly)
{
    DWORD Read;
    BYTE ReadBuffer[Shakespeare_size];
    memset(ReadBuffer, '`', Shakespeare_size);

    if (SetPointerExplicitly)
        cout << "Reading text file line by line, with explicit SetFilePointer calls...\n";
    else
        cout << "Reading text file line by line...\n";

    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    DWORD BytesLeft = Shakespeare_size;
    DWORD Offset = 0;

    char * curLine = (char*)Shakespeare;

    while (BytesLeft)
    {
        DWORD lineSize;

        // find the start of the next line
        char * nextLine = strchr(curLine, '\n');

        // find the length
        if (nextLine == NULL)
            lineSize = BytesLeft;
        else
            lineSize = (nextLine - curLine) + 1;

        if( SetPointerExplicitly )
            SetFilePointer(hFile, Offset, NULL, FILE_BEGIN);

        // do the read
        if (!ReadFile(hFile, ReadBuffer + Offset, lineSize, &Read, NULL))
        {
            cout << "Failed to read file one line at a time, starting at offset "
                 << Offset << "!\n";
            return true;
        }

        if (lineSize != Read)
        {
            cout << "ReadFile() reported different byte count than expected.\nRead " << Read << " bytes "
                << "but was expecting " << lineSize << ".\n";
            return true;
        }
        
        BytesLeft -= Read;
        Offset += lineSize;
        curLine = nextLine + 1;
    }

    // do the comparison
    if (memcmp(ReadBuffer, Shakespeare, Shakespeare_size) == 0)
    {
        cout << "Success!\n";
        return false;
    }

    cout << "Failure! Didn't read expected data!\n";

    return true;
}



HANDLE MakeTestFile()
{
	char Buffer[FILE_SIZE];
	
	std::cout << "Setting up TestText.txt...\n";
	
    // We want the file to be full of X's
	memset(Buffer, 'X', FILE_SIZE);

    // ...except for one test string, located at the halfway point
	memcpy(Buffer + (FILE_SIZE / 2), "SUCCESS! :)", sizeof("SUCCESS! :)"));

    // open the test file
	HANDLE hFile = CreateFile(_T("TestText.txt"),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
        OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        // create the test file
        hFile = CreateFile(_T("TestText.txt"),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    }

	// Did we fail to open or create the file?
	if (hFile == INVALID_HANDLE_VALUE)
	{
		std::cout << "Error creating TestText.txt!\nPlease run this program in Windows first!\n";	
		PrintLastError();// "Error occurred");
		AnyKey();

		return INVALID_HANDLE_VALUE;
	}

	DWORD ByteCount;

    // write out the file's contents
    if (!WriteFile(hFile, Buffer, FILE_SIZE, &ByteCount, NULL))
    {
        cout << "Couldn't write to TestText.txt!\n";
        PrintLastError();
        return INVALID_HANDLE_VALUE;
    }

	std::cout << "Wrote to TestText.txt\n";

	return hFile;
}


HANDLE OpenOrCreateTestText()
{
    /*std::cout << "Opening TestText.txt\n";

    // Attempt to open the text file
    HANDLE hFile = CreateFile(_T("TestText.txt"),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        //CREATE_NEW,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        std::cout << "Error Opening File:\n";

        PrintLastError();
        std::cout << "About to create TestText.txt...\n";
        AnyKey();

        // couldn't open the text file, try creating one
        MakeTestFile();
        return INVALID_HANDLE_VALUE;
    }

    return hFile;*/
    return MakeTestFile();
}

// returns false if successful
bool TestStraddlingReads(HANDLE hFile)
{
    DWORD ByteCount;

    char Buffer[FILE_SIZE];             // Buffer to hold the contents of the read
    memset(Buffer, '0', FILE_SIZE);     // zero it to start with

    std::cout << "Testing reads straddling a " << (FILE_SIZE / 2) << "-byte boundary:\n";

    // advance the file pointer to right before the halfway point
    SetFilePointer(hFile, (FILE_SIZE / 2 - 9), NULL, FILE_BEGIN);

    // read 24 bytes (probably across a sector boundary)
    if (!ReadFile(hFile, Buffer, 24, &ByteCount, NULL))
    {
        std::cout << "Error:\n";
        PrintLastError();// "Error occurred");
    }

    bool failed = false;

    if (ByteCount != 24)
    {
        cout << "Error! Expected to read 24 bytes from offset " << (FILE_SIZE / 2 - 9) << " but ReadFile indicated\n"
            << ByteCount << " bytes were read!\n";
        failed = true;
    }

    //std::cout << ByteCount << " bytes read.\n";
    //std::cout << Buffer << "\n";

    // could we read the SUCCESS_STRING?
    if (strcmp(SUCCESS_STRING, Buffer + 9) != 0)
    {
        std::cout << "Error! Failed to read data straddling " << (FILE_SIZE / 2) << "-byte boundary!\n";
        failed = true;
    }

    if( !failed )
        std::cout << "Success!\n";

    return failed;
}


// returns false if successful
bool TestStraddlingWrites(HANDLE hFile)
{
    DWORD ByteCount;

    std::cout << "Testing writes straddling a " << (FILE_SIZE / 2) << "-byte boundary:\n";

    // advance the file pointer to right before the halfway point
    DWORD offset = (FILE_SIZE / 2 - 9);
    SetFilePointer(hFile, offset, NULL, FILE_BEGIN);

    // Write a lengthy string (across a sector boundary)
    if (!WriteFile(hFile, SUCCESS_STRING4, sizeof(SUCCESS_STRING4), &ByteCount, NULL))
    {
        std::cout << "Error:\n";
        PrintLastError();
        return true;
    }

    bool failed = false;

    /*if (ByteCount != sizeof(SUCCESS_STRING4))
    {
        cout << "Error! Tried to write " << sizeof(SUCCESS_STRING4) << " bytes but WriteFile indicated\n"
            << ByteCount << " bytes were written!\n";
        failed = true;
    }*/

    failed |= ConfirmByteCount(sizeof(SUCCESS_STRING4), ByteCount, true);

    SetFilePointer(hFile, offset, NULL, FILE_BEGIN);

    char Buffer[sizeof(SUCCESS_STRING4)];             // Buffer to hold the contents of the read
    memset(Buffer, '0', sizeof(SUCCESS_STRING4));     // zero it to start with

    if (!ReadFile(hFile, Buffer, sizeof(SUCCESS_STRING4), &ByteCount, NULL))
    {
        cout << "Unable to read from file!\n";
        PrintLastError();
        return true;
    }
    
    failed |= ConfirmByteCount(sizeof(SUCCESS_STRING4), ByteCount, false);

    // could we read the SUCCESS_STRING4?
    if (strcmp(SUCCESS_STRING4, Buffer) != 0)
    {
        std::cout << "Failed to read back data straddling " << (FILE_SIZE / 2) << "-byte boundary!\n";
        failed = true;
    }

    if( !failed)
        std::cout << "Success!\n";

    return failed;
}

// returns false on success
bool PerformShakeSpeareTests()
{
    bool Failed = false;

    // Create the shakespeare file, write junk to it, then data, then validate it
    // First, create the file
    HANDLE hFile = MakeShakeSpearFile();
    if (hFile == INVALID_HANDLE_VALUE)
        return true;
    // now, write junk to it
    Failed |= RuinShakespeare(hFile);
    // ensure we ruined it
    Failed |= ReadJunk(hFile);
    // fix it
    Failed |= WriteShakespeare(hFile);
    // now make sure we fixed it
    Failed |= ReadShakespeare(hFile);


    // Now test writing to the file one line at a time:
    // ruin some Shakespeare
    Failed |= RuinShakespeare(hFile);
    // ensure we ruined it
    Failed |= ReadJunk(hFile);
    // write shakespeare, line by line
    Failed |= WriteShakespeareLineByLine(hFile, false);
    // now make sure we wrote it
    Failed |= ReadShakespeare(hFile);


    // Now test writing to the file one line at a time, with explicit file pointer setting:
    // ruin some Shakespeare
    Failed |= RuinShakespeare(hFile);
    // ensure we ruined it
    Failed |= ReadJunk(hFile);
    // write shakespeare, line by line
    Failed |= WriteShakespeareLineByLine(hFile, true);
    // now make sure we wrote it
    Failed |= ReadShakespeare(hFile);


    // Now test writing to the file one word at a time:
    // ruin some Shakespeare
    Failed |= RuinShakespeare(hFile);
    // ensure we ruined it
    Failed |= ReadJunk(hFile);
    // write shakespeare, line by line
    Failed |= WriteShakespeareWordByWord(hFile);
    // now make sure we wrote it
    Failed |= ReadShakespeare(hFile);


    // Now test writing one character at a time:
    // ruin some Shakespeare
    // This test takes forever! and it has yet to find any bugs
    /*Failed |= RuinShakespeare(hFile);
    // ensure we ruined it
    Failed |= ReadJunk(hFile);
    // write shakespeare, one character at a time
    Failed |= WriteShakespeareCharByChar(hFile);
    // now make sure we wrote it
    Failed |= ReadShakespeare(hFile);*/

    // now read the file line-by-line
    Failed |= ReadShakespeareLineByLine(hFile, false);
    // now read the file line-by-line, explicitly setting the file pointer
    Failed |= ReadShakespeareLineByLine(hFile, true);
    

    CloseHandle(hFile);

    return Failed;
}

// returns false on success
bool PerformBasicTests()
{
    char Buffer[FILE_SIZE];
    memset(Buffer, '0', FILE_SIZE);

    HANDLE hFile = OpenOrCreateTestText();

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Error opening or creating 2239.txt: \n";
        PrintLastError();
        AnyKey();
        return true;
    }

    bool Failed = false;

    Failed |= TestStraddlingReads(hFile);

    Failed |= TestStraddlingWrites(hFile);

    cout << "Performing write test relative to FILE_END...\n";
    Failed |= WriteNearEnd(hFile);

    Failed |= ReadNearEnd(hFile);

    CloseHandle(hFile);

    return Failed;
}

DWORD GetClusterSize();

// Try to test writing to files in a way that will fragment them.
// I'm not sure there's any way to force or guarantee the fsd is actually writing
// the files in a fragmented manner, but that's the condition we're trying to create.
// You can confirm fragmentation by hex editing the disk (winhex works well)
// One file is shakespeare, the other is X's, another is `'s. 
// Search for a bunch of X's, then scroll through to confirm that the files are
// written with an interleaved pattern; you can't miss em.
// returns false if successful
bool PerformInterleavedWriteTests()
{
    HANDLE hShakespeare, hJunk, hJunkX;

    cout << "Testing interleaved writes (file fragmentation)\n";

    // try to open the Shakespeare file
    hShakespeare = CreateFile(FRAG_FILE1,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING, NULL);

    // if that failed
    if (hShakespeare == INVALID_HANDLE_VALUE)
    {
        std::wcout << L"Unable to Open " << FRAG_FILE1 << L", trying to create it...\n";

        // try to create the Shakespeare file
        hShakespeare = CreateFile(FRAG_FILE1,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING, NULL);
    }

    if (hShakespeare == INVALID_HANDLE_VALUE)
    {
        std::wcout << L"Unable to Create " << FRAG_FILE1 << L"\n";
        cout << "Please run this tester on Windows first.\n";
        PrintLastError();
        AnyKey();
        return true;
    }

    // open the Junk 1 file
    hJunk = CreateFile(FRAG_FILE2,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);

    if (hJunk == INVALID_HANDLE_VALUE)
    {
        std::wcout << L"Unable to open " << FRAG_FILE2 << L", trying to create it...\n";
        hJunk = CreateFile(FRAG_FILE2,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
    }

    if (hJunk == INVALID_HANDLE_VALUE)
    {
        std::wcout << L"Unable to Create " << FRAG_FILE2 << L"\n";
        PrintLastError();
        AnyKey();
        CloseHandle(hShakespeare);
        return true;
    }

    // open the junkx file
    hJunkX = CreateFile(FRAG_FILE3,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);

    if (hJunkX == INVALID_HANDLE_VALUE)
    {
        std::wcout << L"Unable to open " << FRAG_FILE3 << L", trying to create it...\n";

        hJunkX = CreateFile(FRAG_FILE3,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
    }

    if (hJunkX == INVALID_HANDLE_VALUE)
    {
        std::wcout << L"Unable to create " << FRAG_FILE3 << L"\n\t\tPlease run this program on Windows first!\n";
        PrintLastError();
        AnyKey();
        CloseHandle(hShakespeare);
        CloseHandle(hJunk);
        return true;
    }

    DWORD BytesLeft = Shakespeare_size / 4;

    char *Shakesbuff = (char*)Shakespeare;

    bool failed = false;

    //use the drive's cluster size from the first test
    DWORD FragmentSize = GetClusterSize();
    DWORD Written = 0;

    // fill a buffer with ` characters
    void *JunkBuffer = malloc(FragmentSize);
    memset(JunkBuffer, '`', FragmentSize);

    // fill a buffer with X's
    void *JunkXBuffer = malloc(FragmentSize);
    memset(JunkXBuffer, 'X', FragmentSize);

    cout << "Writing to three files in a round-robin pattern...\n";

    while (BytesLeft > FragmentSize)
    {
        // write shakespeare
        if (!WriteFile(hShakespeare, Shakesbuff, FragmentSize, &Written, NULL))
        {
            cout << "Error writing to shakespeare file!\n";
            PrintLastError();
            AnyKey();
            CloseHandle(hShakespeare);
            CloseHandle(hJunk);
            CloseHandle(hJunkX);
            free(JunkBuffer);
            free(JunkXBuffer);
            return true;
        }

        failed |= ConfirmByteCount(FragmentSize, Written, true);

        // write junk `
        if (!WriteFile(hJunk, JunkBuffer, FragmentSize, &Written, NULL))
        {
            cout << "Error writing to fragment file 1!\n";
            PrintLastError();
            AnyKey();
            CloseHandle(hShakespeare);
            CloseHandle(hJunk);
            CloseHandle(hJunkX);
            free(JunkBuffer);
            free(JunkXBuffer);
            return true;
        }

        failed |= ConfirmByteCount(FragmentSize, Written, true);

        // write junk X's
        if (!WriteFile(hJunkX, JunkXBuffer, FragmentSize, &Written, NULL))
        {
            cout << "Error writing to fragment file 2!\n";
            PrintLastError();
            AnyKey();
            CloseHandle(hShakespeare);
            CloseHandle(hJunk);
            CloseHandle(hJunkX);
            free(JunkBuffer);
            free(JunkXBuffer);
            return true;
        }

        failed |= ConfirmByteCount(FragmentSize, Written, true);

        Shakesbuff += FragmentSize;
        BytesLeft  -= FragmentSize;
    }

    // finish up the write
    // we're writing directly to the disk, so our writes need to be sector-aligned
    void *LastBit = malloc(FragmentSize);
    memset(LastBit, 0, FragmentSize);

    memcpy(LastBit, Shakesbuff, BytesLeft);

    // write shakespeare
    if (!WriteFile(hShakespeare, LastBit, FragmentSize, &Written, NULL))
    {
        std::wcout << L"Error writing to " << FRAG_FILE1 << L"!\n";
        PrintLastError();
        AnyKey();
        free(LastBit);
        CloseHandle(hShakespeare);
        CloseHandle(hJunk);
        CloseHandle(hJunkX);
        free(JunkBuffer);
        free(JunkXBuffer);
        return true;
    }
    
    failed |= ConfirmByteCount(FragmentSize, Written, true);

    // write junk `
    if (!WriteFile(hJunk, JunkBuffer, FragmentSize, &Written, NULL))
    {
        std::wcout << L"Error writing to " << FRAG_FILE2 << L"!\n";
        PrintLastError();
        AnyKey();
        free(LastBit);
        CloseHandle(hShakespeare);
        CloseHandle(hJunk);
        CloseHandle(hJunkX);
        free(JunkBuffer);
        free(JunkXBuffer);
        return true;
    }

    failed |= ConfirmByteCount(FragmentSize, Written, true);

    // write junk X's
    if (!WriteFile(hJunkX, JunkXBuffer, FragmentSize, &Written, NULL))
    {
        std::wcout << L"Error writing to " << FRAG_FILE3 << L"!\n";
        PrintLastError();
        AnyKey();
        free(LastBit);
        CloseHandle(hShakespeare);
        CloseHandle(hJunk);
        CloseHandle(hJunkX);
        free(JunkBuffer);
        free(JunkXBuffer);
        return true;
    }

    failed |= ConfirmByteCount(FragmentSize, Written, true);

    free(JunkBuffer);
    free(JunkXBuffer);
    free(LastBit);

    CloseHandle(hJunkX);
    CloseHandle(hJunk);
    CloseHandle(hShakespeare);

    // open the Shakespeare file
    hShakespeare = CreateFile(FRAG_FILE1,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);

    if (hShakespeare == INVALID_HANDLE_VALUE)
    {
        std::wcout << L"Unable to Create " << FRAG_FILE1 << L"\n";
        PrintLastError();
        AnyKey();
        return true;
    }

    // open the Junk 1 file
    hJunk = CreateFile(FRAG_FILE2,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);


    if (hJunk == INVALID_HANDLE_VALUE)
    {
        std::wcout << L"Unable to Create " << FRAG_FILE2 << L"\n";
        PrintLastError();
        AnyKey();
        CloseHandle(hShakespeare);
        return true;
    }

    // open the Shakespeare file
    hJunkX = CreateFile(FRAG_FILE3,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);


    if (hJunkX == INVALID_HANDLE_VALUE)
    {
        std::wcout << L"Unable to Create " << FRAG_FILE3 << L"\n";
        PrintLastError();
        AnyKey();
        CloseHandle(hShakespeare);
        CloseHandle(hJunk);
        return true;
    }

    failed |= ReadShakespeare(hShakespeare, Shakespeare_size / 4);

    failed |= ReadJunk(hJunk, Shakespeare_size / 4);

    failed |= ReadJunkX(hJunkX, Shakespeare_size / 4);
    
    CloseHandle(hJunkX);
    CloseHandle(hJunk);
    CloseHandle(hShakespeare);

    return failed;
}


HANDLE MakeOrOpenSparseFile()
{
    HANDLE hFile = CreateFile(L"Sparsely.bin", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        cout << "Using existing Sparsely.bin file\n";
        return hFile;
    }

    cout << "Creating new sparse file, Sparsely.bin\n...";

    hFile = CreateFile(L"Sparsely.bin", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Error creating Sparsely.bin!\n";
        PrintLastError();
        return INVALID_HANDLE_VALUE;
    }


    // find the cluster size for this drive
    DWORD SectorsPerCluster, BytesPerSector, NumberOfFreeClusters, TotalNumberOfClusters;

    DWORD ClusterSize = 4096; // just a guess

    if (GetDiskFreeSpace(NULL, &SectorsPerCluster, &BytesPerSector, &NumberOfFreeClusters, &TotalNumberOfClusters))
    {
        ClusterSize = SectorsPerCluster * BytesPerSector;
        cout << "Found cluster size of " << ClusterSize << "\n";
    }

    WCHAR VolumeName[MAX_PATH + 1];
    WCHAR FSname[MAX_PATH + 1];
    DWORD fsFlags;

    if (GetVolumeInformation(NULL, VolumeName, MAX_PATH + 1, NULL, NULL, &fsFlags, FSname, MAX_PATH + 1))
    {
        wcout << VolumeName << L"\n" << FSname << L"\n";
        DWORD bytesReturned;

        if (fsFlags & FILE_SUPPORTS_SPARSE_FILES)
        {
            cout << "Sparse files supported.\n";
            if (DeviceIoControl(hFile,
                FSCTL_SET_SPARSE,
                NULL,
                0,
                NULL,
                0,
                &bytesReturned,
                NULL))
            {
                FILE_ZERO_DATA_INFORMATION zeroData;
                zeroData.FileOffset.QuadPart = 0;
                zeroData.BeyondFinalZero.QuadPart = 100 * 1024 * 1024;   // ~100 megs of zeros

                if (DeviceIoControl(hFile,
                    FSCTL_SET_ZERO_DATA,
                    &zeroData,
                    sizeof(zeroData),
                    NULL,
                    0,
                    &bytesReturned,
                    NULL))
                {
                    SetFilePointer(hFile, 100 * 1024 * 1024, NULL, FILE_BEGIN);

                    if (!WriteFile(hFile, SUCCESS_STRING, sizeof(SUCCESS_STRING), &bytesReturned, NULL))
                    {
                        cout << "Error writing to sparse file!\n";
                        PrintLastError();
                        CloseHandle(hFile);
                        return INVALID_HANDLE_VALUE;
                    }



                    zeroData.FileOffset.QuadPart = SetFilePointer(hFile, 0, NULL, FILE_CURRENT);
                    zeroData.BeyondFinalZero.QuadPart = 200 * 1024 * 1024;   // ~100 megs of zeros

                    if (!DeviceIoControl(hFile,
                        FSCTL_SET_ZERO_DATA,
                        &zeroData,
                        sizeof(zeroData),
                        NULL,
                        0,
                        &bytesReturned,
                        NULL))
                    {
                        cout << "Error writing second sparse run!\n";
                        PrintLastError();
                        CloseHandle(hFile);
                        return INVALID_HANDLE_VALUE;
                    }



                    SetFilePointer(hFile, 200 * 1024 * 1024, NULL, FILE_BEGIN);

                    if (!WriteFile(hFile, SUCCESS_STRING2, sizeof(SUCCESS_STRING2), &bytesReturned, NULL))
                    {
                        cout << "Error writing to sparse file!\n";
                        PrintLastError();
                        CloseHandle(hFile);
                        return INVALID_HANDLE_VALUE;
                    }




                    zeroData.FileOffset.QuadPart = SetFilePointer(hFile, 0, NULL, FILE_CURRENT);
                    zeroData.BeyondFinalZero.QuadPart = 300 * 1024 * 1024;   // ~100 megs of zeros

                    if (!DeviceIoControl(hFile,
                        FSCTL_SET_ZERO_DATA,
                        &zeroData,
                        sizeof(zeroData),
                        NULL,
                        0,
                        &bytesReturned,
                        NULL))
                    {
                        cout << "Error writing second sparse run!\n";
                        PrintLastError();
                        CloseHandle(hFile);
                        return INVALID_HANDLE_VALUE;
                    }

                    SetFilePointer(hFile, 300 * 1024 * 1024, NULL, FILE_BEGIN);

                    if (!WriteFile(hFile, SUCCESS_STRING3, sizeof(SUCCESS_STRING3), &bytesReturned, NULL))
                    {
                        cout << "Error writing to sparse file!\n";
                        PrintLastError();
                        CloseHandle(hFile);
                        return INVALID_HANDLE_VALUE;
                    }

                    // finally, write some Shakespeare at the 42mB mark
                    SetFilePointer(hFile, 42 * 1024 * 1024, NULL, FILE_BEGIN);

                    if (!WriteFile(hFile, Shakespeare, Shakespeare_size, &bytesReturned, NULL))
                    {
                        cout << "Error writing Shakespeare to sparse file!\n";
                        PrintLastError();
                        CloseHandle(hFile);
                        return INVALID_HANDLE_VALUE;
                    }


                    return hFile;
                }
                else
                {
                    cout << "Failed to write zeros to a sparse file!\n";
                    PrintLastError();
                    CloseHandle(hFile);
                    return INVALID_HANDLE_VALUE;
                }
            }
            else
            {
                cout << "Failed to create a sparse file!\n";
                PrintLastError();
                CloseHandle(hFile);
                return INVALID_HANDLE_VALUE;
            }
        }
        else
        {
            cout << "Filesystem reports it doesn't support sparse files.\n";
            CloseHandle(hFile);
            return INVALID_HANDLE_VALUE;
        }
    }

    CloseHandle(hFile);
    return INVALID_HANDLE_VALUE;
}


HANDLE MakeOrOpenSparseFile2( DWORD ClusterSize )
{
    HANDLE hFile = CreateFile(L"Sparsely2.bin", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        cout << "Using existing Sparsely2.bin file\n";
        return hFile;
    }
    
    cout << "Creating new sparse file, Sparsely2.bin\n...";

    hFile = CreateFile(L"Sparsely2.bin", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Error creating Sparsely2.bin!\n";
        PrintLastError();
        return INVALID_HANDLE_VALUE;
    }

    WCHAR VolumeName[MAX_PATH + 1];
    WCHAR FSname[MAX_PATH + 1];
    DWORD fsFlags;

    if (!GetVolumeInformation(NULL, VolumeName, MAX_PATH + 1, NULL, NULL, &fsFlags, FSname, MAX_PATH + 1))
    {
        cout << "Failed to get volume information!\n";
        PrintLastError();
        CloseHandle(hFile);
        return INVALID_HANDLE_VALUE;
    }
     
    DWORD bytesReturned;

    if (!(fsFlags & FILE_SUPPORTS_SPARSE_FILES))
    {
        cout << "Sparse files not supported!\n";
        CloseHandle(hFile);
        return INVALID_HANDLE_VALUE;
    }

    if (!DeviceIoControl(hFile,
        FSCTL_SET_SPARSE,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL))
    {
        cout << "Unable to set file as sparse!\n";
        PrintLastError();
        CloseHandle(hFile);
        return INVALID_HANDLE_VALUE;
    }

    // Write one cluster of data
    DWORD Written;
    if (!WriteFile(hFile, Shakespeare, ClusterSize, &Written, NULL))
    {
        cout << "Failed write!\n";
        PrintLastError();
        CloseHandle(hFile);
        return INVALID_HANDLE_VALUE;
    }

    // write one cluster of zeros
    FILE_ZERO_DATA_INFORMATION zeroData;
    zeroData.FileOffset.QuadPart = ClusterSize;
    zeroData.BeyondFinalZero.QuadPart = ClusterSize * 128;   // 128 clusters of zeros

    if (!DeviceIoControl(hFile,
        FSCTL_SET_ZERO_DATA,
        &zeroData,
        sizeof(zeroData),
        NULL,
        0,
        &bytesReturned,
        NULL))
    {
        cout << "Error writing zeroes!\n";
        PrintLastError();
        CloseHandle(hFile);
        return INVALID_HANDLE_VALUE;
    }

    // write some more shakespeare
    SetFilePointer(hFile, ClusterSize * 128, NULL, FILE_BEGIN);
    if (!WriteFile(hFile, Shakespeare + ClusterSize, ClusterSize, &bytesReturned, NULL))
    {
        cout << "Failed write 2!\n";
        PrintLastError();
        CloseHandle(hFile);
        return INVALID_HANDLE_VALUE;
    }

    return hFile;
}

DWORD GetClusterSize()
{
    // find the cluster size for this drive
    DWORD SectorsPerCluster, BytesPerSector, NumberOfFreeClusters, TotalNumberOfClusters;

    DWORD ClusterSize = 4096; // just a guess

    if (GetDiskFreeSpace(NULL, &SectorsPerCluster, &BytesPerSector, &NumberOfFreeClusters, &TotalNumberOfClusters))
    {
        ClusterSize = SectorsPerCluster * BytesPerSector;
        cout << "Found cluster size of " << ClusterSize << "\n";
        return ClusterSize;
    }

    cout << "Guessing cluster size of " << ClusterSize << "\n";
    return ClusterSize;
}

// inspired by the bug(?) Aman Priyadarshi reported
// Writes a cluster of data, then some sparseness, then another cluster of data
// Note: I couldn't make windows create only one cluster of sparseness. It
// seems to "optimize" the write into actual zeros below a certain threshold of clusters.
// returns false on success
bool PerformSparseTest2()
{
    cout << "Testing continuous read of a small sparse file...\n";

    DWORD ClusterSize = GetClusterSize();

    HANDLE hFile = MakeOrOpenSparseFile2(ClusterSize);

    if (hFile == INVALID_HANDLE_VALUE)
        return true;

    bool failed = false;

    // setup a buffer we expect the file to be comparable to
    // Wow, I really need to clean this mess up. Sorry to anyone reading this :)
    void *buffer = malloc(ClusterSize * 129);
    memset(buffer, 0, ClusterSize * 129);

    memcpy(buffer, Shakespeare, ClusterSize);
    memcpy((BYTE*)buffer + (ClusterSize * 128), Shakespeare + ClusterSize, ClusterSize);

    void *ReadBuffer = malloc(ClusterSize * 129);
    memset(ReadBuffer, '!' , ClusterSize * 129);

    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

    DWORD BytesRead;
    if (!ReadFile(hFile, ReadBuffer, ClusterSize * 129, &BytesRead, NULL))
    {
        cout << "Error reading second sparse file!\n";
        PrintLastError();
        CloseHandle(hFile);
        free(buffer);
        free(ReadBuffer);
        return true;
    }

    failed |= ConfirmByteCount(ClusterSize * 129, BytesRead, false);

    if (memcmp(ReadBuffer, buffer, ClusterSize * 129) != 0)
    {
        cout << "Data failed to match!\n";
        free(buffer);
        free(ReadBuffer);
        CloseHandle(hFile);
        return true;
    }

    // try a read starting from the sparse section
    SetFilePointer(hFile, ClusterSize + 1, NULL, FILE_BEGIN);

    memset(ReadBuffer, '!', ClusterSize * 129);

    if (!ReadFile(hFile, ReadBuffer, ClusterSize * 128 - 1, &BytesRead, NULL))
    {
        cout << "Error performing read starting from sparse section of sparse file!\n";
        free(buffer);
        free(ReadBuffer);
        CloseHandle(hFile);
        return true;
    }

    failed |= ConfirmByteCount(ClusterSize * 128 - 1, BytesRead, false);

    if (memcmp(ReadBuffer, (BYTE*)buffer + ClusterSize + 1, ClusterSize * 128 - 1) != 0)
    {
        cout << "Didn't read expected data when starting from sparse section of sparse file!\n";
        free(buffer);
        free(ReadBuffer);
        CloseHandle(hFile);
        return true;
    }

    if( !failed )
        cout << "Success!\n";
    free(buffer);
    free(ReadBuffer);
    CloseHandle(hFile);

    return failed;
}


// creates and writes to a sparse file, then reads from it
// returns false on success
bool PerformSparseTests()
{
    cout << "Testing operation of sparse files...\n";

    HANDLE hFile = MakeOrOpenSparseFile();

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Error opening or creating sparse file\n";
        return true;
    }

    cout << "Reading data at various positions in sparse file...\n";

    BYTE buffer[Shakespeare_size];
    memset(buffer, '!', Shakespeare_size);
    DWORD bytesReturned;

    bool failed = false;

    SetFilePointer(hFile, 100 * 1024 * 1024, NULL, FILE_BEGIN);

    if (!ReadFile(hFile, buffer, sizeof(SUCCESS_STRING), &bytesReturned, NULL))
    {
        cout << "Error reading from sparse file!\n";
        PrintLastError();
        CloseHandle(hFile);
        return INVALID_HANDLE_VALUE;
    }
    
    failed |= ConfirmByteCount(sizeof(SUCCESS_STRING), bytesReturned, false);

    if (memcmp(buffer, SUCCESS_STRING, sizeof(SUCCESS_STRING)) != 0)
    {
        cout << "Failed first compare!\n";
        failed = true;
    }

    SetFilePointer(hFile, 200 * 1024 * 1024, NULL, FILE_BEGIN);

    if (!ReadFile(hFile, buffer, sizeof(SUCCESS_STRING2), &bytesReturned, NULL))
    {
        cout << "Error reading from sparse file!\n";
        PrintLastError();
        CloseHandle(hFile);
        return INVALID_HANDLE_VALUE;
    }

    failed |= ConfirmByteCount(sizeof(SUCCESS_STRING2), bytesReturned, false);

    if (memcmp(buffer, SUCCESS_STRING2, sizeof(SUCCESS_STRING2)) != 0)
    {
        cout << "Failed second compare!\n";
        failed = true;
    }

    SetFilePointer(hFile, 300 * 1024 * 1024, NULL, FILE_BEGIN);

    if (!ReadFile(hFile, buffer, sizeof(SUCCESS_STRING3), &bytesReturned, NULL))
    {
        cout << "Error reading from sparse file!\n";
        PrintLastError();
        CloseHandle(hFile);
        return INVALID_HANDLE_VALUE;
    }

    failed |= ConfirmByteCount(sizeof(SUCCESS_STRING3), bytesReturned, false);

    if (memcmp(buffer, SUCCESS_STRING3, sizeof(SUCCESS_STRING3)) != 0)
    {
        cout << "Failed third compare!\n";
        failed = true;
    }

    // now go back a bit
    SetFilePointer(hFile, 42 * 1024 * 1024, NULL, FILE_BEGIN);

    if (!ReadFile(hFile, buffer, Shakespeare_size, &bytesReturned, NULL))
    {
        cout << "Error reading from sparse file!\n";
        PrintLastError();
        CloseHandle(hFile);
        return INVALID_HANDLE_VALUE;
    }
    failed |= ConfirmByteCount(Shakespeare_size, bytesReturned, false);

    if (memcmp(buffer, Shakespeare, Shakespeare_size) != 0)
    {
        cout << "Failed Shakespeare compare!\n";
        failed = true;
    }

    BYTE Zeros[4096];
    memset(Zeros, 0, 4096);

    // read some data from around the 24meg mark, and make sure it's sparse
    SetFilePointer(hFile, 24 * 1024 * 1024, NULL, FILE_BEGIN);

    if (!ReadFile(hFile, buffer, 4096, &bytesReturned, NULL))
    {
        cout << "Error reading from sparse section of sparse file!\n";
        PrintLastError();
        CloseHandle(hFile);
        return INVALID_HANDLE_VALUE;
    }

    failed |= ConfirmByteCount(4096, bytesReturned, false);

    if (memcmp(buffer, Zeros, 4096) != 0)
    {
        cout << "Failed sparse-to-zero compare!\n";
        failed = true;
    }

    if (!failed)
        cout << "Success!\n";
    
    CloseHandle(hFile);
    return failed;
}

// In Windows, if you open a file with FILE_FLAG_NO_BUFFERING, then try to 
// perform a write that isn't sector-aligned, the write will fail. This 
// test is to ensure our driver has consistent behavior with the Windows implementation.
bool PerformNoBufferingFailureTest()
{
    cout << "Testing that FILE_FLAG_NO_BUFFERING works consistently with Windows...\n";
    // Open a file for write-through access
    HANDLE hFile = CreateFile(L"2239.txt", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Couldn't open 2239.txt!\nPlease run this program in Windows first!\n";
        PrintLastError();
        AnyKey();
        return true;
    }

    // now, try to perform a write that is less than a sector
    DWORD written;
    if (WriteFile(hFile, Shakespeare, 1, &written, NULL))
    {
        cout << "Error: Able to perform non-sector-aligned write to file opened with FILE_FLAG_NO_BUFFERING.\n";
        CloseHandle(hFile);
        return true;
    }
    else
    {
        if (GetLastError() == ERROR_INVALID_PARAMETER)
        {
            if( written == 0 )
                cout << "Success!\n";
            else
            {
                cout << "Error! WriteFile() returns ERROR_INVALID_PARAMETER but reports " << written << " bytes written.\n";
                return true;
            }
        }
        else
        {
            cout << "Received different error code, was expected STATUS_INVALID_PARAMETER (87) but got: ";
            PrintLastError();
            CloseHandle(hFile);
            return true;
        }
    }

    CloseHandle(hFile);

    return false;
}

// returns false on success
bool PerformZeroByteTests()
{
    cout << "Testing 0-byte IO to a file with length 0 against Windows behavior...\n";
    bool failed = false;

    // try to open 0byte.bin
    HANDLE hFile = CreateFile(L"0byte.bin", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        hFile = CreateFile(L"0byte.bin", GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, NULL);
    }
    else
    {
        // ensure the file is still 0 bytes
        if (GetFileSize(hFile, NULL) > 0)
        {
            cout << "0byte.bin is no longer 0 bytes and must be recreated.\n";
            CloseHandle(hFile);

            hFile = hFile = CreateFile(L"0byte.bin", GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, NULL);
        }
    }

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Couldn't open 0byte.bin!\nPlease run this program in Windows first!\n";
        PrintLastError();
        AnyKey();
        return true;
    }

    BYTE Buffer[1];
    DWORD ByteCount;

    
    // try to read 0 bytes from the file
    if (!ReadFile(hFile, Buffer, 0, &ByteCount, NULL))
    {
        if (GetLastError() != ERROR_INVALID_PARAMETER)
        {
            cout << "0-byte read with valid buffer returns a different error than expected.\nExpected ERROR_INVALID_PARAMETER.\n";
            PrintLastError();
            failed = true;
        }
    }
    else
    {
        cout << "0-byte read with valid buffer succeeds, when it doesn't in Windows.\nExpected ERROR_INVALID_PARAMETER.\n";
        PrintLastError();
        failed = true;
    }

    // try a 0-byte read
    if (!ReadFile(hFile, NULL, 0, &ByteCount, NULL))
    {
        cout << "0-byte read failed.\n";
        PrintLastError();
        failed = true;
    }
    
    if (ByteCount != 0)
    {
        cout << "0-byte read results in ReadFile() reporting " << ByteCount << " bytes read.\n";
        failed = true;
    }

   
    // try to write 0 bytes, but pass in a valid buffer
    if (!WriteFile(hFile, Buffer, 0, &ByteCount, NULL))
    {
        if (GetLastError() != ERROR_INVALID_PARAMETER)
        {
            cout << "null write but with a valid buffer returns a different error than expected, expected ERROR_INVALID_PARAMETER.\n";
            PrintLastError();
            failed = true;
        }
    }
    else
    {
        cout << "null write succeeded even though we didn't pass in a NULL buffer.\nWas expecting ERROR_INVALID_PARAMETER.\n";
        PrintLastError();
        failed = true;
    }

    // try a null write
    if (!WriteFile(hFile, NULL, 0, &ByteCount, NULL))
    {
        cout << "null write failed.\n";
        PrintLastError();
        failed = true;
    }

    if (ByteCount != 0)
    {
        cout << "Null write results in WriteFile() reporting " << ByteCount << " bytes written.\n";
        failed = true;
    }

    CloseHandle(hFile);

    if( !failed )
        cout << "Success!\n";

    return failed;

}


// returns false on success
bool WriteTo(HANDLE hFile, LPCVOID pBuffer, DWORD size)
{
    DWORD Written;
    if (!WriteFile(hFile, pBuffer, size, &Written, NULL))
    {
        cout << "Error writing to file!\n";
        // TODO: associate filename with handle in a map and present it here

        PrintLastError();
        return true;
    }

    return ConfirmByteCount(size, Written, true);
}

// returns false on success
bool ReadTo(const HANDLE hFile, LPVOID pBuffer, DWORD size)
{
    DWORD Read;
    if (!ReadFile(hFile, pBuffer, size, &Read, NULL))
    {
        cout << "Error reading file!\n";
        // TODO: associate filename with handle in a map and present it here

        PrintLastError();
        return true;
    }

    return ConfirmByteCount(size, Read, true);
}

bool ReadAndCompare(HANDLE hFile, LPCVOID pBuffer, DWORD size)
{
    BYTE *Buffer = (BYTE*)malloc(size);

    bool failed = false;

    failed |= ReadTo(hFile, Buffer, size);

    if (memcmp(Buffer, pBuffer, size) != 0)
    {
        cout << "Error! Data read didn't match expected values\n";
        failed = true;
    }

    free(Buffer);

    return failed;
}

// returns false on success
bool PerformNamedStreamTests()
{
    cout << "Performing named stream tests...\n";
    bool failed = false;

    // Create or open a file containing some named streams
    HANDLE hFile = CreateFile(_T("NamedStreams.txt"), FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_ALWAYS, 0, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Error! Unable to open or create NamedStreams.txt!\n";
        return true;
    }

    failed |= WriteTo(hFile, (LPCVOID)STREAM_STRING, sizeof(STREAM_STRING));

    // now we create a named stream called Shakespeare
    cout << "Testing access to NamedStreams.txt:Shakespeare...\n";
    HANDLE hShake = CreateFile(_T("NamedStreams.txt:Shakespeare"), FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_ALWAYS, 0, NULL);

    if (hShake == INVALID_HANDLE_VALUE)
    {
        cout << "Error! Unable to open or create NamedStreams.txt:Shakespeare!\n";
        CloseHandle(hFile);
        return true;
    }

    failed |= WriteShakespeare(hShake);

    failed |= ReadShakespeare(hShake);

    // now we create a named stream called Junk
    cout << "Testing access to NamedStreams.txt:Junk...\n";
    HANDLE hJunk = CreateFile(_T("NamedStreams.txt:Junk"), FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_ALWAYS, 0, NULL);

    if (hJunk == INVALID_HANDLE_VALUE)
    {
        cout << "Error! Unable to open or create NamedStreams.txt:Junk!\n";
        CloseHandle(hFile);
        CloseHandle(hShake);
        return true;
    }

    failed |= RuinShakespeare(hJunk);

    failed |= ReadJunk(hJunk);

    if (!failed)
        cout << "Success!\n";

    CloseHandle(hShake);
    CloseHandle(hFile);
    return failed;
}

// returns false on success
bool PerformPosixTests()
{
    cout << "Performing POSIX tests...\n";
    bool failed = false;

    // Create or open a file containing lower-case lettering
    HANDLE hFile = CreateFile(_T("specialcase.txt"), FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_ALWAYS, 0, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "\nError! Unable to open or create specialcase.txt!\n";
        return true;
    }

    failed |= WriteTo(hFile, (LPCVOID)SUCCESS_STRING, sizeof(SUCCESS_STRING));

    // close it
    CloseHandle(hFile);

    // Make sure we can open the file using different case

    // Open the same file using upper-case lettering
    hFile = CreateFile(_T("SpecialCase.txt"), FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_ALWAYS, 0, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "\nError! Unable to open or create SpecialCase.txt!\n";
        return true;
    }

    failed |= ReadAndCompare(hFile, (LPCVOID)SUCCESS_STRING, sizeof(SUCCESS_STRING));

    failed |= WriteShakespeare(hFile);

    failed |= ReadShakespeare(hFile);

    // close it
    CloseHandle(hFile);


    // Now do case-sensitive test:
    // Open the same file using upper-case lettering
    hFile = CreateFile(_T("SpEcIalCaSe.tXt"), FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       OPEN_ALWAYS, FILE_FLAG_POSIX_SEMANTICS, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "\nError! Unable to open or create SpecialCase.txt!\n";
        return true;
    }

    cout << "The next test should fail:\n";
    failed |= !ReadAndCompare(hFile, (LPCVOID)SUCCESS_STRING, sizeof(SUCCESS_STRING));

    // close it
    CloseHandle(hFile);

    if (!failed)
        cout << "Success!\n";

    return failed;
}

// returns false on success
bool PerformFileOverWriteTest()
{
    bool failed = false;

    cout << "Performing file overwrite test...\n";

    // Always create
    HANDLE hFile = CreateFile(_T("CreatedAlways.txt"), FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              CREATE_ALWAYS, 0, NULL);

    // truncate if can't create
    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Unable to open file with CREATE_ALWAYS, trying with TRUNCATE_EXISTING\n";
        hFile = CreateFile(_T("CreatedAlways.txt"), FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           TRUNCATE_EXISTING, 0, NULL);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            PrintLastError();
            return true;
        }
    }

    // at this point, the file size should be 0
    DWORD FileSize = GetFileSize(hFile, NULL);
    if (FileSize != 0)
    {
        cout << "Error: File opened with CREATE_ALWAYS has a file size of " << FileSize << "\n";
        failed = true;
    }

    // write 1024 bytes, so the file will definitely be non-resident
    failed |= WriteTo(hFile, (LPCVOID)Shakespeare, 1024);

    CloseHandle(hFile);

    return failed;
}

bool PerformResidentTest()
{
    bool failed = false;

    cout << "Performing resident test...\n";

    HANDLE hFile = CreateFile(L"ResFile.txt", FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              CREATE_ALWAYS, 0, NULL);

    // truncate if can't create
    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Unable to open file with CREATE_ALWAYS, trying with TRUNCATE_EXISTING\n";
        hFile = CreateFile(_T("CreatedAlways.txt"), FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           TRUNCATE_EXISTING, 0, NULL);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            PrintLastError();
            return true;
        }
    }

    // at this point, the file size should be 0
    DWORD FileSize = GetFileSize(hFile, NULL);
    if (FileSize != 0)
    {
        cout << "Error: File opened with CREATE_ALWAYS has a file size of " << FileSize << "\n";
        failed = true;
    }

    // write no more than 600 bytes, so the file should stay resident
    int offset = 0;
    int length = 0;
    BYTE Buffer[600];

    memset(Buffer, 0, 600);

    for (length = 6; length < 600; length += 6, offset += 6)
    {
        failed |= WriteTo(hFile, (LPCVOID)(Shakespeare + offset), 6);

        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
        ReadTo(hFile, Buffer, length);

        if (memcmp((LPCVOID)Shakespeare, Buffer, length) != 0)
        {
            cout << "Didn't read expected data!\n";
            failed = true;
        }

        if (GetFileSize(hFile, NULL) != length)
        {
            cout << "Error! GetFileSize(hFile, NULL) returns " << GetFileSize(hFile, NULL) << " expected " << length << "!\n";
            failed = true;
        }
    }

    CloseHandle(hFile);

    if (!failed)
        cout << "Success!\n";

    return failed;
}

bool PerformResidentTest2()
{
    bool failed = false;

    cout << "Performing resident to non-resident migration test...\n";

    char filename[] = "a.txt" ;
    HANDLE hFile;

    for (char c = 'a'; c <= 'z'; c++)
    {
        filename[0] = c;
        hFile = CreateFileA(filename, FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                  OPEN_ALWAYS, 0, NULL);

        if (hFile == INVALID_HANDLE_VALUE)
            continue;

        // get this file's size
        DWORD FileSize = GetFileSize(hFile, NULL);
        if (FileSize < 624)
            break;

        //CloseHandle(hFile);
    }

    // truncate if can't create
    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "  ERROR: Unable to find an appropriate file to use for migration test!\n";
        return true;
    }

    cout << "Using " << filename << "\n";

    // write no more than 600 bytes, so the file should stay resident
    int offset = 0;
    int length = 0;
    BYTE Buffer[600];

    memset(Buffer, 0, 600);

    for (length = 6; length < 600; length += 6, offset += 6)
    {
        failed |= WriteTo(hFile, (LPCVOID)(Shakespeare + offset), 6);

        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
        ReadTo(hFile, Buffer, length);

        if (memcmp((LPCVOID)Shakespeare, Buffer, length) != 0)
        {
            cout << "Didn't read expected data!\n";
            failed = true;
        }

        /*if (GetFileSize(hFile, NULL) != length)
        {
            cout << "Error! GetFileSize(hFile, NULL) returns " << GetFileSize(hFile, NULL) << " expected " << length << "!\n";
            failed = true;
        }*/
    }

    // now write an extra 1024 bytes past the end
    DWORD FileSize = GetFileSize(hFile, NULL);
    SetFilePointer(hFile, 0, NULL, FILE_END);

    failed |= WriteTo(hFile, (LPCVOID)(Shakespeare + FileSize), 1024);

    BYTE Buffer2k[2048];

    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    failed |= ReadTo(hFile, Buffer2k, FileSize + 1024);

    if (memcmp((LPCVOID)Shakespeare, Buffer2k, FileSize + 1024) != 0)
    {
        cout << "Didn't read expected data!\n";
        failed = true;
    }

    CloseHandle(hFile);

    if (!failed)
        cout << "Success!\n";
    else
    {
        cout << "Failed!\n";
        PrintLastError();
    }

    return failed;
}

bool PerformAllocationSizeTests()
{
    bool failed = false;
    cout << "Performing allocation tests...\n";

    // Create or open a small file
    HANDLE hFile = CreateFile(_T("AllocFile.txt"), FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_ALWAYS, 0, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Error! Unable to open or create AllocFile.txt!\n";
        return true;
    }

    DWORD FileSize = GetFileSize(hFile, NULL);

    cout << "AllocFile.txt size: " << FileSize << "\n";

    // increase the file size by at least one cluster 
    FileSize += 4097;

    failed |= WriteTo(hFile, (LPCVOID)Shakespeare, FileSize);

    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

    char *buffer = (char *)malloc(FileSize);

    failed |= ReadTo(hFile, buffer, FileSize);

    if (memcmp((const void*)Shakespeare, (const void*)buffer, FileSize) != 0)
    {
        cout << "Data read doesn't match expected!\n";
        failed = true;
    }

    free(buffer);

    cout << "New AllocFile.txt size: " << GetFileSize(hFile, NULL) << "\n";

    CloseHandle(hFile);

    if (!failed)
        cout << "Success!\n";


    return failed;
}

// returns false on success
bool PerformFileSizeTests()
{
    bool failed = false;

    cout << "Performing file extension test...\n";

    // Create or open a small file
    HANDLE hFile = CreateFile(_T("SmallFile.txt"), FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_ALWAYS, 0, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Error! Unable to open or create SmallFile.txt!\n";
        return true;
    }

    DWORD FileSize = GetFileSize(hFile, NULL);

    cout << "SmallFile.txt size: " << FileSize << "\n";

    // ensure the file won't be resident
    /*if (FileSize < 1024)
    FileSize = 1024;

    //FileSize = 1063;

    // increase the file size by one byte (just kidding don't)
    failed |= WriteTo(hFile, (LPCVOID)Shakespeare, FileSize);

    //SetFilePointer(hFile, 0, NULL, FILE_END);

    failed |= WriteTo(hFile, SUCCESS_WINDOWS, sizeof(SUCCESS_WINDOWS)-1);
    */

    if (FileSize < 1024)
        FileSize = 1024;

    //FileSize = 1063;

    // increase the file size by one byte (just kidding don't)
    failed |= WriteTo(hFile, (LPCVOID)Shakespeare, FileSize + 6);

    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

    char *buffer = (char *)malloc(FileSize + 6);

    failed |= ReadTo(hFile, buffer, FileSize + 6);

    if (memcmp((const void*)Shakespeare, (const void*)buffer, FileSize + 6) != 0)
    {
        cout << "Data read doesn't match expected!\n";
        failed = true;
    }

    free(buffer);

    //failed |= WriteTo(hFile, SUCCESS_WINDOWS, sizeof(SUCCESS_WINDOWS) - 1);

    cout << "New SmallFile.txt size: " << GetFileSize(hFile, NULL) << "\n";

    CloseHandle(hFile);

    if (!failed)
        cout << "Success!\n";

    return failed;
}


// returns false on success
bool PerformFileSizeTests2()
{
    bool failed = false;

    cout << "Performing file truncation test...\n";

    // Create or open a small file
    HANDLE hFile = CreateFile(_T("SmallFile.txt"), FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_ALWAYS, 0, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Error! Unable to open or create SmallFile.txt!\n";
        return true;
    }

    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

    SetEndOfFile(hFile);

    DWORD FileSize = GetFileSize(hFile, NULL);

    cout << "SmallFile.txt size: " << FileSize << "\n";

    if (FileSize != 0)
    {
        failed = true;
        cout << "Failed to truncate file to 0!\n";
    }

    // ensure the file won't be resident
    /*if (FileSize < 1024)
    FileSize = 1024;

    //FileSize = 1063;

    // increase the file size by one byte (just kidding don't)
    failed |= WriteTo(hFile, (LPCVOID)Shakespeare, FileSize);

    //SetFilePointer(hFile, 0, NULL, FILE_END);

    failed |= WriteTo(hFile, SUCCESS_WINDOWS, sizeof(SUCCESS_WINDOWS)-1);
    */

    if (FileSize < 1024)
        FileSize = 1024;

    //FileSize = 1063;

    FileSize += 6;

    SetFilePointer(hFile, FileSize, NULL, FILE_BEGIN);
    SetEndOfFile(hFile);

    FileSize = GetFileSize(hFile, NULL);

    cout << "SmallFile.txt size: " << FileSize << "\n";

    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

    // increase the file size by one byte (just kidding don't)
    failed |= WriteTo(hFile, (LPCVOID)Shakespeare, FileSize);

    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

    char *buffer = (char *)malloc(FileSize);

    failed |= ReadTo(hFile, buffer, FileSize);

    if (memcmp((const void*)Shakespeare, (const void*)buffer, FileSize) != 0)
    {
        cout << "Data read doesn't match expected!\n";
        failed = true;
    }

    free(buffer);

    //failed |= WriteTo(hFile, SUCCESS_WINDOWS, sizeof(SUCCESS_WINDOWS) - 1);

    cout << "New SmallFile.txt size: " << GetFileSize(hFile, NULL) << "\n";

    CloseHandle(hFile);

    if (!failed)
        cout << "Success!\n";

    return failed;
}

int main()
{
    //bool Failed = CreateABunchOfFiles(700) | BuildVerificationFile();
    bool Failed = VerifyABunchOfFiles();

    // This was intended to stress FAT corruption, but I was never able to reproduce it via this tester
    //Failed |= CreateABunchOfFiles(100);
    /*Failed |= CreateABunchOfFolders(100);
    Failed |= CreateABunchOfFiles(100);
    Failed |= CreateABunchOfFolders(100);
    Failed |= CreateABunchOfFiles(100);
    Failed |= CreateABunchOfFolders(50);
    Failed |= CreateABunchOfFiles(50);
    Failed |= CreateABunchOfFolders(25);
    Failed |= CreateABunchOfFiles(25);
    Failed |= CreateABunchOfFolders(25);
    Failed |= CreateABunchOfFiles(25);
    Failed |= CreateABunchOfFolders(12);
    Failed |= CreateABunchOfFiles(12);
    Failed |= CreateABunchOfFolders(6);
    Failed |= CreateABunchOfFiles(6);
    Failed |= CreateABunchOfFolders(3);
    Failed |= CreateABunchOfFiles(3);
    Failed |= CreateABunchOfFolders(2);
    Failed |= CreateABunchOfFiles(2);
    Failed |= CreateABunchOfFolders(1);
    Failed |= CreateABunchOfFiles(1);
    Failed |= CreateABunchOfFolders(6);
    Failed |= CreateABunchOfFiles(6);
    Failed |= CreateABunchOfFolders(3);
    Failed |= CreateABunchOfFiles(3);
    Failed |= CreateABunchOfFolders(2);
    Failed |= CreateABunchOfFiles(2);
    Failed |= CreateABunchOfFolders(1);
    Failed |= CreateABunchOfFiles(1);
    Failed |= CreateABunchOfFolders(25);
    Failed |= CreateABunchOfFiles(25);
    Failed |= CreateABunchOfFolders(12);
    Failed |= CreateABunchOfFiles(12);
    Failed |= CreateABunchOfFolders(6);
    Failed |= CreateABunchOfFiles(6);
    Failed |= CreateABunchOfFolders(3);
    Failed |= CreateABunchOfFiles(3);
    Failed |= CreateABunchOfFolders(2);
    Failed |= CreateABunchOfFiles(2);
    Failed |= CreateABunchOfFolders(1);
    Failed |= CreateABunchOfFiles(1);
    Failed |= CreateABunchOfFolders(6);
    Failed |= CreateABunchOfFiles(6);
    Failed |= CreateABunchOfFolders(3);
    Failed |= CreateABunchOfFiles(3);
    Failed |= CreateABunchOfFolders(2);
    Failed |= CreateABunchOfFiles(2);
    Failed |= CreateABunchOfFolders(1);
    Failed |= CreateABunchOfFiles(1);*/
    
        
    /*Failed |= PerformPosixTests();
    
    Failed = PerformBasicTests();

    Failed |= PerformShakeSpeareTests();

    Failed |= PerformInterleavedWriteTests();

    //Failed |= PerformSparseTests();

    //Failed |= PerformSparseTest2();

    Failed |= PerformNoBufferingFailureTest();

    //Failed |= PerformZeroByteTests();

    //Failed |= PerformNamedStreamTests();
   
    Failed |= PerformFileSizeTests();
    
    Failed |= PerformFileSizeTests2();

    Failed |= PerformAllocationSizeTests();

    Failed |= PerformFileOverWriteTest();

    Failed |= PerformResidentTest();

    Failed |= PerformResidentTest2();*/
    

    if (Failed)
        cout << "Overall Status: Failure! :(\n";
    else
        cout << "Overall Status: Success! :)\n";

	AnyKey();

	return 0;
}

