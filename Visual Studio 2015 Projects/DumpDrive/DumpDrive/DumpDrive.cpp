// DumpDrive.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "windows.h"
#include  <iostream>
#include "conio.h"
#include "ntfs.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <stdarg.h>
using namespace std;

int GetMenuChoice(int numChoices, int defaultSelection, char *szChoices, ...);
char *menuPrompt = NULL;
int promptLines = 0;

void ViewDiskInfo();
HANDLE OpenDrive(char driveLetter, PULARGE_INTEGER pDriveSize);

// Thank you cplusplus.com
void ClearScreen()
{
    HANDLE                     hStdOut;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD                      count;
    DWORD                      cellCount;
    COORD                      homeCoords = { 0, promptLines };

    hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == INVALID_HANDLE_VALUE) return;

    /* Get the number of cells in the current buffer */
    if (!GetConsoleScreenBufferInfo(hStdOut, &csbi)) return;
    cellCount = csbi.dwSize.X *csbi.dwSize.Y - (promptLines * csbi.dwSize.X);

    /* Fill the entire buffer with spaces */
    if (!FillConsoleOutputCharacter(
        hStdOut,
        (TCHAR) ' ',
        cellCount,
        homeCoords,
        &count
    )) return;

    /* Fill the entire buffer with the current colors and attributes */
    if (!FillConsoleOutputAttribute(
        hStdOut,
        csbi.wAttributes,
        cellCount,
        homeCoords,
        &count
    )) return;

    /* Move the cursor home */
    SetConsoleCursorPosition(hStdOut, homeCoords);
}

NTFS_VCB Vcb;
MFTENTRY MasterFileTableMftEntry;
NTFS_ATTR_RECORD MasterFileTableDataRecord;
PNTFS_ATTR_CONTEXT MftContext;
PNTFS_ATTR_CONTEXT FileContext;
PMFTENTRY mft;
ULONGLONG mftSize;

std::ofstream htmlfile;



NTSTATUS
FixupUpdateSequenceArray(ULONG BytesPerSector,
                         MFTENTRY *Record)
{
    USHORT *USA;
    USHORT USANumber;
    USHORT USACount;
    USHORT *Block;

    USA = (USHORT*)((PCHAR)Record + Record->FixupArrayOffset);
    USANumber = *(USA++);
    USACount = Record->FixupEntryCount - 1; /* Exclude the USA Number. */
    Block = (USHORT*)((PCHAR)Record + BytesPerSector - 2);

    while (USACount)
    {
        if (*Block != USANumber)
        {
            cout << "Mismatch with USA: " << *Block << " read " << USANumber << " %u expected\n";
            return STATUS_UNSUCCESSFUL;
        }
        *Block = *(USA++);
        Block = (USHORT*)((PCHAR)Block + BytesPerSector);
        USACount--;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
ReadFileRecord(HANDLE hDrive,
               PBOOTFILE pBoot,
               ULONGLONG index,
               MFTENTRY *file)
{
    ULONGLONG BytesRead;

    //   DPRINT("ReadFileRecord(%p, %I64x, %p)\n", Vcb, index, file);

    // copy the file record from the master file table
    BytesRead = ReadAttribute(hDrive, pBoot, MasterFileTableDataRecord, index * 1024, (PCHAR)file, 1024);

    if (BytesRead != 1024)
    {
        cout << "ReadFileRecord failed. " << BytesRead << " read, expected: 1024\n";
        return -1;
    }

    /* Apply update sequence array fixups. */
    return FixupUpdateSequenceArray(pBoot->BytesPerSector, file);
}





NTSTATUS
FindAttribute(PDEVICE_EXTENSION Vcb,
              MFTENTRY /* PFILE_RECORD_HEADER */ *MftRecord,
              ULONG Type,
              PCWSTR Name,
              ULONG NameLength,
              PNTFS_ATTR_CONTEXT * AttrCtx,
              PULONG Offset)
{
    BOOLEAN Found;
    NTSTATUS Status;
    FIND_ATTR_CONTXT Context;
    PNTFS_ATTR_RECORD Attribute;

    //cout <<("FindAttribute(%p, %p, 0x%x, %S, %u, %p)\n", Vcb, MftRecord, Type, Name, NameLength, AttrCtx);

    Found = FALSE;
    Status = FindFirstAttribute(&Context, Vcb, MftRecord, FALSE, &Attribute);
    while (NT_SUCCESS(Status))
    {
        if (Attribute->Type == Type && Attribute->NameLength == NameLength)
        {
            if (NameLength != 0)
            {
                PWCHAR AttrName;

                AttrName = (PWCHAR)((PCHAR)Attribute + Attribute->NameOffset);
                //DPRINT1("%.*S, %.*S\n", Attribute->NameLength, AttrName, NameLength, Name);
                cout << Attribute->NameLength << AttrName << NameLength << Name << "\n";

                //if (RtlCompareMemory(AttrName, Name, NameLength << 1) == (NameLength << 1))
                if (memcmp(AttrName, Name, NameLength << 1) == 0)
                {
                    Found = TRUE;
                }
            }
            else
            {
                Found = TRUE;
            }

            if (Found)
            {
                /* Found it, fill up the context and return. */
                cout << "Found context\n";
                *AttrCtx = PrepareAttributeContext(Attribute);
                //                (*AttrCtx)->Offset = (UCHAR*)&Context.CurrAttr - (UCHAR*)&MftRecord;

                if (Offset != NULL)
                    *Offset = Context.Offset;

                FindCloseAttribute(&Context);
                return STATUS_SUCCESS;
            }
        }

        Status = FindNextAttribute(&Context, &Attribute);
    }

    FindCloseAttribute(&Context);
    return STATUS_OBJECT_NAME_NOT_FOUND;
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

int WipeDrive()
{
    {
        cout << "Select drive to ERASE (Press esc to exit): ";

        char driveLetter;
        driveLetter = getch();               

        if (driveLetter == 27)
        {
            cout << "\nA wise decision\n";
            return 0;
        }

        if (driveLetter == 'c')
        {
            cout << "\nErase c:\\?!\nEven I'm not crazy enough to let you do that!\n";
            return 0;
        }

        char pathName[7] = "\\\\.\\C:";
        pathName[4] = driveLetter;

        char driveName[5] = "R:\\";
        driveName[0] = driveLetter;

        ClearScreen();
        promptLines = 3;

        cout << "About to COMPLETELY ERASE " << driveName << "\nYou will lose all data on this drive and need to reformat if you continue!\n";
        
        cout << "You're totally sure you don't have any data on " << driveName << "?";
        int choice = GetMenuChoice(5, 0, "Exit", "Exit", "!Yes - Wipe Drive completely!", "Exit", "Exit");

        if (choice != 2)
            return 0;

        promptLines = 0;
        ClearScreen();

        promptLines = 2;
        cout << "Last chance - Totally sure you want to erase " << driveName << " and reformat after this?\nThis a bad idea if you have important data on ANY of your drives!\n";
        choice = GetMenuChoice(5, 4, "Exit", "Exit", "!Yes - Wipe Drive completely!", "Exit", "Exit");

        if (choice != 2)
            return 0;

        // open up drive whatever:
        HANDLE hDrive = CreateFileA(pathName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

        if (hDrive == INVALID_HANDLE_VALUE)
        {
            cout << "Couldn't open drive\n";
            return -1;
        }

        ULARGE_INTEGER DriveSize;
        //GetFileSizeEx(hDrive, &DriveSize);

        GetDiskFreeSpaceExA(driveName,
                            NULL,
                            &DriveSize,
                            NULL);

        cout << "Size: " << DriveSize.QuadPart / (1024 * 1024) << " megs\n";
        cout << "Size: " << DriveSize.QuadPart / (1024 * 1024 * 1024) << " gigs\n";

        /*OPENFILENAME ofn;

        WCHAR szFileName[MAX_PATH] = L"";

        ZeroMemory(&ofn, sizeof(ofn));

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = L"Bin Files (*.bin)\0*.bin\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = szFileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrDefExt = (LPCWSTR)L"bin";

        if (!GetSaveFileName(&ofn))
        {
            cout << "User cancelled save dialog\n";
            return 0;
        }

        HANDLE hFile = CreateFile(ofn.lpstrFile, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            cout << "Couldn't create ";
            wcout << ofn.lpstrFile << L"\n";
            return -1;
        }

        // Make the file sparse
        FILE_SET_SPARSE_BUFFER sparseBuffer;
        sparseBuffer.SetSparse = true;
        DWORD bytesReturned;
        BOOL returnValue = DeviceIoControl(hFile,
                                           FSCTL_SET_SPARSE,
                                           NULL,
                                           0,
                                           NULL,
                                           0,
                                           &bytesReturned,
                                           NULL);

        if (returnValue)
            printf("Success\n");
        else
            printf("Failure\n");*/

        // zero one meg at a time until almost done
        LARGE_INTEGER BytesLeft;
        BytesLeft.QuadPart = DriveSize.QuadPart;
#define ONEMEG 1024 * 1024
        BYTE *Buffer = (BYTE*)malloc(ONEMEG);
        memset(Buffer, 0, ONEMEG);
        DWORD BufferOffset = 0;
        DWORD Written;
        while (BytesLeft.QuadPart >= ONEMEG)
        {
            // read drive
            /*if (!ReadFile(hDrive, Buffer, ONEMEG, &Read, NULL))
            {
                cout << "Failed to read drive!\n";
                return -1;
            }*/

            if (!WriteFile(hDrive, Buffer, ONEMEG, &Written, NULL))
            {
                cout << "Unable to zero drive!\n";
                return -1;
            }

            BytesLeft.QuadPart -= ONEMEG;
            cout << (float)(DriveSize.QuadPart - BytesLeft.QuadPart) / DriveSize.QuadPart  * 100.0f << "%\n";
        }

        while (BytesLeft.QuadPart >= 512)
        {
            // zero drive
            if (!WriteFile(hDrive, Buffer, 512, &Written, NULL))
            {
                cout << "Failed to zero drive!\n";
                return -1;
            }

            BytesLeft.QuadPart -= 512;
        }

        cout << "Done!\n";

        AnyKey();

        CloseHandle(hDrive);

        return 0;
    }
}

int DumpDrive()
{
    cout << "Select drive to open: ";

    char driveLetter;
    cin >> driveLetter;

    char pathName[7] = "\\\\.\\C:";
    pathName[4] = driveLetter;

    char driveName[5] = "R:\\";
    driveName[0] = driveLetter;

    cout << "About to open " << pathName << "\nSelect a file to save image to:" << endl;

    // open up drive whatever:
    HANDLE hDrive = CreateFileA(pathName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (hDrive == INVALID_HANDLE_VALUE)
    {
        cout << "Couldn't open drive\n";
        return -1;
    }

    ULARGE_INTEGER DriveSize;
    //GetFileSizeEx(hDrive, &DriveSize);

    GetDiskFreeSpaceExA(driveName,
        NULL,
        &DriveSize,
        NULL);

    cout << "Size: " << DriveSize.QuadPart / (1024 * 1024) << " megs\n";
    cout << "Size: " << DriveSize.QuadPart / (1024 * 1024 * 1024) << " gigs\n";

    OPENFILENAME ofn;

    WCHAR szFileName[MAX_PATH] = L"";

    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = L"Bin Files (*.bin)\0*.bin\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = (LPCWSTR)L"bin";

    if (!GetSaveFileName(&ofn))
    {
        cout << "User cancelled save dialog\n";
        return 0;
    }

    HANDLE hFile = CreateFile(ofn.lpstrFile, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Couldn't create ";
        wcout << ofn.lpstrFile << L"\n";
        return -1;
    }

    // Make the file sparse
    FILE_SET_SPARSE_BUFFER sparseBuffer;
    sparseBuffer.SetSparse = true;
    DWORD bytesReturned;
    BOOL returnValue = DeviceIoControl(hFile,
                                           FSCTL_SET_SPARSE,
                                           NULL,
                                           0,
                                           NULL,
                                           0,
                                           &bytesReturned,
                                           NULL);
    
    if (returnValue)
        printf("Success\n");
    else
        printf("Failure\n");

    // read one meg at a time until almost done
    LARGE_INTEGER BytesLeft;
    BytesLeft.QuadPart = DriveSize.QuadPart;
#define ONEMEG 1024 * 1024
    BYTE *Buffer = (BYTE*)malloc(ONEMEG);
    DWORD BufferOffset = 0;
    DWORD Read;
    while (BytesLeft.QuadPart >= ONEMEG)
    {
        // read drive
        if (!ReadFile(hDrive, Buffer, ONEMEG, &Read, NULL))
        {
            cout << "Failed to read drive!\n";
            return -1;
        }

        if (!WriteFile(hFile, Buffer, ONEMEG, &Read, NULL))
        {
            cout << "Unable to update file!\n";
            return -1;
        }

        BytesLeft.QuadPart -= ONEMEG;
        cout << (float)(DriveSize.QuadPart - BytesLeft.QuadPart) / DriveSize.QuadPart  * 100.0f << "%\n";
    }

    while (BytesLeft.QuadPart >= 512)
    {
        // read drive
        if (!ReadFile(hDrive, Buffer, 512, &Read, NULL))
        {
            cout << "Failed to read drive!\n";
            return -1;
        }

        if (!WriteFile(hFile, Buffer, 512, &Read, NULL))
        {
            cout << "Unable to update file!\n";
            return -1;
        }

        BytesLeft.QuadPart -= 512;
    }

    cout << "Done!\n";

    AnyKey();

    CloseHandle(hFile);
    CloseHandle(hDrive);

    return 0;
}

int WriteImageToDrive()
{
    OPENFILENAME ofn;

    WCHAR szFileName[MAX_PATH] = L"";

    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = L"Bin Files (*.bin)\0*.bin\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = (LPCWSTR)L"bin";

    if(!GetOpenFileName(&ofn))
    {
        cout << "User cancelled open dialog\n";
        return 0;
    }

    HANDLE hFile = CreateFile(ofn.lpstrFile, GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Couldn't create ";
        wcout << ofn.lpstrFile << L"\n";
        return -1;
    }

    cout << "Select drive to write an image to. This will of course OVERWRITE the contents (ESC to cancel): ";

    char driveLetter;
    driveLetter = getch();

    if (driveLetter == 27)
    {
        cout << "\nA wise decision\n";
        return 0;
    }

    if (driveLetter == 'c')
    {
        cout << "\nErase c:\\?!\nEven I'm not crazy enough to let you do that!\n";
        return 0;
    }

    char pathName[7] = "\\\\.\\C:";
    pathName[4] = driveLetter;

    char driveName[5] = "R:\\";
    driveName[0] = driveLetter;

    ClearScreen();
    promptLines = 3;

    cout << "About to COMPLETELY OVERWRITE " << driveName << "\nYou will lose all data on this drive if you continue!\n";

    cout << "You're totally sure you don't have any data on " << driveName << "?";
    int choice = GetMenuChoice(5, 0, "Exit", "Exit", "!Yes - Wipe Drive completely!", "Exit", "Exit");

    if (choice != 2)
        return 0;

    promptLines = 0;
    ClearScreen();

    promptLines = 2;
    cout << "Last chance - Totally sure you want to erase " << driveName << "?\nThis a bad idea if you have important data on ANY of your drives!\n";
    choice = GetMenuChoice(5, 4, "Exit", "Exit", "!Yes - Wipe Drive completely!", "Exit", "Exit");

    if (choice != 2)
        return 0;

    // open up drive whatever:
    HANDLE hDrive = CreateFileA(pathName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (hDrive == INVALID_HANDLE_VALUE)
    {
        cout << "Couldn't open drive\n";
        return -1;
    }

    ULARGE_INTEGER DriveSize;
    //GetFileSizeEx(hDrive, &DriveSize);

    GetDiskFreeSpaceExA(driveName,
                        NULL,
                        &DriveSize,
                        NULL);

    cout << "Size: " << DriveSize.QuadPart / (1024 * 1024) << " megs\n";
    cout << "Size: " << DriveSize.QuadPart / (1024 * 1024 * 1024) << " gigs\n";
    


    // write one meg at a time until almost done
    LARGE_INTEGER BytesLeft;
    BytesLeft.QuadPart = DriveSize.QuadPart;
#define ONEMEG 1024 * 1024
    BYTE *Buffer = (BYTE*)malloc(ONEMEG);
    DWORD BufferOffset = 0;
    DWORD Read;
    while (BytesLeft.QuadPart >= ONEMEG)
    {
        // read file and write to drive
        if (!ReadFile(hFile, Buffer, ONEMEG, &Read, NULL))
        {
            cout << "Failed to read image file!\n";
            return -1;
        }

        if (!WriteFile(hDrive, Buffer, ONEMEG, &Read, NULL))
        {
            cout << "Unable to write to file!\n";
            return -1;
        }

        BytesLeft.QuadPart -= ONEMEG;
        cout << (float)(DriveSize.QuadPart - BytesLeft.QuadPart) / DriveSize.QuadPart  * 100.0f << "%\n";
    }

    while (BytesLeft.QuadPart >= 512)
    {
        // read file and write to drive
        if (!ReadFile(hFile, Buffer, 512, &Read, NULL))
        {
            cout << "Failed to read drive!\n";
            return -1;
        }

        if (!WriteFile(hDrive, Buffer, 512, &Read, NULL))
        {
            cout << "Unable to update file!\n";
            return -1;
        }

        BytesLeft.QuadPart -= 512;
    }

    cout << "Done!\n";

    AnyKey();

    CloseHandle(hFile);
    CloseHandle(hDrive);

    return 0;
}

void TypeToAttrib(ULONG AttrType)
{

    switch (AttrType)
    {
        case AttributeFileName:
            htmlfile << " - $FILE_NAME";
            break;

        case AttributeStandardInformation:
            htmlfile << " - $STANDARD_INFORMATION";
            break;

        case AttributeObjectId:
            htmlfile << " - $OBJECT_ID";
            break;

        case AttributeSecurityDescriptor:
            htmlfile << " - $SECURITY_DESCRIPTOR";
            break;

        case AttributeVolumeName:
            htmlfile << " - $VOLUME_NAME";
            break;

        case AttributeVolumeInformation:
            htmlfile << " - $VOLUME_INFORMATION";
            break;

        case AttributeData:
            htmlfile << " - $DATA";
            break;

        case AttributeIndexRoot:
            htmlfile << " - $FILE_NAME";
            break;

        case AttributeIndexAllocation:
            htmlfile << " - $INDEX_ALLOCATION";
            break;

        case AttributeBitmap:
            htmlfile << " - $BITMAP";
            break;

        case AttributeReparsePoint:
            htmlfile << " - $REPARSE_POINT";
            break;

        case AttributeEAInformation:
            htmlfile << " - $EA_INFORMATION";
            break;

        case AttributeEA:
            htmlfile << " - $EA";
            break;

        case AttributePropertySet:
            htmlfile << " - $PROPERTY_SET";
            break;

        case AttributeLoggedUtilityStream:
            htmlfile << " - $LOGGED_UTILITY_STREAM";
            break;

        default:
            htmlfile << "  Attribute 0x" << std::setw(2) << std::hex << AttrType;
            break;
    }

    htmlfile << "&nbsp;&nbsp;&nbsp;&nbsp;";
}

// For testing IOCTL_DISK_GET_DRIVE_GEOMETRY_EX
void ViewDiskInfo()
{
    cout << "Enter letter of drive you'd like to inspect: ";

    char letter;
    cin >> letter;

    ULARGE_INTEGER driveSize;

    // open the drive
    HANDLE hDrive = OpenDrive(letter, &driveSize);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        cout << "Failed to open " << letter << " drive!\n";
        return;
    }

    DISK_GEOMETRY_EX geometryEx;
    DWORD bytesReturned;

    // Issue the IOCTL
    if (!DeviceIoControl(
        hDrive,                 // handle to device
        IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, // dwIoControlCode
        NULL,                             // lpInBuffer
        0,                                // nInBufferSize
        (LPVOID)&geometryEx,             // output buffer
        sizeof(geometryEx),           // size of output buffer
        &bytesReturned,        // number of bytes returned
        NULL       // OVERLAPPED structure
    ))
    {
        cout << "DeviceIoControl failed.\n";
        return;
    }
    //else
   //     cout << "success\n";

    cout << "Drive " << letter << " has " << geometryEx.DiskSize.QuadPart << " bytes.\n ";
    cout << geometryEx.Geometry.Cylinders.QuadPart << " cyl " << geometryEx.Geometry.TracksPerCylinder << " tracks per cyl\n "
        << geometryEx.Geometry.SectorsPerTrack << " sectors per track " << geometryEx.Geometry.BytesPerSector << " bytes per sector\n";

    AnyKey();
}

/*void VcnToMFTIndex(ULONGLONG Vcn)
{
	// hacky hacky
	// for this volume, 512 bytes = cluster; 2 clusters = 1 index
}*/

bool ClusterToFile(ULONGLONG Cluster, ULONG &Vcn)
{
    ULONGLONG mftOffset = 0;

    int maxRecords = mftSize / 1024;

    for (int i = 0; i < maxRecords; i++)
    {
        NTSTATUS Status;
        FIND_ATTR_CONTXT Context;
        PNTFS_ATTR_RECORD Attribute;

        Status = FindFirstAttribute(&Context, &Vcb, &mft[i], FALSE, &Attribute);
        while (NT_SUCCESS(Status))
        {
            if (Attribute->IsNonResident)
            {
                LONGLONG  DataRunOffset;
                ULONGLONG DataRunLength;
                LONGLONG  DataRunStartLCN;
                ULONGLONG LastLCN = 0;
				Vcn = 0;

                PUCHAR DataRun = (PUCHAR)Attribute + Attribute->NonResident.MappingPairsOffset;
                
                while (*DataRun != 0)
                {
                    DataRun = DecodeRun(DataRun, &DataRunOffset, &DataRunLength);

                    if (DataRunOffset != -1)
                    {
                        // Normal data run.
                        DataRunStartLCN = LastLCN + DataRunOffset;
                        LastLCN = DataRunStartLCN;

                        if (Cluster >= DataRunStartLCN && Cluster <= (DataRunStartLCN + DataRunLength - 1))
                        {
                            htmlfile << " [";
                            // get the file's name
                            PFILENAME_ATTRIBUTE fileNameAttr = GetBestFileNameFromRecord(&Vcb, &mft[i]);
                            WCHAR pathName[MAX_PATH];

                            memcpy(pathName, fileNameAttr->Name, fileNameAttr->NameLength * sizeof(WCHAR));
                            pathName[fileNameAttr->NameLength] = UNICODE_NULL;
                            char buffer[MAX_PATH];
                            memset(buffer, 0, MAX_PATH);
                            
                            wcstombs(buffer, pathName, MAX_PATH);

                            htmlfile << buffer << "]  ";
							TypeToAttrib(Attribute->Type);

							if (strcmp(buffer, "$MFT") == 0 && Attribute->Type == AttributeData)
							{
								// Convert Cluster to Vcn
								while (DataRunStartLCN != Cluster)
								{
									DataRunStartLCN++;
									Vcn++;
								}
								return true;
							}
							return false;
                        }
                    }
					else
					{
						Vcn += DataRunLength;
					}
                }
            }

            Status = FindNextAttribute(&Context, &Attribute);
        }

        FindCloseAttribute(&Context);
    }
	return false;
}

int GetMenuChoice(int numChoices, int defaultSelection, char *szChoices, ...)
{
    va_list choiceList;
    int i;
    int selection = defaultSelection;

    COORD cur = { 0, 0 };

    bool usingPrompt = false;
    
    ClearScreen();

    if (promptLines > 0)
    {
        usingPrompt = true;
        cur.Y = promptLines;
    }       
   
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    
    bool done = false;
    while (!done)
    {
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), cur);
        va_start(choiceList, szChoices);
        char *string = szChoices; //va_arg(choiceList, char *);

        for (i = 0; i < numChoices; ++i)
        {
            if (selection == i)
                SetConsoleTextAttribute(hConsole, 0xF0);
            else
                SetConsoleTextAttribute(hConsole, 0x0F);

            cout << i << ") " << string << endl;
            string = va_arg(choiceList, char *);
        }

        //cout << selection << endl;

        char key = getch();

        if (key == -32)
        {
            switch (getch())
            {
                case 72:
                    // up arrow
                    selection--;
                    break;
                case 80:
                    // down arrow
                    selection++;
                    break;
            }
            if (selection < 0)
                selection = numChoices - 1;

            selection = selection % numChoices;
        }

        if (key == '0')
            return 0;
        if (key == '1')
            return 1;
        if (key == '2')
            return 2;
        if (key == '3')
            return 3;
        if (key == '4')
            return 4;
        if (key == '5')
            return 5;
        if (key == '6')
            return 6;
        if (key == '7')
            return 7;
        if (key == '8')
            return 8;
        if (key == '9')
            return 9;

        if (key == 27)
        {
            selection = defaultSelection;
            SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), cur);
            va_start(choiceList, szChoices);
            char *string = szChoices; //va_arg(choiceList, char *);

            for (i = 0; i < numChoices; ++i)
            {
                if (selection == i)
                    SetConsoleTextAttribute(hConsole, 0xF0);
                else
                    SetConsoleTextAttribute(hConsole, 0x0F);

                cout << i << ") " << string << endl;
                string = va_arg(choiceList, char *);
            }

            SetConsoleTextAttribute(hConsole, 0x0F);

            return defaultSelection;
        }

        if (key == 13)
        {
            SetConsoleTextAttribute(hConsole, 0x0F);
            return selection;
        }

        //cout << (int)key << endl;
    }

    va_end(choiceList);

    return i;
}

HANDLE OpenDrive(char driveLetter, PULARGE_INTEGER pDriveSize)
{
    char pathName[7] = "\\\\.\\C:";
    pathName[4] = driveLetter;

    char driveName[5] = "R:\\";
    driveName[0] = driveLetter;

    cout << "About to open " << pathName << "\nSelect a file to save image to:" << endl;

    // open up drive whatever:
    HANDLE hDrive = CreateFileA(pathName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (hDrive == INVALID_HANDLE_VALUE)
    {
        cout << "Couldn't open drive\n";
        return INVALID_HANDLE_VALUE;
    }

    GetDiskFreeSpaceExA(driveName,
                        NULL,
                        pDriveSize,
                        NULL);
    return hDrive;
}

int main()
{
    int choice = GetMenuChoice(6, 0, "Dump Drive", "Compare two files", "Compare two drives", "Write image to drive", /*"!Wipe Drive completely!",*/ "View disk info", "Exit");

    bool comparingDrives = false;

    if (choice == 0)
    {
        DumpDrive();
        return 0;
    }

    if (choice == 2)
    {
        comparingDrives = true;
    }

    if (choice == 3)
    {
        WriteImageToDrive();
        return 0;
    }

    /*if (choice == 4)
    {
        WipeDrive();
        return 0;
    }*/

    if (choice == 4)
    {
        ViewDiskInfo();
        return 0;
    }

    if (choice == 5)
    {
        return 0;
    }

    HANDLE hBefore, hAfter;
    ULARGE_INTEGER FileSize1, FileSize2;

    if (!comparingDrives)
    {
        OPENFILENAME ofn, ofn2;

        WCHAR szFileName[MAX_PATH] = L"";
        WCHAR szFileName2[MAX_PATH] = L"";

        cout << "Open first file...\n";

        ZeroMemory(&ofn, sizeof(ofn));

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = L"Bin Files (*.bin)\0*.bin\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = szFileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrDefExt = (LPCWSTR)L"bin";

        if (!GetOpenFileName(&ofn))
        {
            cout << "User cancelled save dialog\n";
            return 0;
        }

        HANDLE hBefore = CreateFile(ofn.lpstrFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
        if (hBefore == INVALID_HANDLE_VALUE)
        {
            cout << "Couldn't create ";
            wcout << ofn.lpstrFile << L"\n";
            return -1;
        }

        cout << "Open 2nd file...\n";

        ZeroMemory(&ofn2, sizeof(ofn2));
        //ZeroMemory(szFileName,(2 * MAX_PATH));

        ofn2.lStructSize = sizeof(ofn2);
        ofn2.hwndOwner = NULL;
        ofn2.lpstrFilter = L"Bin Files (*.bin)\0*.bin\0All Files (*.*)\0*.*\0";
        ofn2.lpstrFile = szFileName2;
        ofn2.nMaxFile = MAX_PATH;
        ofn2.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn2.lpstrDefExt = (LPCWSTR)L"bin";



        if (!GetOpenFileName(&ofn2))
        {
            cout << "User cancelled save dialog\n";
            return 0;
        }

        HANDLE hAfter = CreateFile(ofn2.lpstrFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
        if (hAfter == INVALID_HANDLE_VALUE)
        {
            cout << "Couldn't create after file";
            wcout << ofn2.lpstrFile << L"\n";
            return -1;
        }

        GetFileSizeEx(hBefore, (LARGE_INTEGER *)(&FileSize1));
        GetFileSizeEx(hAfter, (LARGE_INTEGER *)(&FileSize2));

        if (FileSize1.QuadPart != FileSize2.QuadPart)
        {
            cout << "Files not equal in size!\n";
            return -1;
        }
        else
            cout << "Yes these files totally match in size\n";
    }
    else
    {
        // comparing two drives directly
        cout << "Enter letter of the first drive: ";

        char driveLetter;
        cin >> driveLetter;

        // open up drive whatever:
        LARGE_INTEGER Drive1Size, Drive2Size;
        hBefore = OpenDrive(driveLetter, &FileSize1);
        if (hBefore == INVALID_HANDLE_VALUE)
        {
            cout << "Couldn't open drive " << driveLetter << endl;
            return -1;
        }

        cout << "Enter letter of the second drive: ";

        cin >> driveLetter;

        // open up drive whatever:
        hAfter = OpenDrive(driveLetter, &FileSize2);
        if (hAfter == INVALID_HANDLE_VALUE)
        {
            cout << "Couldn't open drive " << driveLetter << endl;
            return -1;
        }
    }


    // read the mft from the second file
    // now let's find the start of the mft

    // copy the boot file
    BOOTFILE bootFile;
    DWORD Read;
    cout << "Size of Boot file struct: " << sizeof(BOOTFILE) << "\n";
    cout << "Size of mft entry struct: " << sizeof(MFTENTRY) << "\n";
    if (!ReadFile(hAfter, &bootFile, 512, &Read, NULL))
    {
        cout << "Unable to read boot file from second drive or image. Error code: " << GetLastError() << "\n";
        CloseHandle(hAfter);
        return -1;
    }

    Vcb.hDrive = hAfter;
    Vcb.Vpb = &bootFile;

    /*    BYTE mftentry;
    memcpy(&mftentry, bootFile, 1);
    cout << (int)mftentry << "\n";
    */
    // output some values
    cout << std::hex << bootFile.Signature << "\n"; // should be 0xaa55
    cout << std::dec << bootFile.BytesPerSector << "\n";
    cout << "Sectors per cluster: " << (int)bootFile.SectorsPerCluster << "\n";
    cout << "Media Descriptor: " << (int)bootFile.MediaDesc << "\n";
    //cout << bootFile.SectorsInFileSystem.QuadPart << "\n";
    //cout << (int)bootFile.Unused6 << "\n";
    //cout << "Mft entry size: " << (int)bootFile.SizeOfMftEntry << "\n";
    //cout << std::hex << bootFile.SerialNumber.HighPart << "-" << bootFile.SerialNumber.LowPart << "\n";

    LARGE_INTEGER FileSystemSize;
    FileSystemSize.QuadPart = bootFile.BytesPerSector * bootFile.SectorsInFileSystem.QuadPart;
    cout << std::dec << FileSystemSize.QuadPart << "\n";

    cout << FileSystemSize.QuadPart / 1024 / 1024 / 1024 << " gigs\n";

    cout << "First MFT Entry: " << std::hex << bootFile.MftFirstCluster.QuadPart << "\n";

    // find the mft entry
    LARGE_INTEGER mftStart;
    mftStart.QuadPart = bootFile.BytesPerSector * bootFile.SectorsPerCluster * bootFile.MftFirstCluster.QuadPart;

    cout << mftStart.QuadPart << "\n";

    if (!SetFilePointerEx(hAfter, mftStart, NULL, FILE_BEGIN))
        cout << "Unable to set file pointer to " << mftStart.QuadPart << "\nError: " << GetLastError() << "\n";

    memset(&MasterFileTableMftEntry, 0, 1024);
    if (!ReadFile(hAfter, &MasterFileTableMftEntry, 1024, &Read, NULL))
    {
        cout << "unable to read mft entry. Error: " << GetLastError() << "\n";
        return -1;
    }

    cout << std::hex << MasterFileTableMftEntry.Signature << "\n";

    //SetFilePointerEx(hDrive, mftStart, NULL, FILE_BEGIN);

    //DumpAttributes(MasterFileTableMftEntry);

    NtfsDumpFileAttributes(&Vcb, &MasterFileTableMftEntry);



    //NTFS_ATTR_CONTEXT Context;

    // now we need to find the data attribute for the master file table
    NTSTATUS Status = FindAttribute(&Vcb, &MasterFileTableMftEntry, AttributeData, L"", 0, &MftContext, NULL);
    if (!NT_SUCCESS(Status))
    {
        cout << "Can't find data attribute for Master File Table.\n";
        //free(DeviceExt->MasterFileTable);
        return Status;
    }

    MasterFileTableDataRecord = MftContext->Record;

    // now that we've found the mft data attribute, let's try to read it!
    mftSize = AttributeDataLength(&MftContext->Record);

	mftSize = /*min(1024 * 1024 * 1024, */AttributeDataLength(&MftContext->Record); //);

    cout << "\n\nSize of Master File Table: " << mftSize << "\n\n\n";

    //PMFTENTRY 
    mft = (PMFTENTRY)malloc(mftSize); //(PMFTENTRY)calloc(mftSize / 1024, 1024); 

    if (mft == NULL)
        cout << "Couldn't allocate memory for mft!!\n";

    ReadAttribute(&Vcb, MftContext, 0, (PCHAR)mft, (ULONG)mftSize);

    SetFilePointer(hAfter, 0, NULL, FILE_BEGIN);
	
    LARGE_INTEGER Offset;
    
    htmlfile.open("output.html", std::ios::out | std::ios::trunc);

    std::ofstream cssfile;
    cssfile.open("machinestyles.css", std::ios::out | std::ios::trunc);

    htmlfile << "<!DOCTYPE HTML>\n        <html lang = \"en\">\n        <head>\n";

    htmlfile << "        <title>Differences</title>\n        <meta http-equiv=\"Content-Type\" content = \"text/html; charset=utf-8\">\n        <link href = \"styles.css\" type = \"text/css\" rel = \"stylesheet\">\n        <link href = \"machinestyles.css\" type = \"text/css\" rel = \"stylesheet\">\n        </head>\n        <body>\n";

    htmlfile << "\n";

    htmlfile << "<p>Differences:</p>\n";

    cssfile << "body{\n\r\tmargin: 10px;\n\rfont-family: Lucida Console;\n\r}";

    BYTE Buffer1[512];
    BYTE Buffer2[512];

    LARGE_INTEGER BytesLeft;
    BytesLeft.QuadPart = FileSize1.QuadPart;
    Offset.QuadPart = 0;
//    DWORD Read;

    cout << "File size: " << FileSize1.QuadPart << "\n";

    while (BytesLeft.QuadPart > 0)
    {
        if (!ReadFile(hBefore, Buffer1, 512, &Read, NULL))
        {
            cout << "Couldn't read from before file\n";
            return -1;
        }

        if (!ReadFile(hAfter, Buffer2, 512, &Read, NULL))
        {
            cout << "Couldn't read from after file\n";
            return -1;
        }

        if (memcmp(Buffer1, Buffer2, 512))
        {
            bool sectorChanged = false;
            htmlfile << std::setw(4) << std::dec << Offset.QuadPart;
            /*if (Offset.QuadPart >= 8858 && Offset.QuadPart <= 12953)
                htmlfile << "&nbsp[$LogFile]";
            else*/
            {
                /*if (Offset.QuadPart >= 12970 && Offset.QuadPart <= 13065)
                    htmlfile << "&nbsp;[$Mft]";
                else
                {*/
                
                    // map sector to file
                ULONG Vcn;
				if (ClusterToFile(Offset.QuadPart, Vcn))
				{
					//htmlfile << "&nbsp;&nbsp;&nbsp;&nbsp; - " << Vcn << " - ";
					if (Vcn % 2 == 0)
					{
						int index = Vcn / 2;
						PMFTENTRY FileRecord = &mft[index];

						// get the file's name
                        PFILENAME_ATTRIBUTE fileNameAttr = GetBestFileNameFromRecord(&Vcb, &mft[index]);
                        if (fileNameAttr != NULL)
                        {
                            WCHAR pathName[MAX_PATH];

                            memcpy(pathName, fileNameAttr->Name, fileNameAttr->NameLength * sizeof(WCHAR));
                            pathName[fileNameAttr->NameLength] = UNICODE_NULL;
                            char buffer[MAX_PATH];
                            memset(buffer, 0, MAX_PATH);

                            wcstombs(buffer, pathName, MAX_PATH);

                            htmlfile << " &lt;";
                            htmlfile << buffer;
                            htmlfile << "&gt;  ";
                        }
                        else
                        {
                            htmlfile << " &lt;ERROR&gt;";
                        }
						
						htmlfile << "&nbsp;&nbsp;&nbsp;&nbsp; - Record Index: " << Vcn / 2  << " - ";

						// create a file for the new mft entry
						HANDLE hNewRecord;

						/*if (index <= 32)
						{
							std::wstringstream strstr;
							strstr << L"Index" << index << L".bin";
							hNewRecord = CreateFile(strstr.str().c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
						}
						else
							hNewRecord = CreateFile(pathName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
						
						if (hNewRecord == INVALID_HANDLE_VALUE)
						{
							cout << "Couldn't create ";
							wcout << pathName << L"\n";
							return -1;
						}

						// now we need to find the data attribute for the current file
						
						NTSTATUS Status = FindAttribute(&Vcb, &mft[index], AttributeData, L"", 0, &FileContext, NULL);
						if (!NT_SUCCESS(Status))
						{
							wcout << L"Can't find data attribute for " << pathName  << L".\n";
						}
						else
						{
							// now that we've found the data attribute, let's try to read it!
							ULONGLONG DataSize = AttributeDataLength(&FileContext->Record);

							DataSize = AttributeDataLength(&FileContext->Record);

							PFILE_RECORD_HEADER ChangedFile = (PMFTENTRY)malloc(DataSize);

							if (ChangedFile == NULL)
								cout << "Couldn't allocate memory for ChangedFile!!\n";

							ReadAttribute(&Vcb, FileContext, 0, (PCHAR)ChangedFile, (ULONG)DataSize);

							void *NewBuffer = malloc(DataSize);

							if (!WriteFile(hNewRecord, NewBuffer, DataSize, &Read, NULL))
							{
								cout << "Couldn't write to file!\n";
								cout << GetLastError();
							}

							free(NewBuffer);

							CloseHandle(hNewRecord);
						}*/
					}
				}
                //}
                
                //if(1)
                //{
                int firstByte = 0;
                int lastByte = 511;
                while (!memcmp(Buffer1 + firstByte, Buffer2 + firstByte, 1))
                {
                    firstByte++;
                }

                while (!memcmp(Buffer1 + lastByte, Buffer2 + lastByte, 1))
                {
                    lastByte--;
                }

                htmlfile << "&nbsp;" << firstByte << " - " << lastByte;

                if (firstByte == 510)
                {
                    WORD fsn1 = (BYTE)*(Buffer1 + 510);
                    WORD fsn2 = (BYTE)*(Buffer2 + 510);

                    if (fsn2 == fsn1 + 1)
                    {
                        htmlfile << "&nbsp;(Fixup Sequence number increased from " << fsn1 << " to " << fsn2 << ")";
                    }
                    else
                    {
                        htmlfile << " &nbsp;&nbsp;" << fsn1 << " -&gt; " << fsn2;
                        htmlfile << "<br>\n";

                        for (int i = 0; i < 512; i += 8)
                        {
                            if (memcmp(Buffer1 + i, Buffer2 + i, 8))
                            {
                                htmlfile << std::setfill('0') << std::hex << "&nbsp;&nbsp;" << std::setw(3) << i << "&nbsp; &nbsp;&nbsp; ";
                                    
                                for (int j = 0; j < 8; j++)
                                {
                                    htmlfile << std::setw(2) << std::hex << std::setfill('0') << (int)*(Buffer1 + i + j) << "&nbsp;";
                                }

                                htmlfile << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";

                                for (int j = 0; j < 8; j++)
                                {
                                    htmlfile << std::setw(2) << std::hex << std::setfill('0') << (int)*(Buffer2 + i + j) << "&nbsp;";
                                }
                                htmlfile << "<br>\n";
                            }
                        }
                    }             
                }
                else
                {
                    // firstByte != 510
                    htmlfile << "<br>\n";

                    for (int i = 0; i < 512; i += 8)
                    {
                        if (memcmp(Buffer1 + i, Buffer2 + i, 8))
                        {
                            htmlfile << std::setfill('0') << std::hex << "&nbsp;&nbsp;" << std::setw(3) << i << "&nbsp; &nbsp;&nbsp; ";
                            //htmlfile << "<font color=#FF0000>";
                            for (int j = 0; j < 8; j++)
                            {
                                htmlfile << std::setw(2) << std::hex << std::setfill('0') << (int)*(Buffer1 + i + j) << "&nbsp;";
                            }

                            htmlfile << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";

                            for (int j = 0; j < 8; j++)
                            {
                                htmlfile << std::setw(2) << std::hex << std::setfill('0') << (int)*(Buffer2 + i + j) << "&nbsp;";
                            }
                            htmlfile << "<br>\n";
                            sectorChanged = true;
                        }
                    }
                }
                //}

                //htmlfile << "\n<br>";
                
            }
            //if( !sectorChanged )
                htmlfile << "\n<br>";
        }


        Offset.QuadPart += 1;
        BytesLeft.QuadPart -= 512;
    }

    htmlfile.close();
    cssfile.close();

    ShellExecute(NULL, NULL, L"output.html", NULL, NULL, 0);

    return 0;
}