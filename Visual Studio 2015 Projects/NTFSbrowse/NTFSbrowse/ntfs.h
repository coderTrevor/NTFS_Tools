#pragma once

#define MOUSEOVER false

#include <string>
#include "stdafx.h"
#include <iostream>
#include <iomanip>
#include <Windows.h>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <vector>
using namespace std;


using std::cout;
using std::wcout;


#define STATUS_SUCCESS                  0
#define STATUS_UNSUCCESSFUL             -1
#define STATUS_OBJECT_NAME_NOT_FOUND    -2
#define STATUS_END_OF_FILE              -3
#define STATUS_FILE_CORRUPT_ERROR       -4
#define STATUS_INSUFFICIENT_RESOURCES   -5
#define STATUS_BUFFER_OVERFLOW          -6


#define NTFS_FILE_TYPE_READ_ONLY  0x1
#define NTFS_FILE_TYPE_HIDDEN     0x2
#define NTFS_FILE_TYPE_SYSTEM     0x4
#define NTFS_FILE_TYPE_ARCHIVE    0x20
#define NTFS_FILE_TYPE_REPARSE    0x400
#define NTFS_FILE_TYPE_COMPRESSED 0x800
#define NTFS_FILE_TYPE_DIRECTORY  0x10000000

/* Indexed Flag in Resident attributes - still somewhat speculative */
#define RA_INDEXED    0x01

typedef struct
{
    ULONG Type;             /* Magic number 'FILE' */
    USHORT UsaOffset;       /* Offset to the update sequence */
    USHORT UsaCount;        /* Size in words of Update Sequence Number & Array (S) */
    ULONGLONG Lsn;          /* $LogFile Sequence Number (LSN) */
} NTFS_RECORD_HEADER, *PNTFS_RECORD_HEADER;

/* NTFS_RECORD_HEADER.Type */
#define NRH_FILE_TYPE  0x454C4946  /* 'FILE' */
#define NRH_INDX_TYPE  0x58444E49  /* 'INDX' */


static BOOLEAN NT_SUCCESS(int a)
{
    if (a)
        return FALSE;

    return TRUE;
}


#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))
#define ROUND_DOWN(N, S) ((N) - ((N) % (S)))


#define TAG_NTFS 'SFTN'
#define NTFS_MFT_MASK 0x0000FFFFFFFFFFFFULL
//#define NTFS_MFT_MASK 0xFFFFFFFFFFFF0000ULL

#pragma pack(1)
// These values are from File System Forensic Analysis
typedef struct
{
    BYTE jmpcode[3];
    BYTE OEM_NAME[8];
    WORD BytesPerSector;
    BYTE SectorsPerCluster;
    WORD Reserved1;     // must be 0
    BYTE Unused1[5];      // must be 0
    BYTE MediaDesc;     // will be f8 for a hard drive
    WORD Unused2;             // must be 0
    LARGE_INTEGER Unused3;    // not checked
    DWORD Unused4;            // must be 0
    DWORD Unused5;            // not checked
    LARGE_INTEGER SectorsInFileSystem;
    LARGE_INTEGER MftFirstCluster;
    LARGE_INTEGER MftMirrorFirstCluster;
    //WORD Unused9;
    BYTE SizeOfMftEntry;      // seems to always be 1024 in practice (uses crazy encoding scheme)
    WORD Unused6;
    int SizeOfIndexRecord;
    //BYTE Unused7[3];
    LARGE_INTEGER SerialNumber;
    DWORD Unused8;
    BYTE BootCode[427];
    WORD Signature;           // 0xaa55
} BOOTFILE, *PBOOTFILE;

typedef struct
{
    DWORD   Signature;
    WORD    FixupArrayOffset;
    WORD    FixupEntryCount;
    LARGE_INTEGER LogFileSequenceNumber;
    WORD    SequenceValue;
    WORD    HardLinkCount;
    WORD    FirstAttributeOffset;
    WORD    Flags;
    DWORD   MftEntryUsedSize;
    DWORD   MftEntryAllocatedSize;
    LARGE_INTEGER baseRecordReference;
    WORD    NextAttributeId;
    BYTE    AttributesAndFixups[982];
} MFTENTRY, *PMFTENTRY, *PFILE_RECORD_HEADER;


// FILESIG = "FILE"
#define FILESIG 0x454c4946

typedef struct
{
    ULONG        Type;
    ULONG        Length;
    UCHAR        IsNonResident;
    UCHAR        NameLength;
    USHORT        NameOffset;
    USHORT        Flags;
    USHORT        Instance;
    union
    {
        // Resident attributes
        struct
        {
            ULONG        ValueLength;
            USHORT        ValueOffset;
            UCHAR        Flags;
            UCHAR        Reserved;
        } Resident;
        // Non-resident attributes
        struct
        {
            ULONGLONG        LowestVCN;
            ULONGLONG        HighestVCN;
            USHORT        MappingPairsOffset;
            USHORT        CompressionUnit;
            UCHAR        Reserved[4];
            LONGLONG        AllocatedSize;
            LONGLONG        DataSize;
            LONGLONG        InitializedSize;
            LONGLONG        CompressedSize;
        } NonResident;
    };
} NTFS_ATTR_RECORD, *PNTFS_ATTR_RECORD;

typedef struct
{
    ULONG Type;
    ULONG Size;
} NTFSIDENTIFIER, *PNTFSIDENTIFIER;

typedef struct
{
    NTFSIDENTIFIER Identifier;

    //ERESOURCE DirResource;
    //    ERESOURCE FatResource;

    //KSPIN_LOCK FcbListLock;
    // LIST_ENTRY FcbListHead;

    //PVPB Vpb;
    PBOOTFILE Vpb;
    //PDEVICE_OBJECT StorageDevice;
    HANDLE hDrive;
    //PFILE_OBJECT StreamFileObject;

    //struct _NTFS_ATTR_CONTEXT* MFTContext;
    /*
    struct _FILE_RECORD_HEADER* MasterFileTable;
    struct _FCB *VolumeFcb;*/

    //NTFS_INFO NtfsInfo;

    //ULONG Flags;
    // ULONG OpenHandleCount;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION, NTFS_VCB, *PNTFS_VCB;




typedef struct _NTFS_ATTR_CONTEXT
{
    PUCHAR            CacheRun;
    ULONGLONG            CacheRunOffset;
    LONGLONG            CacheRunStartLCN;
    ULONGLONG            CacheRunLength;
    LONGLONG            CacheRunLastLCN;
    ULONGLONG            CacheRunCurrentOffset;
    PNTFS_ATTR_RECORD    pRecord;
    //ULONG               Offset;   // where in the Mft entry is this attribute's header stored
} NTFS_ATTR_CONTEXT, *PNTFS_ATTR_CONTEXT;

typedef struct _FIND_ATTR_CONTXT
{
    PDEVICE_EXTENSION Vcb;
    BOOLEAN OnlyResident;
    PNTFS_ATTR_RECORD FirstAttr;
    PNTFS_ATTR_RECORD CurrAttr;
    PNTFS_ATTR_RECORD LastAttr;
    PNTFS_ATTR_RECORD NonResidentStart;
    PNTFS_ATTR_RECORD NonResidentEnd;
    ULONG Offset;
} FIND_ATTR_CONTXT, *PFIND_ATTR_CONTXT;

typedef enum
{
    AttributeStandardInformation = 0x10,
    AttributeAttributeList = 0x20,
    AttributeFileName = 0x30,
    AttributeObjectId = 0x40,
    AttributeSecurityDescriptor = 0x50,
    AttributeVolumeName = 0x60,
    AttributeVolumeInformation = 0x70,
    AttributeData = 0x80,
    AttributeIndexRoot = 0x90,
    AttributeIndexAllocation = 0xA0,
    AttributeBitmap = 0xB0,
    AttributeReparsePoint = 0xC0,
    AttributeEAInformation = 0xD0,
    AttributeEA = 0xE0,
    AttributePropertySet = 0xF0,
    AttributeLoggedUtilityStream = 0x100,
    AttributeEnd = 0xFFFFFFFF
} ATTRIBUTE_TYPE, *PATTRIBUTE_TYPE;

#define NTFS_FILE_MFT                0
#define NTFS_FILE_MFTMIRR            1
#define NTFS_FILE_LOGFILE            2
#define NTFS_FILE_VOLUME            3
#define NTFS_FILE_ATTRDEF            4
#define NTFS_FILE_ROOT                5
#define NTFS_FILE_BITMAP            6
#define NTFS_FILE_BOOT                7
#define NTFS_FILE_BADCLUS            8
#define NTFS_FILE_QUOTA                9
#define NTFS_FILE_UPCASE            10
#define NTFS_FILE_EXTEND            11


PUCHAR
DecodeRun(PUCHAR DataRun,
    LONGLONG *DataRunOffset,
    ULONGLONG *DataRunLength);

NTSTATUS
NtfsReadDisk(HANDLE hDrive,
    IN LONGLONG StartingOffset,
    IN ULONG Length,
    IN ULONG SectorSize,
    IN OUT PUCHAR Buffer,
    IN BOOLEAN Override);

VOID
FindCloseAttribute(PFIND_ATTR_CONTXT Context);

NTSTATUS
FindNextAttribute(PFIND_ATTR_CONTXT Context,
    PNTFS_ATTR_RECORD * Attribute);

NTSTATUS
FindFirstAttribute(PFIND_ATTR_CONTXT Context,
    PDEVICE_EXTENSION Vcb,
    MFTENTRY *FileRecord,
    BOOLEAN OnlyResident,
    PNTFS_ATTR_RECORD * Attribute);



static PNTFS_ATTR_CONTEXT PrepareAttributeContext(PNTFS_ATTR_RECORD AttrRecord)
{
    PNTFS_ATTR_CONTEXT Context;

    Context = (PNTFS_ATTR_CONTEXT)malloc(sizeof(NTFS_ATTR_CONTEXT)); //FrLdrTempAlloc(FIELD_OFFSET(NTFS_ATTR_CONTEXT, Record) + AttrRecord->Length,        TAG_NTFS_CONTEXT);

    Context->pRecord = (PNTFS_ATTR_RECORD)malloc(AttrRecord->Length);

    memcpy(Context->pRecord, AttrRecord, AttrRecord->Length);
    //RtlCopyMemory(&Context->Record, AttrRecord, AttrRecord->Length);

    if (AttrRecord->IsNonResident)
    {
        LONGLONG DataRunOffset;
        ULONGLONG DataRunLength;

        Context->CacheRun = (PUCHAR)((ULONG_PTR)Context->pRecord + Context->pRecord->NonResident.MappingPairsOffset);
        Context->CacheRunOffset = 0;
        Context->CacheRun = DecodeRun(Context->CacheRun, &DataRunOffset, &DataRunLength);
        Context->CacheRunLength = DataRunLength;
        if (DataRunOffset != -1)
        {
            /* Normal run. */
            Context->CacheRunStartLCN =
                Context->CacheRunLastLCN = DataRunOffset;
        }
        else
        {
            /* Sparse run. */
            Context->CacheRunStartLCN = -1;
            Context->CacheRunLastLCN = 0;
        }
        Context->CacheRunCurrentOffset = 0;
    }

    return Context;
}


typedef enum _POOL_TYPE {
    NonPagedPool,
    NonPagedPoolExecute = NonPagedPool,
    PagedPool,
    NonPagedPoolMustSucceed = NonPagedPool + 2,
    DontUseThisType,
    NonPagedPoolCacheAligned = NonPagedPool + 4,
    PagedPoolCacheAligned,
    NonPagedPoolCacheAlignedMustS = NonPagedPool + 6,
    MaxPoolType,
    NonPagedPoolBase = 0,
    NonPagedPoolBaseMustSucceed = NonPagedPoolBase + 2,
    NonPagedPoolBaseCacheAligned = NonPagedPoolBase + 4,
    NonPagedPoolBaseCacheAlignedMustS = NonPagedPoolBase + 6,
    NonPagedPoolSession = 32,
    PagedPoolSession = NonPagedPoolSession + 1,
    NonPagedPoolMustSucceedSession = PagedPoolSession + 1,
    DontUseThisTypeSession = NonPagedPoolMustSucceedSession + 1,
    NonPagedPoolCacheAlignedSession = DontUseThisTypeSession + 1,
    PagedPoolCacheAlignedSession = NonPagedPoolCacheAlignedSession + 1,
    NonPagedPoolCacheAlignedMustSSession = PagedPoolCacheAlignedSession + 1,
    NonPagedPoolNx = 512,
    NonPagedPoolNxCacheAligned = NonPagedPoolNx + 4,
    NonPagedPoolSessionNx = NonPagedPoolNx + 32
} POOL_TYPE;


static PVOID ExAllocatePoolWithTag(
    _In_ POOL_TYPE PoolType,
    _In_ SIZE_T    NumberOfBytes,
    _In_ ULONG     Tag
    )
{
    return malloc(NumberOfBytes);
}

static VOID ExFreePoolWithTag(
    _In_ PVOID P,
    _In_ ULONG Tag
    )
{
    free(P);
    P = (PVOID)0xdeadbeef;
}

ULONG
ReadAttribute(HANDLE hDrive,
    PBOOTFILE pBoot,
    PNTFS_ATTR_RECORD Record,
    ULONGLONG Offset,
    PCHAR Buffer,
    ULONG Length);

ULONG
ReadAttribute(PDEVICE_EXTENSION Vcb,
    PNTFS_ATTR_CONTEXT Context,
    ULONGLONG Offset,
    PCHAR Buffer,
    ULONG Length);

typedef struct UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

BOOLEAN
FindRun(PNTFS_ATTR_RECORD NresAttr,
    ULONGLONG vcn,
    PULONGLONG lcn,
    PULONGLONG count);

void DbgPrint(char *string);

typedef struct
{
    ULONGLONG DirectoryFileReferenceNumber;
    ULONGLONG CreationTime;
    ULONGLONG ChangeTime;
    ULONGLONG LastWriteTime;
    ULONGLONG LastAccessTime;
    ULONGLONG AllocatedSize;
    ULONGLONG DataSize;
    ULONG FileAttributes;
    union
    {
        struct
        {
            USHORT PackedEaSize;
            USHORT AlignmentOrReserved;
        } EaInfo;
        ULONG ReparseTag;
    } Extended;
    UCHAR NameLength;
    UCHAR NameType;
    WCHAR Name[1];
} FILENAME_ATTRIBUTE, *PFILENAME_ATTRIBUTE;

typedef struct
{
    ULONGLONG CreationTime;
    ULONGLONG ChangeTime;
    ULONGLONG LastWriteTime;
    ULONGLONG LastAccessTime;
    ULONG FileAttribute;
    ULONG AlignmentOrReserved[3];
} STANDARD_INFORMATION, *PSTANDARD_INFORMATION;


typedef struct
{
    union
    {
        struct
        {
            ULONGLONG    IndexedFile;
        } Directory;
        struct
        {
            USHORT    DataOffset;
            USHORT    DataLength;
            ULONG    Reserved;
        } ViewIndex;
    } Data;
    USHORT            Length;
    USHORT            KeyLength;
    USHORT            Flags;
    USHORT            Reserved;
    FILENAME_ATTRIBUTE    FileName;
} INDEX_ENTRY_ATTRIBUTE, *PINDEX_ENTRY_ATTRIBUTE;

typedef struct
{
    ULONGLONG Unknown1;
    UCHAR MajorVersion;
    UCHAR MinorVersion;
    USHORT Flags;
    ULONG Unknown2;
} VOLINFO_ATTRIBUTE, *PVOLINFO_ATTRIBUTE;

typedef struct
{
    ULONG FirstEntryOffset;
    ULONG TotalSizeOfEntries;
    ULONG AllocatedSize;
    UCHAR Flags;
    UCHAR Padding[3];
} INDEX_HEADER_ATTRIBUTE, *PINDEX_HEADER_ATTRIBUTE;

typedef struct
{
    ULONG AttributeType;
    ULONG CollationRule;
    ULONG SizeOfEntry;
    UCHAR ClustersPerIndexRecord;
    UCHAR Padding[3];
    INDEX_HEADER_ATTRIBUTE Header;
} INDEX_ROOT_ATTRIBUTE, *PINDEX_ROOT_ATTRIBUTE;

typedef struct
{
    NTFS_RECORD_HEADER Ntfs;
    ULONGLONG VCN;
    INDEX_HEADER_ATTRIBUTE Header;
} INDEX_BUFFER, *PINDEX_BUFFER;

#define COLLATION_BINARY              0x00
#define COLLATION_FILE_NAME           0x01
#define COLLATION_UNICODE_STRING      0x02
#define COLLATION_NTOFS_ULONG         0x10
#define COLLATION_NTOFS_SID           0x11
#define COLLATION_NTOFS_SECURITY_HASH 0x12
#define COLLATION_NTOFS_ULONGS        0x13



#define INDEX_ROOT_SMALL 0x0
#define INDEX_ROOT_LARGE 0x1

#define INDEX_NODE_SMALL 0x0
#define INDEX_NODE_LARGE 0x1

#define NTFS_INDEX_ENTRY_NODE            1
#define NTFS_INDEX_ENTRY_END            2

#define NTFS_FILE_NAME_POSIX            0
#define NTFS_FILE_NAME_WIN32            1
#define NTFS_FILE_NAME_DOS            2
#define NTFS_FILE_NAME_WIN32_AND_DOS        3


/* attrib.c:  */

PFILENAME_ATTRIBUTE
GetBestFileNameFromRecord(PDEVICE_EXTENSION Vcb,
                          PFILE_RECORD_HEADER FileRecord);

PFILENAME_ATTRIBUTE
GetFileNameFromRecord(PDEVICE_EXTENSION Vcb,
                      PFILE_RECORD_HEADER FileRecord,
                      UCHAR NameType);


VOID
NtfsDumpVolumeInformationAttribute(PNTFS_ATTR_RECORD Attribute);

VOID
NtfsDumpFileNameAttribute(PNTFS_ATTR_RECORD Attribute);

VOID
NtfsDumpStandardInformationAttribute(PNTFS_ATTR_RECORD Attribute);

VOID
NtfsDumpVolumeNameAttribute(PNTFS_ATTR_RECORD Attribute);

VOID
NtfsDumpIndexRootAttribute(PNTFS_ATTR_RECORD Attribute);

static VOID
ReleaseAttributeContext(PNTFS_ATTR_CONTEXT Context)
{
    ExFreePoolWithTag(Context, TAG_NTFS);
}

VOID
NtfsDumpAttribute(PDEVICE_EXTENSION Vcb,
    PNTFS_ATTR_RECORD Attribute);


class infoRange
{
public:
    ULONGLONG start;
    ULONGLONG end;
    ULONGLONG current;
    std::ostream *pOutput;
    std::ostream *pCSS;

    std::string startText;
    std::string endText;

    std::string className, LegendId, legend;

    std::vector<infoRange*> pChildren;

    bool startPrinted, endPrinted;

    static int instance;
    int myInstance;

    int depth;
    bool dynamicNames, legendAtTop;

    #define MAXDEPTH    5
    const std::string depthColors[MAXDEPTH] = { "#33FF66", "#9966FF", "#CC0000", "#0000BB", "#99FF66" };
    const std::string borderColors[MAXDEPTH] = { "green", "purple", "red", "blue", "yellow" };
    const std::string textColors[MAXDEPTH] = { "black", "white", "white", "white", "black" };

    /*infoRange(ULONGLONG start, ULONGLONG end, char *startText, char *endText, std::ostream *pOutput)
    {
    this->start = start;
    this->end = end;
    this->startText = startText;
    this->endText = endText;
    this->pOutput = pOutput;
    }*/

/*    infoRange *AddChild(ULONGLONG start, ULONG size, 
                        std::string LegendText, bool legendAtTop = false, bool dynamic = false)
    {
        std::stringstream ClassName, LegendID;
        
        ClassName.str(LegendText.c_str());//  LegendText.replace(" ", )
        ClassName.str().erase(remove_if(ClassName.str().begin(), ClassName.str().end(), ' '), ClassName.str().end());

            //std::string LegendID;
        LegendID.str(ClassName.str().c_str());// << "Legend";
        LegendID << "Legend";

        infoRange *pChild = new infoRange(start, size, pOutput, pCSS, ClassName.str(), LegendID.str(), LegendText, TRUE, dynamic, legendAtTop);
        pChild->depth = depth + 1;

        pChildren.push_back(pChild);

        return pChild;
    }*/

    infoRange *AddChild(ULONGLONG start, ULONG size, std::string ClassName, std::string LegendID,
                        std::string LegendText, bool legendAtTop = false, bool dynamic = false)
    {
        infoRange *pChild = new infoRange(start, size, pOutput, pCSS, ClassName, LegendID, LegendText, TRUE, dynamic, legendAtTop);
        pChild->depth = depth + 1;

        pChildren.push_back(pChild);

        return pChild;
    }

    infoRange(ULONGLONG start, ULONG size, char *startText, char *endText, std::ostream *pOutput, bool legendAtTop = false)
    {
        this->start = start;
        end = start + size;
        this->startText = startText;
        this->endText = endText;
        this->pOutput = pOutput;
        current = 0;
        startPrinted = false;
        endPrinted = false;
        depth = 0;
        dynamicNames = false;
        this->legendAtTop = legendAtTop;
    }

    infoRange(ULONGLONG start, ULONG size, std::ostream *pOutput, std::ostream *pCSS, std::string ClassName, std::string LegendID,
        std::string LegendText, BOOLEAN makeUnique = false, bool dynamic = false, bool legendAtTop = false)
    {
        std::stringstream startStream, endStream;

        if (makeUnique == TRUE)
        {
            myInstance = instance++;

            std::stringstream cnStream, lidStream;

            cnStream << ClassName << instance;
            ClassName = cnStream.str();

            lidStream << LegendID << instance;
            LegendID = lidStream.str();
        }

        /*if( makeUnique )
            startStream << "<div class=\"" << ClassName << instance << "\">";
        else*/

        startStream << "<div class=\"" << ClassName << "\">";

        if (legendAtTop)
            startStream << "<div id=\"" << LegendID << "\">&nbsp;" << LegendText << "</div>";

        /*if( makeUnique )
            endStream << "</div><div id=\"" << LegendID << instance++ << "\">&nbsp;" << LegendText << "</div>";
        else*/
        
        endStream << "</div>";
        
        if(!legendAtTop)
            endStream << "<div id=\"" << LegendID << "\">" << LegendText << "&nbsp;</div>";

        this->start = start;
        end = start + size;
        this->startText = (char*)(startStream.str().c_str());
        this->endText = (char*)endStream.str().c_str();
        this->legend = LegendText.c_str();
        this->pOutput = pOutput;
        this->pCSS = pCSS;
        this->legendAtTop = legendAtTop;
        current = 0;
        
        legendAtTop = false;
        dynamicNames = dynamic;

        className = ClassName;
        LegendId = LegendID;

        startPrinted = false;
        endPrinted = false;
        depth = 0;
    }

    void PrintStart()
    {
        if (!startPrinted)
        {
            if (current >= start)
            {
                if (current > start)
                    cout << "current > start\n";

                *pOutput << startText;
                startPrinted = true;
            }
        }

        if (current >= start)
        {
            for (size_t i = 0; i < pChildren.size(); i++)
            {
                pChildren[i]->PrintStart();
            }
        }
    }

    void advance(int bytes)
    {
        if (current >= start && current <= end)
        {
            for (size_t i = 0; i < pChildren.size(); i++)
            {
                pChildren[i]->PrintStart();
                //pChildren[i]->PrintEnd();
                pChildren[i]->advance(bytes);
                //pChildren[i]->PrintStart();
                pChildren[i]->PrintEnd();
            }
        }

        current += bytes;
    }

    void PrintEnd()
    {
        if (current >= start)
        {
            for (size_t i = 0; i < pChildren.size(); i++)
            {
                pChildren[i]->PrintEnd();
            }
        }

        if (endPrinted)
            return;
        
        if (current >= end)
        {
            if (current > end)
                cout << "current > end\n";

            *pOutput << endText;
            endPrinted = true;
        }

        /*if (current <= end)
        {
            for (size_t i = 0; i < pChildren.size(); i++)
            {
                pChildren[i]->PrintEnd();
            }
        }*/
    }

    void PrintCSS()
    {
        int color = depth % MAXDEPTH;
        bool hover = MOUSEOVER;;

        //*pCSS << "div." << className << ":hover\n{\n\tcolor: white;\n\tbackground-color: purple;\n\tborder: 1px solid blue;\n}\n";
        *pCSS << "div." << className;
        if (hover)
            *pCSS << ":hover";

        *pCSS << "\n{\n\tpadding: 10px;\n\tcolor: " << textColors[color] << ";\n\tbackground-color: "
              << depthColors[color] << ";\n\tborder: 1px solid " << borderColors[color] << ";\n}\n";

        // legend normal
        *pCSS << "#" << LegendId << "\n{\tdisplay: none;\n\tfloat: none;\n\tpadding: 5px;\n\tmargin: auto;\n\t"
            << "color: " << textColors[color] << ";\n\tbackground-color: "
            << depthColors[color] << ";\n\tborder: none;\n}\n";// 1px solid " << borderColors[color] << "; \n    }\n";


        // legend hover:
        *pCSS << "div." << className;
        
        if( hover )
            *pCSS << ":hover";
        
        if (!legendAtTop)
            *pCSS << " +";
        
        *pCSS << " #" << LegendId << "\n{\n\t/*background-color: green;*/\n\tdisplay: ";
        
        if( !legendAtTop )
            *pCSS << "inline-";
        
        *pCSS << "block;\n}\n";

        for (size_t i = 0; i < pChildren.size(); i++)
        {
            pChildren[i]->PrintCSS();

            *pCSS << "\n";
        }
    }

};

void fillupInfoRange(infoRange *pRange, PNTFS_ATTR_RECORD record, HANDLE hDrive);

void DumpAttrib(PNTFS_ATTR_RECORD currentRecord, infoRange *pCurrentRange, std::ostream *pHtmlFile, std::ostream *pCSS, int CurrentRecordOffset, HANDLE hDrive);

void DumpAttributeData(PNTFS_ATTR_RECORD Attribute, stringstream &cssName, HANDLE hDrive);

VOID
NtfsDumpDataRuns(PVOID StartOfRun,
                 ULONGLONG CurrentLCN);

VOID
NtfsDumpFileAttributes(PDEVICE_EXTENSION Vcb,
    PFILE_RECORD_HEADER FileRecord);

static ULONGLONG
AttributeAllocatedLength(PNTFS_ATTR_RECORD AttrRecord)
{
    if (AttrRecord->IsNonResident)
        return AttrRecord->NonResident.AllocatedSize;
    else
        return AttrRecord->Resident.ValueLength;
}


static ULONGLONG
AttributeDataLength(PNTFS_ATTR_RECORD AttrRecord)
{
    if (AttrRecord->IsNonResident)
        return AttrRecord->NonResident.DataSize;
    else
        return AttrRecord->Resident.ValueLength;
}


static
NTSTATUS
InternalReadNonResidentAttributes(PFIND_ATTR_CONTXT Context)
{
    ULONGLONG ListSize;
    PNTFS_ATTR_RECORD Attribute;
    PNTFS_ATTR_CONTEXT ListContext;

    cout << "InternalReadNonResidentAttributes " << Context << "\n";

    Attribute = Context->CurrAttr;
    if (Attribute->Type != AttributeAttributeList)
    {
        cout << "\a\t\t\tWoh stop everything!\n";
        return -1;
    }

    if (Context->OnlyResident)
    {
        Context->NonResidentStart = NULL;
        Context->NonResidentEnd = NULL;
        return STATUS_SUCCESS;
    }

    if (Context->NonResidentStart != NULL)
    {
        return STATUS_FILE_CORRUPT_ERROR;
    }

    ListContext = PrepareAttributeContext(Attribute);

    ListSize = AttributeDataLength(ListContext->pRecord);
    if (ListSize > 0xFFFFFFFF)
    {
        ReleaseAttributeContext(ListContext);
        return STATUS_BUFFER_OVERFLOW;
    }

    Context->NonResidentStart = (PNTFS_ATTR_RECORD)ExAllocatePoolWithTag(NonPagedPool, (ULONG)ListSize, TAG_NTFS);

    if (Context->NonResidentStart == NULL)
    {
        ReleaseAttributeContext(ListContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (ReadAttribute(Context->Vcb, ListContext, 0, (PCHAR)Context->NonResidentStart, (ULONG)ListSize) != ListSize)
    {
        ExFreePoolWithTag(Context->NonResidentStart, TAG_NTFS);
        Context->NonResidentStart = NULL;
        ReleaseAttributeContext(ListContext);
        return STATUS_FILE_CORRUPT_ERROR;
    }

    ReleaseAttributeContext(ListContext);
    Context->NonResidentEnd = (PNTFS_ATTR_RECORD)((PCHAR)Context->NonResidentStart + ListSize);
    return STATUS_SUCCESS;
}



static
PNTFS_ATTR_RECORD
InternalGetNextAttribute(PFIND_ATTR_CONTXT Context)
{
    PNTFS_ATTR_RECORD NextAttribute;

    if (Context->CurrAttr == (PVOID)-1)
    {
        return NULL;
    }

    if (Context->CurrAttr >= Context->FirstAttr &&
        Context->CurrAttr < Context->LastAttr)
    {
        if (Context->CurrAttr->Length == 0)
        {
            cout << "Broken length!\n";
            Context->CurrAttr = (PNTFS_ATTR_RECORD)-1;
            return NULL;
        }

        NextAttribute = (PNTFS_ATTR_RECORD)((ULONG_PTR)Context->CurrAttr + Context->CurrAttr->Length);

        //
        Context->Offset += ((ULONG_PTR)NextAttribute - (ULONG_PTR)Context->CurrAttr);// Context->CurrAttr->Length;

        Context->CurrAttr = NextAttribute;

        if (Context->CurrAttr < Context->LastAttr &&
            Context->CurrAttr->Type != AttributeEnd)
        {
            return Context->CurrAttr;
        }
    }

    if (Context->NonResidentStart == NULL)
    {
        Context->CurrAttr = (PNTFS_ATTR_RECORD)-1;
        return NULL;
    }

    if (Context->CurrAttr < Context->NonResidentStart ||
        Context->CurrAttr >= Context->NonResidentEnd)
    {
        Context->CurrAttr = Context->NonResidentStart;
    }
    else if (Context->CurrAttr->Length != 0)
    {
        //Context->CurrAttr = (PNTFS_ATTR_RECORD)((ULONG_PTR)Context->CurrAttr + Context->CurrAttr->Length);
        //(PCHAR)Context->Offset += Context->CurrAttr->Length;
        NextAttribute = (PNTFS_ATTR_RECORD)((ULONG_PTR)Context->CurrAttr + Context->CurrAttr->Length);

        //
        Context->Offset += ((ULONG_PTR)NextAttribute - (ULONG_PTR)Context->CurrAttr);// Context->CurrAttr->Length;

        Context->CurrAttr = NextAttribute;
    }
    else
    {
        cout << "Broken length!\n";
        Context->CurrAttr = (PNTFS_ATTR_RECORD)-1;
        return NULL;
    }

    if (Context->CurrAttr < Context->NonResidentEnd &&
        Context->CurrAttr->Type != AttributeEnd)
    {
        return Context->CurrAttr;
    }

    Context->CurrAttr = (PNTFS_ATTR_RECORD)-1;
    return NULL;
}
