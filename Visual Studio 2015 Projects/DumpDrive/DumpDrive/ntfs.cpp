#include "stdafx.h"
#include "ntfs.h"
#include <cstdio>

int infoRange::instance = 0;

PUCHAR
DecodeRun(PUCHAR DataRun,
    LONGLONG *DataRunOffset,
    ULONGLONG *DataRunLength)
{
    UCHAR DataRunOffsetSize;
    UCHAR DataRunLengthSize;
    CHAR i;

    DataRunOffsetSize = (*DataRun >> 4) & 0xF;
    DataRunLengthSize = *DataRun & 0xF;
    *DataRunOffset = 0;
    *DataRunLength = 0;
    DataRun++;
    for (i = 0; i < DataRunLengthSize; i++)
    {
        *DataRunLength += ((ULONG64)*DataRun) << (i * 8);
        DataRun++;
    }

    // NTFS 3+ sparse files 
    if (DataRunOffsetSize == 0)
    {
        *DataRunOffset = -1;
    }
    else
    {
        for (i = 0; i < DataRunOffsetSize - 1; i++)
        {
            *DataRunOffset += ((ULONG64)*DataRun) << (i * 8);
            DataRun++;
        }
        // The last byte contains sign so we must process it different way.
        *DataRunOffset = ((LONG64)(CHAR)(*(DataRun++)) << (i * 8)) + *DataRunOffset;
    }

    /*DPRINT("DataRunOffsetSize: %x\n", DataRunOffsetSize);
    DPRINT("DataRunLengthSize: %x\n", DataRunLengthSize);
    DPRINT("DataRunOffset: %x\n", *DataRunOffset);
    DPRINT("DataRunLength: %x\n", *DataRunLength);*/

    return DataRun;
}

void DbgPrint(char *string)
{
    cout << string << "\n";
}

VOID
NtfsDumpVolumeNameAttribute(PNTFS_ATTR_RECORD Attribute)
{
    PWCHAR VolumeName;

    DbgPrint("  $VOLUME_NAME ");

    //    DbgPrint(" Length %lu  Offset %hu ", Attribute->Resident.ValueLength, Attribute->Resident.ValueOffset);

    VolumeName = (PWCHAR)((ULONG_PTR)Attribute + Attribute->Resident.ValueOffset);
    wcout << Attribute->Resident.ValueLength / sizeof(WCHAR) << VolumeName << "\n";
}

VOID
NtfsDumpVolumeInformationAttribute(PNTFS_ATTR_RECORD Attribute)
{
    PVOLINFO_ATTRIBUTE VolInfoAttr;

    DbgPrint("  $VOLUME_INFORMATION ");

    //    DbgPrint(" Length %lu  Offset %hu ", Attribute->Resident.ValueLength, Attribute->Resident.ValueOffset);

    VolInfoAttr = (PVOLINFO_ATTRIBUTE)((ULONG_PTR)Attribute + Attribute->Resident.ValueOffset);
    cout << " NTFS Version " << VolInfoAttr->MajorVersion << "." <<
        VolInfoAttr->MinorVersion << "\t\t" <<
        VolInfoAttr->Flags << "\n";
}

VOID
NtfsDumpFileAttributes(PDEVICE_EXTENSION Vcb,
    PFILE_RECORD_HEADER FileRecord)
{
    NTSTATUS Status;
    FIND_ATTR_CONTXT Context;
    PNTFS_ATTR_RECORD Attribute;

    wcout << L"\n\nCalled\n\n";

    Status = FindFirstAttribute(&Context, Vcb, FileRecord, FALSE, &Attribute);
    while (NT_SUCCESS(Status))
    {
        //DPRINT1("Offset: %lu\n", Context.Offset);
        //DPRINT1("Offset2: %lu\n", (PCHAR)Attribute - (PCHAR)FileRecord);
        cout << "Attribute:\n";
        NtfsDumpAttribute(Vcb, Attribute);
        cout << "\n";
        Status = FindNextAttribute(&Context, &Attribute);
    }

    FindCloseAttribute(&Context);
}

VOID
NtfsDumpAttribute(PDEVICE_EXTENSION Vcb,
    PNTFS_ATTR_RECORD Attribute)
{
    UNICODE_STRING Name;

    ULONGLONG lcn = 0;
    ULONGLONG runcount = 0;

    switch (Attribute->Type)
    {
    case AttributeFileName:
        NtfsDumpFileNameAttribute(Attribute);
        break;

    case AttributeStandardInformation:
        NtfsDumpStandardInformationAttribute(Attribute);
        break;

    case AttributeObjectId:
        DbgPrint("  $OBJECT_ID ");
        break;

    case AttributeSecurityDescriptor:
        DbgPrint("  $SECURITY_DESCRIPTOR ");
        break;

    case AttributeVolumeName:
        NtfsDumpVolumeNameAttribute(Attribute);
        break;

    case AttributeVolumeInformation:
        NtfsDumpVolumeInformationAttribute(Attribute);
        break;

    case AttributeData:
        DbgPrint("  $DATA ");
        //DataBuf = ExAllocatePool(NonPagedPool,AttributeLengthAllocated(Attribute));
        break;

    case AttributeIndexRoot:
        NtfsDumpIndexRootAttribute(Attribute);
        break;

    case AttributeIndexAllocation:
        DbgPrint("  $INDEX_ALLOCATION ");
        break;

    case AttributeBitmap:
        DbgPrint("  $BITMAP ");
        break;

    case AttributeReparsePoint:
        DbgPrint("  $REPARSE_POINT ");
        break;

    case AttributeEAInformation:
        DbgPrint("  $EA_INFORMATION ");
        break;

    case AttributeEA:
        DbgPrint("  $EA ");
        break;

    case AttributePropertySet:
        DbgPrint("  $PROPERTY_SET ");
        break;

    case AttributeLoggedUtilityStream:
        DbgPrint("  $LOGGED_UTILITY_STREAM ");
        break;

    default:
        cout << "  Attribute  " << Attribute->Type;
        break;
    }

    if (Attribute->Type != AttributeAttributeList)
    {
        if (Attribute->NameLength != 0)
        {
            Name.Length = Attribute->NameLength * sizeof(WCHAR);
            Name.MaximumLength = Name.Length;
            Name.Buffer = (PWCHAR)((ULONG_PTR)Attribute + Attribute->NameOffset);

            WCHAR Name2[260];
            memset(Name2, 0, 260 * 2);

            memcpy(Name2, (PCHAR)Attribute + Attribute->NameOffset, Attribute->NameLength * 2);
            char ch[260];
            char DefChar = ' ';
            WideCharToMultiByte(CP_ACP, 0, Name2, -1, ch, 260, &DefChar, NULL);

            std::stringstream namestream;
            namestream << "Name: " << ch;

            cout << namestream.str() << "\n";

            //wcout << "Name: " << Name.Buffer << L"\n\n";
        }

        cout << (Attribute->IsNonResident ? "non-resident" : "resident") << "\n";

        if (Attribute->IsNonResident)
        {
            FindRun(Attribute, 0, &lcn, &runcount);

            cout << "  AllocatedSize " << Attribute->NonResident.AllocatedSize << " data size "
                << Attribute->NonResident.DataSize << "\n";

            cout << "  logical clusters: " << lcn << " - " << lcn + runcount - 1 << "\n";
        }
    }
}

void fillupAttributeHeader(infoRange *pRange, PNTFS_ATTR_RECORD record)
{
    int headerLength;
    std::stringstream classname;

    if (record->IsNonResident)
    {
        classname << "Non-Resident";
        headerLength = record->NonResident.MappingPairsOffset;
    }
    else
    {
        // resident
        classname << "Resident";
        headerLength = record->Resident.ValueOffset;
    }

    classname << " attribute header";

    //infoRange *pHeader = new infoRange(0, headerLength, pRange->pOutput, pRange->pCSS, "AttHeader", "attheaderLegend", classname.str(), TRUE, false, true);
    //pRange->pChildren.push_back(pHeader);
    infoRange *pHeader = pRange->AddChild(0, headerLength, "AttHeader", "AttHeaderLegend", classname.str(), true, false);

    if (record->NameLength != 0)
    {
        WCHAR Name[260];
        memset(Name, 0, 260 * 2);

        memcpy(Name, (PCHAR)record + record->NameOffset, record->NameLength * 2);
        char ch[260];
        char DefChar = ' ';
        WideCharToMultiByte(CP_ACP, 0, Name, -1, ch, 260, &DefChar, NULL);

        std::stringstream namestream;
        namestream << "Name - " << ch;

        infoRange *pName = new infoRange(record->NameOffset, record->NameLength * 2, pRange->pOutput, pRange->pCSS, "FileNameClass", "FNCLegend", namestream.str(), TRUE);
        pHeader->pChildren.push_back(pName);
    }

    pHeader->AddChild(0, 4, "Type", "TLegend", "Attribute Type");
    pHeader->AddChild(4, 4, "Length", "LLegend", "Attribute Length");
    pHeader->AddChild(8, 1, "NonRes", "NRLEgend", "Non-Resident Flag");
    pHeader->AddChild(9, 1, "NameLength", "NLLegend", "Name Length");
    pHeader->AddChild(10, 2, "NameOffset", "NOLegend", record->NameLength > 0 ? "Offset to Name" : "Offset to Data");
    pHeader->AddChild(0x0c, 2, "Flags", "FLegend", "Flags");
    pHeader->AddChild(0x0e, 2, "id", "idLegend", "Attribute ID");

    if (record->IsNonResident)
    {
        pHeader->AddChild(0x10, 8, "LowestVCN", "LVLegend", "Lowest VCN");
        pHeader->AddChild(0x18, 8, "HighestVCN", "HVLegend", "Highest VCN");
        pHeader->AddChild(0x20, 2, "MappingPairsOffset", "MPOLegend", "Offset to Data Run");
        pHeader->AddChild(0x22, 2, "CompressionUnit", "CULegend", "Compression Unit");
        pHeader->AddChild(0x24, 4, "Reserved", "RLegend", "Reserved");
        pHeader->AddChild(0x28, 8, "AllocSize", "ASLegend", "Allocated Size");
        pHeader->AddChild(0x30, 8, "DataSize", "DSLegend", "Data Size");
        pHeader->AddChild(0x38, 8, "InitSize", "ISLegend", "Initialized Size");
        //pHeader->AddChild(0x40, 8, "CompSize", "CSLegend", "Compressed Size");

    }
    else
    {
        pHeader->AddChild(0x10, 4, "length", "LLegend", "Length of Attribute");
        pHeader->AddChild(0x14, 2, "AttrOffset", "AOLegend", "Offset to Attribute");
        pHeader->AddChild(0x16, 1, "IndexedFlag", "IFLegend", "Indexed Flag");
        pHeader->AddChild(0x17, 1, "Padding", "PLegend", "Padding");
    }
}

void fillupInfoRange(infoRange *pRange, PNTFS_ATTR_RECORD record)
{
    infoRange *pAtt;
    infoRange *pData;
    infoRange *pIndexRoot;
    infoRange *pStandardInfoRange;
    std::stringstream legend;
    PFILENAME_ATTRIBUTE pFileName;
    PSTANDARD_INFORMATION pStandardInfo;
    ULONG Parent, Allocated, Real;
    ULONGLONG LParent;
    UNICODE_STRING Name;
    char ch[260];
    char DefChar = ' ';
    FILETIME ft;
    SYSTEMTIME st;
    WCHAR szLocalDate[255], szLocalTime[255];

    switch (record->Type)
    {
    case AttributeStandardInformation:
        pRange->AddChild(0, record->Length, "StdInfo", "SILegend", "$STANDARD_INFORMATION Attribute", true);
        pRange = pRange->pChildren.back();
        fillupAttributeHeader(pRange, record);

        pStandardInfoRange = pRange->AddChild(record->Resident.ValueOffset, record->Resident.ValueLength, "STDINFOData", "SILegend", "$STANDARD_INFORMATION DATA", true);
        pStandardInfo = (PSTANDARD_INFORMATION)((PCHAR)record + record->Resident.ValueOffset);
        

        legend.str("");
        legend.clear();

        legend << "Creation Time - ";

        memcpy(&ft,&pStandardInfo->CreationTime,sizeof(FILETIME));

        FileTimeToLocalFileTime(&ft, &ft);
        FileTimeToSystemTime(&ft, &st);
        GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL,
                      szLocalDate, 255);

        WideCharToMultiByte(CP_ACP, 0, szLocalDate, -1, ch, 255, &DefChar, NULL);

        legend << ch << ", ";
        GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, szLocalTime, 255);
        //printf("%s %s\n", szLocalDate, szLocalTime);


        WideCharToMultiByte(CP_ACP, 0, szLocalTime, -1, ch, 255, &DefChar, NULL);
        legend << ch;

        pStandardInfoRange->AddChild(0, 8, "CTime", "CTLegend", legend.str());

        break;
    case AttributeFileName:
        pAtt = pRange->AddChild(0, record->Length, "AttrFileName", "AFNLegend", "$FILE_NAME Attribute", true);
        
        //pRange = pRange->pChildren.back();
                         
        fillupAttributeHeader(pAtt, record);
        pData = pAtt->AddChild(record->Resident.ValueOffset, record->Resident.ValueLength, "FILE_NAMEData", "FNDLegend", "$FILE_NAME DATA", true);
        
        pFileName = (PFILENAME_ATTRIBUTE)((PCHAR)record + record->Resident.ValueOffset);

        LParent = (pFileName->DirectoryFileReferenceNumber & NTFS_MFT_MASK);
        Parent = LParent;
        legend << "Parent Directory MFT Index (" << Parent << ")";
        pData->AddChild(0, 8, "ParentMft", "PMFTLegend", legend.str());
        pData->AddChild(8, 8, "FileCreate", "FCLegend", "File Creation Time");
        pData->AddChild(16, 8, "FileMod", "FMLegend", "File Modification Time");
        pData->AddChild(24, 8, "MftMod", "MMLegend", "MFT Modification Time");
        pData->AddChild(32, 8, "FileAccess", "FATLegend", "File Access Time");

        legend.str("");
        legend.clear();
        legend << "Allocated size of file - ";
        Allocated = (ULONG)pFileName->AllocatedSize;
        legend << Allocated << " bytes";
        pData->AddChild(40, 8, "AllocSize", "ASLegend", legend.str());

        legend.str("");
        legend.clear();
        legend << "Size of file - ";
        Real = (ULONG)pFileName->DataSize;
        legend << Allocated << " bytes";
        pData->AddChild(48, 8, "AllocSize", "ASLegend", legend.str());

        legend.str("");
        legend.clear();
        legend << "Flags - ";
        
        if (pFileName->FileAttributes & NTFS_FILE_TYPE_READ_ONLY)
            legend << "Read-Only ";
        if (pFileName->FileAttributes & NTFS_FILE_TYPE_HIDDEN)
            legend << "Hidden ";
        if (pFileName->FileAttributes & NTFS_FILE_TYPE_SYSTEM)
            legend << "System ";
/*#define NTFS_FILE_TYPE_ARCHIVE    0x20
#define NTFS_FILE_TYPE_REPARSE    0x400
#define NTFS_FILE_TYPE_COMPRESSED 0x800*/
        if (pFileName->FileAttributes & NTFS_FILE_TYPE_DIRECTORY)
            legend << "Directory";

        pData->AddChild(56, 4, "Flags", "FlLegend", legend.str());
        pData->AddChild(60, 4, "Reparse", "RLegend", "Reparse Value");
        pData->AddChild(64, 1, "NameLength", "NLLegend", "Name Length");

        legend.str("");
        legend.clear();
        legend << "Name Space ";
        switch (pFileName->NameType)
        {
        case 0:
            legend << "(POSIX)";
            break;
        case 1:
            legend << "(Win32)";
            break;
        case 2:
            legend << "(DOS)";
            break;
        case 3:
            legend << "(Win32 & DOS)";
            break;
        }

        pData->AddChild(65, 1, "NameSpace", "NSLegend", legend.str() );

        if (pFileName->NameLength != 0)
        {
            WCHAR Name[260];
            memset(Name, 0, 260 * 2);

            memcpy(Name, (PCHAR)pFileName + 66, pFileName->NameLength * 2);
            char ch[260];
            char DefChar = ' ';
            WideCharToMultiByte(CP_ACP, 0, Name, -1, ch, 260, &DefChar, NULL);

            legend.str("");
            legend.clear();
            legend << "Name - " << ch;
            pData->AddChild(66, pFileName->NameLength * 2, "Name", "NLegend", legend.str());

        }

        break;
    case AttributeAttributeList:
        pRange->AddChild(0, record->Length, "AttrAttrList", "AALLegend", "$ATTRIBUTE_LIST Attribute", true);
        pRange = pRange->pChildren.back();
        fillupAttributeHeader(pRange, record);
        break;
    case AttributeObjectId:
        pRange->AddChild(0, record->Length, "AttrObjId", "AiLegend", "$OBJECT_ID Attribute", true);
        pRange = pRange->pChildren.back();
        fillupAttributeHeader(pRange, record);
        break;
    case AttributeData:
        pRange->AddChild(0, record->Length, "AttrData", "ADLegend", "$DATA Attribute", true);
        pRange = pRange->pChildren.back();
        fillupAttributeHeader(pRange, record);
        break;
    case AttributeIndexRoot:
        pRange->AddChild(0, record->Length, "AttrIndexRoot", "AIRLegend", "$INDEX_ROOT Attribute", true);
        pRange = pRange->pChildren.back();
        fillupAttributeHeader(pRange, record);


        pIndexRoot = pRange->AddChild(record->Resident.ValueOffset, record->Resident.ValueLength, "INDEX_ROOTData", "IRLegend", "$INDEX_ROOT DATA", true);
        pIndexRoot->AddChild(0, 4, "AttrType", "ATLegend", "Attribute Type");
        pIndexRoot->AddChild(4, 4, "CollationRule", "CRLegend", "Collation Rule");
        pIndexRoot->AddChild(8, 4, "BPIR", "BPIRLegend", "Bytes Per Index Record");
        pIndexRoot->AddChild(12, 4, "CPIR", "CPIRLegend", "Clusters Per Index Record");


        break;
    case AttributeIndexAllocation:
        pRange->AddChild(0, record->Length, "AttrIndexAllocation", "AIALegend", "$INDEX_ALLOCATION Attribute", true);
        pRange = pRange->pChildren.back();
        fillupAttributeHeader(pRange, record);
        break;
    case AttributeBitmap:
        pRange->AddChild(0, record->Length, "AttrBitmap", "ABLegend", "$BITMAP Attribute", true);
        pRange = pRange->pChildren.back();
        fillupAttributeHeader(pRange, record);
        break;
    case AttributeLoggedUtilityStream:
        pRange->AddChild(0, record->Length, "AttrLoggedUtility", "ALULegend", "$LOGGED_UTILITY_STREAM Attribute", true);
        pRange = pRange->pChildren.back();
        fillupAttributeHeader(pRange, record);
        break;
    default:
        fillupAttributeHeader(pRange, record);
        break;
    }

    
    //pHeader->AddChild(0x0e, 2, "id" "idLegend", "Attribute ID");

    
}

void DumpAttrib(PNTFS_ATTR_RECORD currentRecord, infoRange *pCurrentRange, std::ostream *pHtmlFile, std::ostream *pCSS, int CurrentRecordOffset)
{

    fillupInfoRange(pCurrentRange, currentRecord);

    *pHtmlFile << "\n<br>";
    *pCSS << "\n";


    

    int j = 0;

    int bytesOffFromAligned = CurrentRecordOffset % 16;

    if (bytesOffFromAligned > 0)
    {
        int i = 0;
        pCurrentRange->PrintStart();
        for (; i < (16 - bytesOffFromAligned); i++)
        {
            *pHtmlFile << "&nbsp;&nbsp;&nbsp;";
        }
        i = 16 - bytesOffFromAligned;

        for (int k = 0; i < 16; i++, k++, j++)
        {
            pCurrentRange->PrintStart();
            *pHtmlFile << std::setw(2) << std::hex << std::setfill('0') << (int)((PBYTE)currentRecord)[k] << "&nbsp;";
            pCurrentRange->advance(1);
            pCurrentRange->PrintEnd();
        }
        *pHtmlFile << "\n<br>";

        //pCurrentRange->PrintStart();
        //pCurrentRange->PrintEnd();


        //j += k;
    }


    for (; j < currentRecord->Length; j += 16)
    {
        for (int i = 0; i < 16 && (i + j < currentRecord->Length); i++)
        {
            pCurrentRange->PrintStart();
            *pHtmlFile << std::setw(2) << std::hex << std::setfill('0') << (int)((PBYTE)currentRecord)[i + j] << "&nbsp;";
            pCurrentRange->advance(1);
            pCurrentRange->PrintEnd();
            //pCurrentRange->PrintStart();
        }

        *pHtmlFile << "\n<br>";
    }

    pCurrentRange->PrintCSS();

    // for (int l = 0; l < j; l++)
    //    *pHtmlFile << "&nbsp;&nbsp;&nbsp;";

    /* for (int l = j, currentByte = 0; l < 16; l++, currentByte++)
    {
    pCurrentRange->PrintStart();
    *pHtmlFile << std::setw(2) << std::hex << std::setfill('0') << (int)((PBYTE)currentRecord)[currentByte] << "&nbsp;";
    pCurrentRange->advance(1);
    }*/


    /*i = j;
    for (int j = entry.FirstAttributeOffset; currentRecord->Type != 0xffffffff && j < 1024 && currentRecord->Length != 0; j += currentRecord->Length)
    {
    for (int k = 0; k < currentRecord->Length; k += 16)
    {
    for (i = 0; i < 16 && i + k < currentRecord->Length; i++)
    {
    pCurrentRange->PrintStart();
    *pHtmlFile << std::setw(2) << std::hex << std::setfill('0') << (int)((PBYTE)currentRecord)[i + k + currentByte] << "&nbsp;";

    pCurrentRange->advance(1);
    pCurrentRange->PrintStart();
    pCurrentRange->PrintEnd();
    }

    i = 0;

    *pHtmlFile << "\n<br>";
    }

    currentRecord = (PNTFS_ATTR_RECORD)((BYTE*)currentRecord + currentRecord->Length);

    std::stringstream classname;
    classname << "Attribute" << ++attributeNumber;

    std::stringstream legendid;
    legendid << "ATT" << attributeNumber << "Legend";

    std::stringstream legend;
    legend << "Attribute " << attributeNumber;

    pCurrentRange->PrintCSS();

    pCurrentRange = new infoRange(0, currentRecord->Length, pHtmlFile, pCSS, classname.str(), legendid.str(), legend.str());
    */
}

VOID
NtfsDumpIndexRootAttribute(PNTFS_ATTR_RECORD Attribute)
{
    PINDEX_ROOT_ATTRIBUTE IndexRootAttr;

    IndexRootAttr = (PINDEX_ROOT_ATTRIBUTE)((ULONG_PTR)Attribute + Attribute->Resident.ValueOffset);

    if (IndexRootAttr->AttributeType == AttributeFileName)
    {
        if (IndexRootAttr->CollationRule != COLLATION_FILE_NAME)
        {
            cout << "Collation rule doesn't match expected!\n";
        }
    }

    cout << "  $INDEX_ROOT \nIndexRootAttr->SizeOfEntry: " << (int)IndexRootAttr->SizeOfEntry 
        << ", IndexRootAttr->ClustersPerIndexRecord: " << (int)IndexRootAttr->ClustersPerIndexRecord << "\n";

    if (IndexRootAttr->Header.Flags == INDEX_ROOT_SMALL)
    {
        DbgPrint(" (small) ");
    }
    else
    {
        if (IndexRootAttr->Header.Flags != INDEX_ROOT_LARGE)
        {
            cout << "IndexRootAttr->Header.Flags doesn't match expected \n";
        }

        DbgPrint(" (large) ");
    }
}

VOID
NtfsDumpStandardInformationAttribute(PNTFS_ATTR_RECORD Attribute)
{
    PSTANDARD_INFORMATION StandardInfoAttr;

    DbgPrint("  $STANDARD_INFORMATION ");

    //    DbgPrint(" Length %lu  Offset %hu ", Attribute->Resident.ValueLength, Attribute->Resident.ValueOffset);

    StandardInfoAttr = (PSTANDARD_INFORMATION)((ULONG_PTR)Attribute + Attribute->Resident.ValueOffset);
    //cout << StandardInfoAttr->FileAttribute << "\n";

    cout << "File Permissions: ";

    if (StandardInfoAttr->FileAttribute & NTFS_FILE_TYPE_READ_ONLY)
        cout << "Read-Only ";
    if (StandardInfoAttr->FileAttribute & NTFS_FILE_TYPE_HIDDEN)
        cout << "Hidden ";
    if (StandardInfoAttr->FileAttribute & NTFS_FILE_TYPE_SYSTEM)
        cout << "System ";
    if (StandardInfoAttr->FileAttribute & NTFS_FILE_TYPE_ARCHIVE)
        cout << "Archive ";
    if (StandardInfoAttr->FileAttribute & NTFS_FILE_TYPE_DIRECTORY)
        cout << "Directory ";

    cout << "\n";

    //cout << (int)FileNameAttr->FileAttributes << "\n";
}

VOID
NtfsDumpFileNameAttribute(PNTFS_ATTR_RECORD Attribute)
{
    PFILENAME_ATTRIBUTE FileNameAttr;

    DbgPrint("  $FILE_NAME ");

    //    DbgPrint(" Length %lu  Offset %hu ", Attribute->Resident.ValueLength, Attribute->Resident.ValueOffset);

    FileNameAttr = (PFILENAME_ATTRIBUTE)((ULONG_PTR)Attribute + Attribute->Resident.ValueOffset);
    //cout << (int)FileNameAttr->NameType << ", " << (int)FileNameAttr->NameLength << "\n";
    wcout << L"Filename: \"" << FileNameAttr->Name << L"\"\n";
    
    cout << "Flags: ";

    if (FileNameAttr->FileAttributes & NTFS_FILE_TYPE_READ_ONLY)
        cout << "Read-Only ";
    if (FileNameAttr->FileAttributes & NTFS_FILE_TYPE_HIDDEN)
            cout << "Hidden ";
    if (FileNameAttr->FileAttributes & NTFS_FILE_TYPE_SYSTEM)
        cout << "System ";
    if (FileNameAttr->FileAttributes & NTFS_FILE_TYPE_ARCHIVE)
        cout << "Archive ";
    if (FileNameAttr->FileAttributes & NTFS_FILE_TYPE_DIRECTORY)
        cout << "Directory ";
    
    cout << "\n";
    //cout << (int)FileNameAttr->FileAttributes << "\n";
    

    cout << "FileNameAttr->AllocatedSize: " << FileNameAttr->AllocatedSize << "\n" 
         << "FileNameAttr->DataSize: " << FileNameAttr->DataSize << "\n";
}

BOOLEAN
FindRun(PNTFS_ATTR_RECORD NresAttr,
    ULONGLONG vcn,
    PULONGLONG lcn,
    PULONGLONG count)
{
    if (vcn < NresAttr->NonResident.LowestVCN || vcn > NresAttr->NonResident.HighestVCN)
        return FALSE;

    DecodeRun((PUCHAR)((ULONG_PTR)NresAttr + NresAttr->NonResident.MappingPairsOffset), (PLONGLONG)lcn, count);

    return TRUE;
}

NTSTATUS
NtfsReadDisk(HANDLE hDrive,
    IN LONGLONG StartingOffset,
    IN ULONG Length,
    IN ULONG SectorSize,
    IN OUT PUCHAR Buffer,
    IN BOOLEAN Override)
{
    LONGLONG RealOffset = StartingOffset - (StartingOffset % SectorSize);
    ULONG RealLength = Length;
    LARGE_INTEGER Offset;
    BOOLEAN AllocatedBuffer = FALSE;
    PUCHAR ReadBuffer = Buffer;

    if ((RealOffset % SectorSize) != 0 || (RealLength % SectorSize) != 0)
    {
        RealOffset = ROUND_DOWN(StartingOffset, SectorSize);
        RealLength = ROUND_UP(Length, SectorSize);

        ReadBuffer = (PUCHAR)malloc(RealLength + SectorSize); //ExAllocatePoolWithTag(NonPagedPool, RealLength + SectorSize, TAG_NTFS);
        if (ReadBuffer == NULL)
        {
            cout << "Not enough memory!\n";
            return -1;
        }
        AllocatedBuffer = TRUE;
    }

    Offset.QuadPart = RealOffset;

    if (!SetFilePointerEx(hDrive, Offset, NULL, FILE_BEGIN))
    {
        cout << "Unable to set file pointer to " << std::hex << Offset.QuadPart << ".\nLast error: " << GetLastError() << "\n";
        return -1;
    }

    DWORD Read;
    if (!ReadFile(hDrive, ReadBuffer, Length, &Read, NULL))
    {
        cout << "Unable to perform read of Length " << Length << " from offset " << Offset.QuadPart << "\n";
        return -1;
    }

    if (AllocatedBuffer)
    {
        memcpy(Buffer, ReadBuffer + (StartingOffset - RealOffset), Length);
        free(ReadBuffer);
    }

    return TRUE;
}

NTSTATUS
FindFirstAttribute(PFIND_ATTR_CONTXT Context,
    PDEVICE_EXTENSION Vcb,
    MFTENTRY *FileRecord,
    BOOLEAN OnlyResident,
    PNTFS_ATTR_RECORD * Attribute)
{
    NTSTATUS Status;

    //DPRINT("FindFistAttribute(%p, %p, %p, %p, %u, %p)\n", Context, Vcb, FileRecord, OnlyResident, Attribute);

    Context->Vcb = Vcb;
    Context->OnlyResident = OnlyResident;
    Context->FirstAttr = (PNTFS_ATTR_RECORD)((ULONG_PTR)FileRecord + FileRecord->FirstAttributeOffset);
    Context->CurrAttr = Context->FirstAttr;
    Context->LastAttr = (PNTFS_ATTR_RECORD)((ULONG_PTR)FileRecord + FileRecord->MftEntryUsedSize);
    Context->NonResidentStart = NULL;
    Context->NonResidentEnd = NULL;
    Context->Offset = FileRecord->FirstAttributeOffset;

    if (Context->FirstAttr->Type == AttributeEnd)
    {
        Context->CurrAttr = (PNTFS_ATTR_RECORD)-1;
        return STATUS_END_OF_FILE;
    }
    else if (Context->FirstAttr->Type == AttributeAttributeList)
    {
        Status = InternalReadNonResidentAttributes(Context);
        if (!NT_SUCCESS(Status))
        {
            return Status;
        }

        *Attribute = InternalGetNextAttribute(Context);
        if (*Attribute == NULL)
        {
            return STATUS_END_OF_FILE;
        }
    }
    else
    {
        *Attribute = Context->CurrAttr;
        Context->Offset = (UCHAR*)Context->CurrAttr - (UCHAR*)FileRecord;
    }

    return STATUS_SUCCESS;
}

ULONG
ReadAttribute(PDEVICE_EXTENSION Vcb,
    PNTFS_ATTR_CONTEXT Context,
    ULONGLONG Offset,
    PCHAR Buffer,
    ULONG Length)
{
    ULONGLONG LastLCN;
    PUCHAR DataRun;
    LONGLONG DataRunOffset;
    ULONGLONG DataRunLength;
    LONGLONG DataRunStartLCN;
    ULONGLONG CurrentOffset;
    ULONG ReadLength;
    ULONG AlreadyRead;
    NTSTATUS Status;

    if (!Context->Record.IsNonResident)
    {
        if (Offset > Context->Record.Resident.ValueLength)
            return 0;
        if (Offset + Length > Context->Record.Resident.ValueLength)
            Length = (ULONG)(Context->Record.Resident.ValueLength - Offset);
        RtlCopyMemory(Buffer, (PCHAR)&Context->Record + Context->Record.Resident.ValueOffset + Offset, Length);
        return Length;
    }

    /*
    * Non-resident attribute
    */

    /*
    * I. Find the corresponding start data run.
    */

    AlreadyRead = 0;

    // FIXME: Cache seems to be non-working. Disable it for now
    //if(Context->CacheRunOffset <= Offset && Offset < Context->CacheRunOffset + Context->CacheRunLength * Volume->ClusterSize)
    if (0)
    {
        DataRun = Context->CacheRun;
        LastLCN = Context->CacheRunLastLCN;
        DataRunStartLCN = Context->CacheRunStartLCN;
        DataRunLength = Context->CacheRunLength;
        CurrentOffset = Context->CacheRunCurrentOffset;
    }
    else
    {
        LastLCN = 0;
        DataRun = (PUCHAR)&Context->Record + Context->Record.NonResident.MappingPairsOffset;
        CurrentOffset = 0;

        while (1)
        {
            DataRun = DecodeRun(DataRun, &DataRunOffset, &DataRunLength);
            if (DataRunOffset != -1)
            {
                /* Normal data run. */
                DataRunStartLCN = LastLCN + DataRunOffset;
                LastLCN = DataRunStartLCN;
            }
            else
            {
                /* Sparse data run. */
                DataRunStartLCN = -1;
            }

            if (Offset >= CurrentOffset &&
                Offset < CurrentOffset + (DataRunLength * (Vcb->Vpb->BytesPerSector * Vcb->Vpb->SectorsPerCluster)))
            {
                break;
            }

            if (*DataRun == 0)
            {
                return AlreadyRead;
            }

            CurrentOffset += DataRunLength * (Vcb->Vpb->BytesPerSector * Vcb->Vpb->SectorsPerCluster);
        }
    }

    /*
    * II. Go through the run list and read the data
    */

    ReadLength = (ULONG)min(DataRunLength * (Vcb->Vpb->BytesPerSector * Vcb->Vpb->SectorsPerCluster) - (Offset - CurrentOffset), Length);
    if (DataRunStartLCN == -1)
    {
        RtlZeroMemory(Buffer, ReadLength);
        Status = STATUS_SUCCESS;
    }
    else
    {
        Status = NtfsReadDisk(Vcb->hDrive,
            DataRunStartLCN * (Vcb->Vpb->BytesPerSector * Vcb->Vpb->SectorsPerCluster) + Offset - CurrentOffset,
            ReadLength,
            (Vcb->Vpb->BytesPerSector * Vcb->Vpb->SectorsPerCluster),
            (PUCHAR)Buffer,
            FALSE);
    }
    if (NT_SUCCESS(Status))
    {
        Length -= ReadLength;
        Buffer += ReadLength;
        AlreadyRead += ReadLength;

        if (ReadLength == DataRunLength * (Vcb->Vpb->BytesPerSector * Vcb->Vpb->SectorsPerCluster) - (Offset - CurrentOffset))
        {
            CurrentOffset += DataRunLength * (Vcb->Vpb->BytesPerSector * Vcb->Vpb->SectorsPerCluster);
            DataRun = DecodeRun(DataRun, &DataRunOffset, &DataRunLength);
            if (DataRunOffset != (ULONGLONG)-1)
            {
                DataRunStartLCN = LastLCN + DataRunOffset;
                LastLCN = DataRunStartLCN;
            }
            else
                DataRunStartLCN = -1;

            if (*DataRun == 0)
                return AlreadyRead;
        }

        while (Length > 0)
        {
            ReadLength = (ULONG)min(DataRunLength * (Vcb->Vpb->BytesPerSector * Vcb->Vpb->SectorsPerCluster), Length);
            if (DataRunStartLCN == -1)
                RtlZeroMemory(Buffer, ReadLength);
            else
            {
                Status = NtfsReadDisk(Vcb->hDrive,
                    DataRunStartLCN * (Vcb->Vpb->BytesPerSector * Vcb->Vpb->SectorsPerCluster),
                    ReadLength,
                    (Vcb->Vpb->BytesPerSector * Vcb->Vpb->SectorsPerCluster),
                    (PUCHAR)Buffer,
                    FALSE);
                if (!NT_SUCCESS(Status))
                    break;
            }

            Length -= ReadLength;
            Buffer += ReadLength;
            AlreadyRead += ReadLength;

            /* We finished this request, but there still data in this data run. */
            if (Length == 0 && ReadLength != DataRunLength * (Vcb->Vpb->BytesPerSector * Vcb->Vpb->SectorsPerCluster))
                break;

            /*
            * Go to next run in the list.
            */

            if (*DataRun == 0)
                break;
            CurrentOffset += DataRunLength * (Vcb->Vpb->BytesPerSector * Vcb->Vpb->SectorsPerCluster);
            DataRun = DecodeRun(DataRun, &DataRunOffset, &DataRunLength);
            if (DataRunOffset != -1)
            {
                /* Normal data run. */
                DataRunStartLCN = LastLCN + DataRunOffset;
                LastLCN = DataRunStartLCN;
            }
            else
            {
                /* Sparse data run. */
                DataRunStartLCN = -1;
            }
        } /* while */

    } /* if Disk */

    Context->CacheRun = DataRun;
    Context->CacheRunOffset = Offset + AlreadyRead;
    Context->CacheRunStartLCN = DataRunStartLCN;
    Context->CacheRunLength = DataRunLength;
    Context->CacheRunLastLCN = LastLCN;
    Context->CacheRunCurrentOffset = CurrentOffset;

    return AlreadyRead;
}

ULONG
ReadAttribute(HANDLE hDrive,
    PBOOTFILE pBoot,
    NTFS_ATTR_RECORD Record,
    ULONGLONG Offset,
    PCHAR Buffer,
    ULONG Length)
{
    ULONGLONG LastLCN;
    PUCHAR DataRun;
    LONGLONG DataRunOffset;
    ULONGLONG DataRunLength;
    LONGLONG DataRunStartLCN;
    ULONGLONG CurrentOffset;
    ULONG ReadLength;
    ULONG AlreadyRead;
    NTSTATUS Status;
    ULONG BytesPerCluster = pBoot->SectorsPerCluster * pBoot->BytesPerSector;

    if (Record.IsNonResident)
    {
        if (Offset > Record.Resident.ValueLength)
            return 0;
        if (Offset + Length > Record.Resident.ValueLength)
            Length = (ULONG)(Record.Resident.ValueLength - Offset);
        RtlCopyMemory(Buffer, (PCHAR)&Record + Record.Resident.ValueOffset + Offset, Length);
        return Length;
    }

    /*
    * Non-resident attribute
    */

    /*
    * I. Find the corresponding start data run.
    */

    AlreadyRead = 0;

    // FIXME: Cache seems to be non-working. Disable it for now
    //if(Context->CacheRunOffset <= Offset && Offset < Context->CacheRunOffset + Context->CacheRunLength * Volume->ClusterSize)

    {
        LastLCN = 0;
        DataRun = (PUCHAR)&Record + Record.NonResident.MappingPairsOffset;
        CurrentOffset = 0;

        bool done = false;

        while (!done)
        {
            DataRun = DecodeRun(DataRun, &DataRunOffset, &DataRunLength);
            if (DataRunOffset != -1)
            {
                /* Normal data run. */
                DataRunStartLCN = LastLCN + DataRunOffset;
                LastLCN = DataRunStartLCN;
            }
            else
            {
                /* Sparse data run. */
                DataRunStartLCN = -1;
            }

            if (Offset >= CurrentOffset && Offset < CurrentOffset + (DataRunLength * (BytesPerCluster)))
            {
                break;
            }

            if (*DataRun == 0)
            {
                return AlreadyRead;
            }

            CurrentOffset += DataRunLength * BytesPerCluster;
        }
    }



    ReadLength = (ULONG)min(DataRunLength * BytesPerCluster - (Offset - CurrentOffset), Length);
    if (DataRunStartLCN == -1)
    {
        RtlZeroMemory(Buffer, ReadLength);
        Status = STATUS_SUCCESS;
    }
    else
    {
        Status = NtfsReadDisk(hDrive,
            DataRunStartLCN * BytesPerCluster + Offset - CurrentOffset,
            ReadLength,
            pBoot->BytesPerSector,
            (PUCHAR)Buffer,
            FALSE);
    }
    if (NT_SUCCESS(Status))
    {
        Length -= ReadLength;
        Buffer += ReadLength;
        AlreadyRead += ReadLength;

        if (ReadLength == DataRunLength * BytesPerCluster - (Offset - CurrentOffset))
        {
            CurrentOffset += DataRunLength * BytesPerCluster;
            DataRun = DecodeRun(DataRun, &DataRunOffset, &DataRunLength);
            if (DataRunOffset != (ULONGLONG)-1)
            {
                DataRunStartLCN = LastLCN + DataRunOffset;
                LastLCN = DataRunStartLCN;
            }
            else
                DataRunStartLCN = -1;

            if (*DataRun == 0)
                return AlreadyRead;
        }

        while (Length > 0)
        {
            ReadLength = (ULONG)min(DataRunLength * BytesPerCluster, Length);
            if (DataRunStartLCN == -1)
                RtlZeroMemory(Buffer, ReadLength);
            else
            {
                Status = NtfsReadDisk(hDrive,
                    DataRunStartLCN * BytesPerCluster,
                    ReadLength,
                    pBoot->BytesPerSector,
                    (PUCHAR)Buffer,
                    FALSE);
                if (!NT_SUCCESS(Status))
                    break;
            }

            Length -= ReadLength;
            Buffer += ReadLength;
            AlreadyRead += ReadLength;

            /* We finished this request, but there still data in this data run. */
            if (Length == 0 && ReadLength != DataRunLength * BytesPerCluster)
                break;

            /*
            * Go to next run in the list.
            */

            if (*DataRun == 0)
                break;
            CurrentOffset += DataRunLength * BytesPerCluster;
            DataRun = DecodeRun(DataRun, &DataRunOffset, &DataRunLength);
            if (DataRunOffset != -1)
            {
                /* Normal data run. */
                DataRunStartLCN = LastLCN + DataRunOffset;
                LastLCN = DataRunStartLCN;
            }
            else
            {
                /* Sparse data run. */
                DataRunStartLCN = -1;
            }
        } /* while */

    } /* if Disk */


    return AlreadyRead;
}

NTSTATUS
FindNextAttribute(PFIND_ATTR_CONTXT Context,
    PNTFS_ATTR_RECORD * Attribute)
{
    NTSTATUS Status;

    //DPRINT("FindNextAttribute(%p, %p)\n", Context, Attribute);

    *Attribute = InternalGetNextAttribute(Context);
    if (*Attribute == NULL)
    {
        return STATUS_END_OF_FILE;
    }

    if (Context->CurrAttr->Type != AttributeAttributeList)
    {
        return STATUS_SUCCESS;
    }

    Status = InternalReadNonResidentAttributes(Context);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    *Attribute = InternalGetNextAttribute(Context);
    if (*Attribute == NULL)
    {
        return STATUS_END_OF_FILE;
    }

    return STATUS_SUCCESS;
}

VOID
FindCloseAttribute(PFIND_ATTR_CONTXT Context)
{
    if (Context->NonResidentStart != NULL)
    {
        //ExFreePoolWithTag(Context->NonResidentStart, TAG_NTFS);
        free(Context->NonResidentStart);
        Context->NonResidentStart = NULL;
    }
}


PFILENAME_ATTRIBUTE
GetFileNameFromRecord(PDEVICE_EXTENSION Vcb,
                      PFILE_RECORD_HEADER FileRecord,
                      UCHAR NameType)
{
    FIND_ATTR_CONTXT Context;
    PNTFS_ATTR_RECORD Attribute;
    PFILENAME_ATTRIBUTE Name;
    NTSTATUS Status;

    Status = FindFirstAttribute(&Context, Vcb, FileRecord, FALSE, &Attribute);
    while (NT_SUCCESS(Status))
    {
        if (Attribute->Type == AttributeFileName)
        {
            Name = (PFILENAME_ATTRIBUTE)((ULONG_PTR)Attribute + Attribute->Resident.ValueOffset);
            if (Name->NameType == NameType ||
                (Name->NameType == NTFS_FILE_NAME_WIN32_AND_DOS && NameType == NTFS_FILE_NAME_WIN32) ||
                (Name->NameType == NTFS_FILE_NAME_WIN32_AND_DOS && NameType == NTFS_FILE_NAME_DOS))
            {
                FindCloseAttribute(&Context);
                return Name;
            }
        }

        Status = FindNextAttribute(&Context, &Attribute);
    }

    FindCloseAttribute(&Context);
    return NULL;
}

PFILENAME_ATTRIBUTE
GetBestFileNameFromRecord(PDEVICE_EXTENSION Vcb,
                          PFILE_RECORD_HEADER FileRecord)
{
    PFILENAME_ATTRIBUTE FileName;

    FileName = GetFileNameFromRecord(Vcb, FileRecord, NTFS_FILE_NAME_POSIX);
    if (FileName == NULL)
    {
        FileName = GetFileNameFromRecord(Vcb, FileRecord, NTFS_FILE_NAME_WIN32);
        if (FileName == NULL)
        {
            FileName = GetFileNameFromRecord(Vcb, FileRecord, NTFS_FILE_NAME_DOS);
        }
    }

    return FileName;
}