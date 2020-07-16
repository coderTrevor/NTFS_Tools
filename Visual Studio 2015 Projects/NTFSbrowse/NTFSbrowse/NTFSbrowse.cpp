// NTFSbrowse.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <iomanip>
#include <Windows.h>
#include "ntfs.h"
#include  <stdlib.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <conio.h>
#include <stdlib.h>

using namespace std;



NTFS_VCB Vcb;
MFTENTRY MasterFileTableMftEntry;
PNTFS_ATTR_RECORD pMasterFileTableDataRecord;
PNTFS_ATTR_CONTEXT MftContext;
BOOTFILE bootFile;





NTSTATUS
FixupUpdateSequenceArray(ULONG BytesPerSector,
    MFTENTRY *Record)
{
    USHORT *USA;
    USHORT USANumber;
    USHORT USACount;
    USHORT *Block;

    USA = (USHORT*)((ULONG_PTR)Record + Record->FixupArrayOffset);
    USANumber = *(USA++);
    USACount = Record->FixupEntryCount - 1; /* Exclude the USA Number. */
    Block = (USHORT*)((ULONG_PTR)Record + BytesPerSector - 2);

    while (USACount)
    {
        if (*Block != USANumber)
        {
            cout << "Mismatch with USA: " << *Block << " read " << USANumber << " %u expected\n";
            // return STATUS_UNSUCCESSFUL;
        }
        *Block = *(USA++);
        Block = (USHORT*)((ULONG_PTR)Block + BytesPerSector);
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
    BytesRead = ReadAttribute(hDrive, pBoot, pMasterFileTableDataRecord, index * 1024, (PCHAR)file, 1024);

    if (BytesRead != 1024)
    {
        cout << "ReadFileRecord failed. " << BytesRead << " read, expected: 1024\n";
        return -1;
    }

    if (file->Signature != NRH_FILE_TYPE)
    {
        cout << "Mega error!\n";
        return STATUS_UNSUCCESSFUL;
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
                if(memcmp(AttrName, Name, NameLength << 1) == 0)
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





void DumpMft(MFTENTRY entry, HANDLE hDrive)
{
    std::ofstream htmlfile;
    htmlfile.open("output.html", std::ios::out | std::ios::trunc);

    std::ofstream cssfile;
    cssfile.open("machinestyles.css", std::ios::out | std::ios::trunc);

    htmlfile << "<!DOCTYPE HTML>\n        <html lang = \"en\">\n        <head>\n";

    htmlfile << "        <title>File Record (MFT Entry)</title>\n        <meta http-equiv=\"Content-Type\" content = \"text/html; charset=utf-8\">\n        <link href = \"styles.css\" type = \"text/css\" rel = \"stylesheet\">\n        <link href = \"machinestyles.css\" type = \"text/css\" rel = \"stylesheet\">\n        </head>\n        <body>\n";

    htmlfile << "\n";

    htmlfile << "<div class=\"wrapper\">\n";

    //htmlfile << "<div class=\"MFT_Entry\">\n";

    //htmlfile << "<div class=\"MFT_Header\">\n";

    std::stringstream classname, legendid, legend;

    classname << "FileRecordHeader";
    legendid << "FHLegend";
    legend << "File Record Header";

    infoRange *pHeaderRange = new infoRange(0,
                                            entry.FirstAttributeOffset,
                                            &htmlfile,
                                            &cssfile,
                                            classname.str(),
                                            legendid.str(),
                                            legend.str(), false, false, true);
    
    infoRange *pHeaderData = pHeaderRange->AddChild(0,
                                           entry.FirstAttributeOffset,
                                           "FileRecordData",
                                           "FRDLegend",
                                           "File Record Data", true, false);

    //DumpFileRecordHeader(MFTENTRY entry, pHeaderRange, &htmlfile, &cssfile, hDrive);
    

    /*htmlfile << "<div id = \"MFTLegend\">File Record Header</div>\n";*/

    int currentByte = 0;
    stringstream fileSig;
    fileSig << "Magic number '" << ((PCHAR)&entry)[0] << ((PCHAR)&entry)[1] << ((PCHAR)&entry)[2] << ((PCHAR)&entry)[3] << "'";
   
    pHeaderData->AddChild(0, 4, "Magic", "MLegend", fileSig.str());
    pHeaderData->AddChild(4, 2, "OffsetToUpdate", "OTULegend", "Offset to Update Sequence");
    pHeaderData->AddChild(6, 2, "SizeOfUS", "SOUSLegend", "Size in words of Update Sequence");
    pHeaderData->AddChild(8, 8, "LogfileSN", "LSNLegend", "$LogFile Sequence Number");

    //infoRange sequenceRange(16, 2, "<div class=\"SequenceValue\">", "</div><div id=\"SeqLegend\">&nbsp;Sequence Value</div>", &htmlfile);
    pHeaderData->AddChild(0x10, 2, "SequenceValue", "SLegend", "&nbsp;Sequence Value");

    //infoRange linkCount(18, 2, "<div class=\"LinkCount\">", "</div><div id=\"LCLegend\">&nbsp;Link Count</div>", &htmlfile);
    pHeaderData->AddChild(0x12, 2, "LinkCount", "LCLegend", "&nbsp;Link Count");

    pHeaderData->AddChild(0x14, 2, "OffsetToFirst", "OTFALegend", "&nbsp;Offset to first Attribute");
    pHeaderData->AddChild(0x16, 2, "Flags", "FLegend", "&nbsp;Flags");

    //infoRange realSize(0x18, 4, "<div class=\"RealSize\">", "</div><div id=\"RSLegend\">&nbsp;Real Size</div>", &htmlfile);
    pHeaderData->AddChild(0x18, 4, "RealSize", "RSLegend", "&nbsp;Real Size");
    pHeaderData->AddChild(0x1C, 4, "AllSize", "ASLegend", "&nbsp;Allocated Size");

    //infoRange baseRecord(32, 8, "<div class=\"BaseRecord\">", "</div><div id=\"BRLegend\">&nbsp;File reference to base record</div>", &htmlfile);
    pHeaderData->AddChild(0x20, 8, "BaseRecord", "BRLegend", "&nbsp;File reference to base record");

    //infoRange nextAttributeID(40, 2, "<div class=\"NextAttribute\">", "</div><div id=\"NALegend\">&nbsp;Next Attribute ID</div>", &htmlfile);
    pHeaderData->AddChild(40, 2, "NextAttribute", "NALegend", "&nbsp;Next Attribute ID");

    pHeaderData->AddChild(0x2C, 4, "MftNum", "MNLegend", "&nbsp;MFT record index");
    
    infoRange fixupArray(entry.FixupArrayOffset, (ULONG)2 + 2 * entry.FixupEntryCount, &htmlfile, &cssfile, "FixupArray", "FULegend", "Fixup Array");

    std::stringstream fixupsig;
    fixupsig << "Fixup Signature (rev #" << (WORD)*(WORD*)((BYTE*)&entry + entry.FixupArrayOffset) << ")";
    infoRange fixupEntryCount(0, (ULONG)2, &htmlfile, &cssfile, "FixupSig", "FSLegend", fixupsig.str());
    fixupArray.pChildren.push_back(&fixupEntryCount);

    for (int i = 0; i < entry.FixupEntryCount; i++)
    {
        std::stringstream classname, idname, legendText;
        classname << "FixupEntry" << i;
        idname << "FELegend" << i;
        legendText << "Fixup Entry for sector " << i;

        infoRange *fixupEntry = new infoRange(2 + (i * 2), 2, &htmlfile, &cssfile, classname.str(), idname.str(), legendText.str(), false, false);
        fixupArray.pChildren.push_back(fixupEntry);
    }

    int i = 0, j = 0;

//    int currentByte = 0;

    for (i = 0; i < entry.FirstAttributeOffset; i += 16)
    {
        htmlfile << "\n<br>";

        // output the header first
        //if (i == 0)
        //    htmlfile << "<div class=\"Signature\">";

        for (j = 0; j < 16 && (i + j < entry.FirstAttributeOffset); j++, currentByte++)
        {
            /*if (i == 0 && j == 4)  // end of signature
            {
                htmlfile << "</div><div id=\"SigLegend\">&nbsp;\"FILE\" - Signature</div><div class =\"FixupArrayOffset\">";
            }

            if (i == 0 && j == 6) // end of fixuparrayoffset
            {
                htmlfile << "</div><div id=\"FAOLegend\">&nbsp;Offset to fixup array</div><div class=\"FixupEntryCount\">";
            }

            if (i == 0 && j == 8) // end of fixup entry count
                htmlfile << "</div><div id=\"FUCLegend\">&nbsp;Fixup Entry Count</div><div class=\"LogFileSequence\">";
                
            sequenceRange.PrintStart();
            linkCount.PrintStart();
            baseRecord.PrintStart();
            nextAttributeID.PrintStart();

            if (currentByte == entry.FirstAttributeOffset)
                htmlfile << "</div>";*/
            pHeaderRange->PrintStart();
            fixupArray.PrintStart();


            htmlfile << std::setw(2) << std::hex << std::setfill('0') << (int)((PBYTE)&entry)[i + j] << "&nbsp;";

            fixupArray.advance(1);
            pHeaderRange->advance(1);
            /*sequenceRange.advance(1);
            linkCount.advance(1);
            baseRecord.advance(1);
            nextAttributeID.advance(1);

            sequenceRange.PrintEnd();
            linkCount.PrintEnd();
            baseRecord.PrintEnd();
            nextAttributeID.PrintEnd();*/
            fixupArray.PrintEnd();
            pHeaderRange->PrintEnd();
        }

        //if (i == 0) // end of log file sequence number
         //   htmlfile << "</div><div id=\"LSNLegend\">&nbsp;($LogFile) Sequence Number</div>\n";


        /*cout << " | ";

        for (int j = 0; j < 16; j++)
        cout << ((PBYTE)&entry)[i + j];
        */

        fixupArray.PrintEnd();
        pHeaderRange->PrintEnd();
    }
    //htmlfile << "</div>\n</div>\n";

    pHeaderRange->PrintCSS();
    fixupArray.PrintEnd();


    // now we need to output the attributes
    int attributeNumber = 0;
    PNTFS_ATTR_RECORD currentRecord = (PNTFS_ATTR_RECORD)((BYTE*)&entry + entry.FirstAttributeOffset);
    int currentOffset = entry.FirstAttributeOffset;

    while (currentRecord->Type != AttributeEnd)
    {
        htmlfile << "\n\n<!-- Attribute " << attributeNumber << " -->" << std::endl;
        std::stringstream classname, legendid, legend;

        classname << "Attribute" << attributeNumber;
        legendid << "Att" << attributeNumber << "Legend";
        legend << "Attribute " << attributeNumber;

        infoRange *pCurrentRange = new infoRange(0, currentRecord->Length, &htmlfile, &cssfile, classname.str(), legendid.str(),
                                                 legend.str(), false, false, true);

        DumpAttrib(currentRecord, pCurrentRange, &htmlfile, &cssfile, currentOffset, hDrive);

        //htmlfile << std::endl << "<br>";

        currentOffset += currentRecord->Length;
        currentRecord = (PNTFS_ATTR_RECORD)((BYTE*)currentRecord + currentRecord->Length);
        attributeNumber++;
    }

    //

    //htmlfile << "</div>\n";
    //htmlfile << "</div>\n";
    htmlfile << "</div>\n";
    htmlfile << "</body>\n";
    htmlfile << "</html>\n";

    fixupArray.PrintCSS();

    NtfsDumpFileAttributes(&Vcb, &entry);

    htmlfile.close();
    cssfile.close();

    ShellExecute(NULL, NULL, L"output.html", NULL, NULL, 0);

    //cout << "</div>\n";
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


void DumpAttributeData(PNTFS_ATTR_RECORD Attribute, stringstream &namestream, HANDLE hDrive)
{
    std::ofstream htmlfile;
    
    WCHAR Name[260];
    memset(Name, 0, 260 * 2);

    memcpy(Name, (PCHAR)Attribute + Attribute->NameOffset, Attribute->NameLength * 2);
    char ch[260];
    char DefChar = ' ';
    WideCharToMultiByte(CP_ACP, 0, Name, -1, ch, 260, &DefChar, NULL);

    std::stringstream cssName;
    namestream << ch;

    
    cssName << namestream.str() << "machinestyles.css";

    namestream << ".html";

    cout << "Using name: " << namestream.str() << "\n";

    htmlfile.open(namestream.str(), std::ios::out | std::ios::trunc);
    
    std::ofstream cssfile;
    cssfile.open(cssName.str(), std::ios::out | std::ios::trunc);

    htmlfile << "<!DOCTYPE HTML>\n        <html lang = \"en\">\n        <head>\n";

    htmlfile << "        <title>Data</title>\n        <meta http-equiv=\"Content-Type\" content = \"text/html; charset=utf-8\">\n        <link href = \"styles.css\" type = \"text/css\" rel = \"stylesheet\">\n";
    htmlfile << "<link href=\"" << cssName.str() << "\" type=\"text/css\" rel=\"stylesheet\">\n        </head>\n        <body>\n";

    htmlfile << "\n";

    htmlfile << "<div class=\"wrapper\">\n";

    //htmlfile << "<div id = \"DataLegend\">Data</div>\n";

    int currentByte = 0;

    ULONGLONG Length = (Attribute->IsNonResident) ? Attribute->NonResident.AllocatedSize : Attribute->Resident.ValueLength;
    infoRange *pCurrentRange = new infoRange(0, Length, &htmlfile, &cssfile, "DATA", "DATALegend",
                                             "Data", false, false, true);



    // read the data
    PCHAR Buffer = (PCHAR)malloc(Length);

    //PINDEX_ROOT_ATTRIBUTE indexRoot = (PINDEX_ROOT_ATTRIBUTE)malloc(rootContext->Record.Length);

    //ReadAttribute(&Vcb, rootContext, 0, (PCHAR)indexRoot, rootContext->Record.Length);

    PUCHAR DataRun = (PUCHAR)((ULONG_PTR)Attribute + Attribute->NonResident.MappingPairsOffset);

    ULONGLONG readLength = ReadAttribute(hDrive,
                                     &bootFile,
                                     Attribute,
                                     0,
                                     Buffer,
                                     Length);

    if (readLength != Length)
        cout << "\t\tINVALID READ LENGTH of " << readLength << ", was expecting " << Length << "\n";

    infoRange *pData = pCurrentRange;

    int nodeNumber = 0;
    for (ULONGLONG bytes = 0; bytes < readLength; bytes += 4096/*Vcb.Vpb->SizeOfIndexRecord*/)
    {
        std::stringstream recordName;
        recordName << "Index Record " << nodeNumber++;
        pCurrentRange = pData->AddChild(bytes, 0x1000, "NODERECORD", "NRLegend", recordName.str(), true, true);

        // apply the fixup array
        FixupUpdateSequenceArray(Vcb.Vpb->BytesPerSector, (MFTENTRY*)((ULONG_PTR)Buffer + bytes));

        ULONGLONG IndexEntryOffset;
        memcpy(&IndexEntryOffset, Buffer + bytes + 0x18, 4);

        ULONGLONG IndexEntriesSize;
        memcpy(&IndexEntriesSize, Buffer + bytes + 0x18 + 4, 4);

        ULONGLONG AllocatedSize;
        memcpy(&AllocatedSize, Buffer + bytes + 0x18 + 8, 4);

        // this is all wrong: need to offset by bytes variable
        infoRange *pAllocated = pCurrentRange->AddChild(0x18, AllocatedSize, "ALL", "ALLLegend", "Allocated Node", true, false);
        infoRange* pIndices = pAllocated->AddChild(0, IndexEntriesSize, "IES", "IESLegend", "Index Entries", true, false);


        stringstream fileSig;
        PCHAR FileSigPtr = (PCHAR)((ULONG_PTR)Buffer + bytes);
        fileSig << "Magic number '" << FileSigPtr[0] << FileSigPtr[1] << FileSigPtr[2] << FileSigPtr[3] << "'";

        pCurrentRange->AddChild(0, 4, "INDX", "INDXLegend", fileSig.str(), false, true);
        pCurrentRange->AddChild(4, 2, "USO", "USOLegend", "Offset to the Update Sequence", false, true);
        pCurrentRange->AddChild(6, 2, "USS", "USSLegend", "Size in words of the Update Sequence", false, true);
        pCurrentRange->AddChild(8, 8, "LSN", "LSNLegend", "$LogFile squence number", false, true);
        pCurrentRange->AddChild(16, 8, "VCN", "VCNLegend", "VCN of this Index record in the Index Allocation", false, true);

        infoRange *pIndexNodeHeader = pCurrentRange->AddChild(0x18, 16, "INH", "INHLegend", "Index Node Header", true, true);
        pIndexNodeHeader->AddChild(0, 4, "OIE", "OIELegend", "Offset to first Index Entry", false, true);
        pIndexNodeHeader->AddChild(4, 4, "TIE", "TIELegend", "Total size of the Index Entries", false, true);
        pIndexNodeHeader->AddChild(8, 4, "AN", "ANLegend", "Allocated size of the node", false, true);
        pIndexNodeHeader->AddChild(12, 1, "NL", "NLLegend", "subnode flag", false, true);
        pIndexNodeHeader->AddChild(13, 3, "PAD", "PADLegend", "Padding", false, true);

        pCurrentRange->AddChild(0x28, 2, "US", "USLegend", "Update Sequence", false, true);

        WORD USsize;
        memcpy(&USsize, Buffer + 6, 2);
        pCurrentRange->AddChild(0x2a, USsize, "USA", "USALegend", "Update sequence array", false, true);

        ULONG currentOffset = IndexEntryOffset;
        PINDEX_ENTRY_ATTRIBUTE currentEntry = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)Buffer + bytes + IndexEntryOffset + 0x18);
        int index = 0;
        bool done = false;
        while (currentOffset < IndexEntriesSize)
        {
            stringstream entryName;
            entryName << "Index Entry " << index++;
            infoRange *pEntry;
            if(!done)
                pEntry = pIndices->AddChild(currentOffset, currentEntry->Length, "IE", "IELegend", entryName.str(), true, false);
            else
            {
                infoRange *pSpace = pAllocated->AddChild(currentOffset, AllocatedSize - IndexEntriesSize, "IS", "ISLegend", "Slack Space", true, false);
                break;
            }
            pEntry->AddChild(0, 8, "FR", "FRLegend", "File reference", false, true);
            pEntry->AddChild(8, 2, "LIE", "LIELegend", "Length of index entry", false, true);
            pEntry->AddChild(10, 2, "LOS", "LOSLegend", "Length of stream", false, true);
            pEntry->AddChild(12, 1, "F", "FLegend", "Flags", false, true);

            if (currentEntry->Flags & 2)
            {
                done = true;
            }
            else
            {
                infoRange *pStream = pEntry->AddChild(16, currentEntry->KeyLength, "ST", "STLegend", "Stream", true, true);
                infoRange *pFilename = pStream->AddChild(0, currentEntry->KeyLength, "FN", "FNLegend", "Filename", true, true);

                pFilename->AddChild(0, 8, "FR", "FRLegend", "File reference");
                pFilename->AddChild(8, 8, "CT", "CTLegend", "File Creation time");
                pFilename->AddChild(0x10, 8, "AT", "ATLegend", "File Altered time");
                pFilename->AddChild(0x18, 8, "MT", "MTLegend", "MFT changed time");
                pFilename->AddChild(0x20, 8, "RT", "RTLegend", "File read time");
                pFilename->AddChild(0x28, 8, "AS", "ASLegend", "Allocated size");
                pFilename->AddChild(0x30, 8, "FS", "FSLegend", "File size");
                pFilename->AddChild(0x38, 4, "FF", "FFLegend", "Filename flags");

                //currentEntry->FileName.NameLength
                WCHAR Name[260];
                memset(Name, 0, 260 * 2);

                memcpy(Name, (PCHAR)currentEntry->FileName.Name, currentEntry->FileName.NameLength * 2);
                char ch[260];
                char DefChar = ' ';
                WideCharToMultiByte(CP_ACP, 0, Name, -1, ch, 260, &DefChar, NULL);

                std::stringstream fName;
                fName << "File name - '" << ch << "'";
                pFilename->AddChild(0x42, currentEntry->FileName.NameLength * 2, "N", "NLegend", fName.str(), false, true);

            }

            if (currentEntry->Flags & 1)
                pEntry->AddChild(currentEntry->Length - 8, 8, "VCN", "VCNLegend", "VCN of the sub-node in the Index Allocation");

            currentOffset += currentEntry->Length;
            currentEntry = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)currentEntry + currentEntry->Length);
            if (currentEntry->Length == 0)
                break;
        }
    }

    while (currentByte < Length)
    {
        pData->PrintStart();
        pData->PrintEnd();
        htmlfile << "\n<br>";

        for (int j = 0; j < 16; j++)
        {
            pData->PrintStart();
            BYTE dataByte;
            memcpy(&dataByte, (VOID*)((ULONG_PTR)Buffer + currentByte + j), 1);

            int dataInt = (int)dataByte;
            htmlfile << std::setw(2) << std::hex << std::setfill('0') << dataInt << "&nbsp;";

            if (j == 7)
                htmlfile << "&nbsp;";

            pData->advance(1);

            pData->PrintEnd();
        }
        currentByte += 16;
    }

    pData->PrintCSS();

    /* PLain data:
    while (currentByte < Length)
    {
        htmlfile << "\n<br>";

        for (int j = 0; j < 16; j++)
        {
            //pCurrentRange->PrintStart();
            BYTE dataByte;
            memcpy(&dataByte, (VOID*)((ULONG_PTR)Buffer + currentByte + j), 1);

            int dataInt = (int)dataByte;
            htmlfile << std::setw(2) << std::hex << std::setfill('0') << dataInt << "&nbsp;";

            if (j == 7)
                htmlfile << "&nbsp;";

            //pCurrentRange->advance(1);

            //pCurrentRange->PrintEnd();
        }
        currentByte += 16;
    }*/
    //htmlfile << Buffer;

    //DumpAttrib(currentRecord, pCurrentRange, &htmlfile, &cssfile, currentOffset);

    /*infoRange sequenceRange(16, 2, "<div class=\"SequenceValue\">", "</div><div id=\"SeqLegend\">&nbsp;Sequence Value</div>", &htmlfile);

    infoRange linkCount(18, 2, "<div class=\"LinkCount\">", "</div><div id=\"LCLegend\">&nbsp;Link Count</div>", &htmlfile);

    infoRange baseRecord(32, 8, "<div class=\"BaseRecord\">", "</div><div id=\"BRLegend\">&nbsp;File reference to base record</div>", &htmlfile);

    infoRange nextAttributeID(40, 2, "<div class=\"NextAttribute\">", "</div><div id=\"NALegend\">&nbsp;Next Attribute ID</div>", &htmlfile);

    infoRange fixupArray(entry.FixupArrayOffset, (ULONG)2 + 2 * entry.FixupEntryCount, &htmlfile, &cssfile, "FixupArray", "FULegend", "Fixup Array");
    
    std::stringstream fixupsig;
    fixupsig << "Fixup Signature (rev #" << (WORD)*(WORD*)((BYTE*)&entry + entry.FixupArrayOffset) << ")";
    infoRange fixupEntryCount(0, (ULONG)2, &htmlfile, &cssfile, "FixupSig", "FSLegend", fixupsig.str());
    fixupArray.pChildren.push_back(&fixupEntryCount);

    for (int i = 0; i < entry.FixupEntryCount; i++)
    {
        std::stringstream classname, idname, legendText;
        classname << "FixupEntry" << i;
        idname << "FELegend" << i;
        legendText << "Fixup Entry for sector " << i;

        infoRange *fixupEntry = new infoRange(2 + (i * 2), 2, &htmlfile, &cssfile, classname.str(), idname.str(), legendText.str());
        fixupArray.pChildren.push_back(fixupEntry);
    }
    */
    /*int i = 0, j = 0;


    for (i = 0; i < entry.FirstAttributeOffset; i += 16)
    {
        htmlfile << "\n<br>";

        // output the header first
        if (i == 0)
            htmlfile << "<div class=\"Signature\">";

        for (j = 0; j < 16 && (i + j < entry.FirstAttributeOffset); j++, currentByte++)
        {
            if (i == 0 && j == 4)  // end of signature
            {
                htmlfile << "</div><div id=\"SigLegend\">&nbsp;\"FILE\" - Signature</div><div class =\"FixupArrayOffset\">";
            }

            if (i == 0 && j == 6) // end of fixuparrayoffset
            {
                htmlfile << "</div><div id=\"FAOLegend\">&nbsp;Offset to fixup array</div><div class=\"FixupEntryCount\">";
            }

            if (i == 0 && j == 8) // end of fixup entry count
                htmlfile << "</div><div id=\"FUCLegend\">&nbsp;Fixup Entry Count</div><div class=\"LogFileSequence\">";

            sequenceRange.PrintStart();
            linkCount.PrintStart();
            baseRecord.PrintStart();
            nextAttributeID.PrintStart();
            fixupArray.PrintStart();

            if (currentByte == entry.FirstAttributeOffset)
                htmlfile << "</div>";

            htmlfile << std::setw(2) << std::hex << std::setfill('0') << (int)((PBYTE)&entry)[i + j] << "&nbsp;";

            sequenceRange.advance(1);
            linkCount.advance(1);
            baseRecord.advance(1);
            nextAttributeID.advance(1);
            fixupArray.advance(1);

            sequenceRange.PrintEnd();
            linkCount.PrintEnd();
            baseRecord.PrintEnd();
            nextAttributeID.PrintEnd();
            fixupArray.PrintEnd();
        }

        if (i == 0) // end of log file sequence number
            htmlfile << "</div><div id=\"LSNLegend\">&nbsp;($LogFile) Sequence Number</div>\n";


      //cout << " | ";

        //for (int j = 0; j < 16; j++)
        //cout << ((PBYTE)&entry)[i + j];
        

    }
    htmlfile << "</div>\n";


    // now we need to output the attributes
    int attributeNumber = 0;
    PNTFS_ATTR_RECORD currentRecord = (PNTFS_ATTR_RECORD)((BYTE*)&entry + entry.FirstAttributeOffset);
    int currentOffset = entry.FirstAttributeOffset;

    while (currentRecord->Type != AttributeEnd)
    {
        htmlfile << "\n\n<!-- Attribute " << attributeNumber << " -->\n";
        std::stringstream classname, legendid, legend;

        classname << "Attribute" << attributeNumber;
        legendid << "Att" << attributeNumber << "Legend";
        legend << "Attribute " << attributeNumber;

        infoRange *pCurrentRange = new infoRange(0, currentRecord->Length, &htmlfile, &cssfile, classname.str(), legendid.str(),
                                                 legend.str(), false, false, true);

        DumpAttrib(currentRecord, pCurrentRange, &htmlfile, &cssfile, currentOffset);

        currentOffset += currentRecord->Length;
        currentRecord = (PNTFS_ATTR_RECORD)((BYTE*)currentRecord + currentRecord->Length);
        attributeNumber++;
    }
    */
    //

    htmlfile << "</div>\n";
    //htmlfile << "</div>\n";
    htmlfile << "</body>\n";
    htmlfile << "</html>\n";

    //fixupArray.PrintCSS();
    //
    //NtfsDumpFileAttributes(&Vcb, &entry);

    htmlfile.close();
    cssfile.close();

    //ShellExecute(NULL, NULL, L"output.html", NULL, NULL, 0);

    //cout << "</div>\n";*/
}

void DumpAttributes(MFTENTRY entry)
{
    NTFS_ATTR_RECORD attribute;

    DWORD currentOffset = entry.FirstAttributeOffset;

    WCHAR AttrName[256];

    DWORD endValue;
    bool done = false;

    memcpy(&endValue, ((BYTE*)&entry) + currentOffset, 4);

    while (!done)
    {
        // copy the attribute
        memcpy(&attribute, ((BYTE*)&entry) + currentOffset, sizeof(NTFS_ATTR_RECORD));
        memcpy(&endValue, ((BYTE*)&entry) + currentOffset, 4);

        if (endValue == 0xffFFffFF || endValue == 0)
            break;

        if (currentOffset >= 1024)
        {
            cout << "Went past end of MFT\n";
            break;
        }

        if (attribute.NameLength > 0)
        {
            memset(AttrName, 0, sizeof(WCHAR) * 256);
            memcpy(AttrName, ((BYTE*)&attribute) + attribute.NameOffset, attribute.NameLength * 2);
            wcout << AttrName << "\n";
        }
        else
        {
           // cout << "found Attribute without name " << attribute.Type << "\n";
        }
        
        if (attribute.Length == 0)
        {
            cout << "Attribute with length 0\n";
            break;
        }

        if (attribute.Type == 0x30)    // $FILE_NAME attribute
        {
            if (!attribute.IsNonResident)
            {
                memset(AttrName, 0, sizeof(WCHAR) * 256);
                int offset = currentOffset + attribute.Resident.ValueOffset + 64;
                BYTE nameLength;
                memcpy(&nameLength, ((BYTE*)&entry) + offset, 1);
                offset += 2;
                memcpy(AttrName, ((BYTE*)&entry) + offset, nameLength * 2);

                wcout << AttrName << "\n";
            }
        }
        else
            cout << "Attribute with type: " << attribute.Type << "\n";

        if( attribute.IsNonResident )
            currentOffset += (attribute.Length);
        else
            currentOffset += (attribute.Length);
    }
}

void DumpParticularEntry(HANDLE hDrive)
{
    int entryIndex = 5;
    cout << "Index of MFT entry to dump: ";

    cin >> entryIndex;

    MFTENTRY mfte;
    ReadFileRecord(hDrive,
                   Vcb.Vpb,
                   entryIndex,
                   &mfte);

    cout << "Dumping entry #" << entryIndex << "\n";

    NtfsDumpFileAttributes(&Vcb, (PFILE_RECORD_HEADER)&mfte);

    // Dumps the mft entry to an html file
    DumpMft(mfte, hDrive);
}

int GetMenuChoice(int numChoices, int defaultSelection, char *szChoices, ...)
{
    system("cls");
    va_list choiceList;
    int i;
    int selection = defaultSelection;

    COORD cur = { 0, 0 };

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


// Name should be defined as WCHAR Name[260];
// mbs should be defined as char mbs[260];
void FileNameToMB(WCHAR *Name, char *mbs, PINDEX_ENTRY_ATTRIBUTE CurrentEntry)
{
    memset(Name, 0, 260 * 2);

    memcpy(Name, (PCHAR)CurrentEntry->FileName.Name, CurrentEntry->FileName.NameLength * sizeof(WCHAR));
    char DefChar = ' ';
    WideCharToMultiByte(CP_ACP, 0, Name, -1, mbs, 260, &DefChar, NULL);
}

PINDEX_ENTRY_ATTRIBUTE GetDirectoryChoice(vector<PINDEX_ENTRY_ATTRIBUTE> *entries, HANDLE hDrive, NTFS_VCB *Vcb, string dirName)
{
    system("cls");
    va_list choiceList;
    int i;
    int selection = 0;
    int numChoices = entries->size();
    int pageOffset = 0;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int columns, rows;

    COORD cur = { 0, 0 };

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    bool done = false;
    WCHAR Name[260];
    char mbs[260];
    bool showHelp = true;
    /*for (int i = 0; i < 255; i++)
    {
        cout << i << " " << (char)i << endl;
    }
    AnyKey();*/

    while (!done)
    {
        system("cls");
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), cur);

        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

        //printf("columns: %d\n", columns);
        //printf("rows: %d\n", rows);

        int numLines = rows - 5;
        if (showHelp)
            numLines -= 4;

        if (numChoices == 0)
            return NULL;

        cout << "Browsing " << dirName << "\n";
        if (showHelp)
        {
            cout << "Use " << (char)30 << " & " << (char)31 << " to select a file. Press PgUp or PgDown to move 10 slots up or down.\n";
            cout << "Press Enter to browse a directory or dump a file. Press D to dump a directory.\n";
            cout << "Press Esc or Backspace to go up one directory. Press H to Hide / show help.\nPress Q to quit.\n";
        }
        cout << "-------------------------------------------------------------------------------\n";

        //cout << "Use "
        //va_start(choiceList, szChoices);

        for (i = pageOffset; i < numChoices && (i - pageOffset - 1 < numLines); ++i)
        {
            if (selection == i)
                SetConsoleTextAttribute(hConsole, 0xF0);
            else
                SetConsoleTextAttribute(hConsole, 0x0F);

            PINDEX_ENTRY_ATTRIBUTE CurrentEntry = entries->at(i);
            FileNameToMB(Name, mbs, CurrentEntry); //va_arg(choiceList, char *);

            if (selection == i)
            {
                ULONGLONG Index = CurrentEntry->Data.Directory.IndexedFile & NTFS_MFT_MASK;
                cout << Index << "\t";
            }
            else
                cout << "\t";

            if (CurrentEntry->FileName.FileAttributes & NTFS_FILE_TYPE_DIRECTORY)
                cout << "[" << mbs << "]\n";
            else
                cout << mbs << endl;

            SetConsoleTextAttribute(hConsole, 0x0F);
            //string = va_arg(choiceList, char *);
        }

        //cout << selection << endl;

        char key = getch();
        cout << key << endl;

        // was an arrow key pressed?
        if (key == -32)
        {
            char ch = getch();
            switch (ch)
            {
                case 72:
                    // up arrow
                    selection--;
                    break;
                case 73:
                    // page up
                    selection -= 10;
                    if (selection < 0)
                        selection = 0;
                    break;
                case 80:
                    // down arrow
                    selection++;
                    break;
                case 81:
                    // page down
                    selection += 10;
                    if (selection >= numChoices)
                        selection = numChoices - 1;
                    break;
                default:
                    cout << (int)ch << endl;
            }
            if (selection < 0)
                selection = numChoices - 1;

            selection = selection % numChoices;

            while (selection < pageOffset)
                pageOffset--;
            while (selection - pageOffset > numLines)
                pageOffset++;
        }

        // Check for escape or backspace
        if (key == 27 || key == '\b')
        {
            return (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)-1);
        }

        // Check for enter
        if (key == 13)
        {
            SetConsoleTextAttribute(hConsole, 0x0F);
            return entries->at(selection);
        }

        if (key == 'd' || key == 'D')
        {
            // Read the file record
            MFTENTRY FileRecordToDump;
            PINDEX_ENTRY_ATTRIBUTE ChoiceEntry = entries->at(selection);

            NTSTATUS Status = ReadFileRecord(hDrive, Vcb->Vpb, ChoiceEntry->Data.Directory.IndexedFile & NTFS_MFT_MASK, &FileRecordToDump);
            if (!NT_SUCCESS(Status))
            {
                cout << "ERROR: Failed to read file record!\n";
                return 0;
            }

            DumpMft(FileRecordToDump, hDrive);

            exit(0);
        }

        // Check for quit
        if (key == 'q' || key == 'Q')
        {
            exit(0);
        }

        // Check for help
        if(key == 'h' || key == 'H')
        {
            showHelp = !showHelp;
        }

    }

    //va_end(choiceList);

    return entries->at(selection);
}

ULONGLONG
GetIndexEntryVCN(PINDEX_ENTRY_ATTRIBUTE IndexEntry)
{
    PULONGLONG Destination = (PULONGLONG)((ULONG_PTR)IndexEntry + IndexEntry->Length - sizeof(ULONGLONG));

  //  ASSERT(IndexEntry->Flags & NTFS_INDEX_ENTRY_NODE);

    return *Destination;
}

void AddIndexEntryToVector(vector<PINDEX_ENTRY_ATTRIBUTE> *theVector, PINDEX_ENTRY_ATTRIBUTE IndexEntry)
{
    PINDEX_ENTRY_ATTRIBUTE EntryCopy = (PINDEX_ENTRY_ATTRIBUTE)malloc(IndexEntry->Length);
    if (!EntryCopy)
    {
        cout << "FDailed to alloceate EntryCopy!\n";    // close enough
        return;
    }

    memcpy(EntryCopy, IndexEntry, IndexEntry->Length);

    theVector->push_back(EntryCopy);
}

void ParseNode(PNTFS_VCB Vcb,
               vector<PINDEX_ENTRY_ATTRIBUTE> *IndexEntries,
               ULONGLONG VCN,
               ULONG BytesPerSector,
               ULONG BytesPerCluster,
               ULONG BytesPerIndexRecord,
               PNTFS_ATTR_CONTEXT IndexAllocation,
               PFILE_RECORD_HEADER FileRecord)
{
    ULONGLONG Offset = VCN * BytesPerCluster;
    PINDEX_BUFFER IndexRecord = (PINDEX_BUFFER)malloc(BytesPerIndexRecord);
    if (!IndexRecord)
    {
        cout << "Err! Couldn't allocate index record\n";
        return;
    }

    ULONG BytesRead = ReadAttribute(Vcb, IndexAllocation, Offset, (PCHAR)IndexRecord, BytesPerIndexRecord);
    if (BytesRead != BytesPerIndexRecord)
    {
        cout << "Error! BytesRead != BytesPerIndexRecord!\n";
        return;
    }

    FixupUpdateSequenceArray(BytesPerSector, (MFTENTRY*)IndexRecord);


    PINDEX_ENTRY_ATTRIBUTE CurrentEntry = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)&IndexRecord->Header + IndexRecord->Header.FirstEntryOffset); 
    WCHAR Name[260];
    while (CurrentEntry)
    {
        // is there a sub-node?
        if (CurrentEntry->Flags & NTFS_INDEX_ENTRY_NODE)
        {
            ParseNode(Vcb,
                      IndexEntries,
                      GetIndexEntryVCN(CurrentEntry),
                      BytesPerSector,
                      BytesPerCluster,
                      4096,
                      IndexAllocation,
                      FileRecord);
        }

        if (CurrentEntry->Flags & NTFS_INDEX_ENTRY_END)
            break;

        memset(Name, 0, 260 * 2);

        memcpy(Name, (PCHAR)CurrentEntry->FileName.Name, CurrentEntry->FileName.NameLength * sizeof(WCHAR));
        char ch[260];
        char DefChar = ' ';
        WideCharToMultiByte(CP_ACP, 0, Name, -1, ch, 260, &DefChar, NULL);

        // skip dos names
        //if (CurrentEntry->FileName.NameType != NTFS_FILE_NAME_DOS)
        {
            bool isDir = false;
            if (CurrentEntry->FileName.FileAttributes & NTFS_FILE_TYPE_DIRECTORY)
                isDir = true;

            if (isDir)
                cout << "[" << ch << "]\n";
            else
                cout << ch << "\n";

            AddIndexEntryToVector(IndexEntries, CurrentEntry);
        }

        CurrentEntry = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)CurrentEntry + CurrentEntry->Length);
    }
}

ULONGLONG BrowseDirectory(HANDLE hDrive, NTFS_VCB *Vcb, ULONGLONG MftIndex, string dirName)
{
    MftIndex = MftIndex & NTFS_MFT_MASK;
    vector<PINDEX_ENTRY_ATTRIBUTE> IndexEntries;

    MFTENTRY mfte;
    NTSTATUS Status = ReadFileRecord(hDrive,
                                     Vcb->Vpb,
                                     MftIndex,
                                     &mfte);
    if (!NT_SUCCESS(Status))
    {
        cout << "ERROR: couldn't read file record!\n";
        return 0;
    }

    PNTFS_ATTR_CONTEXT IndexRootContext;

    // Find index root
    Status = FindAttribute(Vcb, &mfte, AttributeIndexRoot, L"$I30", 4, &IndexRootContext, NULL);
    if (!NT_SUCCESS(Status))
    {
        cout << "ERROR: Couldn't find index root!\n";
        return Status;
    }

    ULONGLONG IndexRootSize = AttributeDataLength(IndexRootContext->pRecord);

    PINDEX_ROOT_ATTRIBUTE IndexRoot = (PINDEX_ROOT_ATTRIBUTE)malloc((ULONG)IndexRootSize);
    if (!IndexRoot)
    {
        cout << "Couldn't allocate index root!\n";
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ULONG BytesRead = ReadAttribute(Vcb, IndexRootContext, 0, (PCHAR)IndexRoot, (ULONG)IndexRootSize);
    if (BytesRead != IndexRootSize)
    {
        cout << "ERROR: failed to read index root!\n";
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    PNTFS_ATTR_CONTEXT IndexAllocationContext = NULL;
    if (IndexRoot->Header.Flags & INDEX_ROOT_LARGE)
    {
        // find index allocation
        Status = FindAttribute(Vcb, &mfte, AttributeIndexAllocation, L"$I30", 4, &IndexAllocationContext, NULL);
        if (!NT_SUCCESS(Status))
        {
            cout << "ERROR: Couldn't find index allocation!\n";
            return Status;
        }
    }

    WCHAR Name[260];
    PINDEX_ENTRY_ATTRIBUTE CurrentAttribute = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)&IndexRoot->Header + IndexRoot->Header.FirstEntryOffset);
    while (CurrentAttribute)
    {
        // is there a sub-node?
        if (CurrentAttribute->Flags & NTFS_INDEX_ENTRY_NODE)
        {
            ParseNode(Vcb,
                      &IndexEntries,
                      GetIndexEntryVCN(CurrentAttribute),
                      bootFile.BytesPerSector,
                      bootFile.BytesPerSector * bootFile.SectorsPerCluster,
                      4096,
                      IndexAllocationContext,
                      &mfte);
        }

        if (CurrentAttribute->Flags & NTFS_INDEX_ENTRY_END)
            break;

        memset(Name, 0, 260 * 2);

        memcpy(Name, (PCHAR)CurrentAttribute->FileName.Name, CurrentAttribute->FileName.NameLength * sizeof(WCHAR));
        char ch[260];
        char DefChar = ' ';
        WideCharToMultiByte(CP_ACP, 0, Name, -1, ch, 260, &DefChar, NULL);

        bool isDir = false;
        if (CurrentAttribute->FileName.FileAttributes & NTFS_FILE_TYPE_DIRECTORY)
            isDir = true;

        if (isDir)
            cout << "[" << ch << "]\n";
        else
            cout << ch << "\n";

        AddIndexEntryToVector(&IndexEntries, CurrentAttribute);

        CurrentAttribute = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)CurrentAttribute + CurrentAttribute->Length);
    }

    PINDEX_ENTRY_ATTRIBUTE ChoiceEntry = GetDirectoryChoice(&IndexEntries, hDrive, Vcb, dirName);
    if ((ULONG_PTR)ChoiceEntry == (ULONG_PTR)-1)
    {
        // Find the mft index and dirName of the parent
        PFILENAME_ATTRIBUTE BestFileName = GetBestFileNameFromRecord(Vcb, &mfte);
        if (!BestFileName)
        {
            cout << "ERROR: Couldn't find filename of current directory!\n";
            exit(1);
        }

        // get the mft index
        ULONGLONG ParentIndex = BestFileName->DirectoryFileReferenceNumber & NTFS_MFT_MASK;

        // get the parent dir name
        std::size_t lastBackslash = dirName.find_last_of('\\');

        dirName = dirName.substr(0, lastBackslash);

        return BrowseDirectory(hDrive, Vcb, ParentIndex, dirName);
    }
    else if (ChoiceEntry && ChoiceEntry->FileName.FileAttributes & NTFS_FILE_TYPE_DIRECTORY)
    {
        WCHAR Name[260];
        char mbs[260];
        FileNameToMB(Name, mbs, ChoiceEntry);

        return BrowseDirectory(hDrive, Vcb, ChoiceEntry->Data.Directory.IndexedFile, dirName + "\\" + mbs);
    }
    else if (ChoiceEntry)
    {
        // Read the file record
        MFTENTRY FileRecordToDump;

        Status = ReadFileRecord(hDrive, Vcb->Vpb, ChoiceEntry->Data.Directory.IndexedFile & NTFS_MFT_MASK, &FileRecordToDump);
        if (!NT_SUCCESS(Status))
        {
            cout << "ERROR: Failed to read file record!\n";
            return 0;
        }

        DumpMft(FileRecordToDump, hDrive);

        return 0;
    }

    if (!ChoiceEntry)
    {
        cout << "Empty directory encountered. Can't browse any further.\nDumping file record...\n";
        
        DumpMft(mfte, hDrive);

        return 0;
    }

    return ChoiceEntry->Data.Directory.IndexedFile;
}

int main()
{
    // open drive for reading
    // opening a volume is better than opening a physicaldrive, because the offsets will be the same for this 
    // app as they are for the driver, making debugging easier

    cout << "Select drive to open: ";

    char driveLetter;
    cin >> driveLetter;

    char pathName[7] = "\\\\.\\C:";
    pathName[4] = driveLetter;

    cout << "About to open " << pathName << "\n";

    HANDLE hDrive = CreateFileA(pathName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (hDrive == INVALID_HANDLE_VALUE)
    {
        cout << "Couldn't open drive\n";
        return -1;
    }

    cout << "Opened " << pathName << " drive\n";

    // now let's find the start of the mft
   
    // copy the boot file
    DWORD Read;
    cout << "Size of Boot file struct: " << sizeof(BOOTFILE) << "\n";
    cout << "Size of mft entry struct: " << sizeof(MFTENTRY) << "\n";
    if (!ReadFile(hDrive, &bootFile, 512, &Read, NULL))
    {
        cout << "Unable to read from drive. Error code: " << GetLastError() << "\n";
        CloseHandle(hDrive);
        return -1;
    }

    Vcb.hDrive = hDrive;
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
    cout << "Mft entry size: " << (int)bootFile.SizeOfMftEntry << "\n";
    //cout << std::hex << bootFile.SerialNumber.HighPart << "-" << bootFile.SerialNumber.LowPart << "\n";
    cout << "Cluster size: " << (int)bootFile.BytesPerSector * bootFile.SectorsPerCluster << "\n";
    cout << "Size of Index Record: " << bootFile.SizeOfIndexRecord << "\n";
    
    
    LARGE_INTEGER FileSystemSize;
    FileSystemSize.QuadPart = bootFile.BytesPerSector * bootFile.SectorsInFileSystem.QuadPart;
    cout << std::dec << FileSystemSize.QuadPart << "\n";

    cout << FileSystemSize.QuadPart / 1024 / 1024 / 1024 << " gigs\n";

    cout << "First MFT Entry: " << std::hex << bootFile.MftFirstCluster.QuadPart << "\n";

    // find the mft entry
    LARGE_INTEGER mftStart;
    mftStart.QuadPart = bootFile.BytesPerSector * bootFile.SectorsPerCluster * bootFile.MftFirstCluster.QuadPart;

    cout << mftStart.QuadPart << "\n";
    
    if (!SetFilePointerEx(hDrive, mftStart, NULL, FILE_BEGIN))
        cout << "Unable to set file pointer to " << mftStart.QuadPart << "\nError: " << GetLastError() << "\n";

    //MFTENTRY mft; // first entry is always the mft
    memset(&MasterFileTableMftEntry, 0, 1024);
    if (!ReadFile(hDrive, &MasterFileTableMftEntry, 1024, &Read, NULL))
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
    NTSTATUS Status = FindAttribute(&Vcb, &MasterFileTableMftEntry, AttributeData, L"", 0,  &MftContext, NULL);
    if (!NT_SUCCESS(Status))
    {
        cout << "Can't find data attribute for Master File Table.\n";
        //free(DeviceExt->MasterFileTable);
        return Status;
    }

    pMasterFileTableDataRecord = MftContext->pRecord;

    // now that we've found the mft data attribute, let's try to read it!
    ULONG mftSize = (ULONG)AttributeDataLength(MftContext->pRecord);

    cout << "\n\nSize of Master File Table: " << std::dec << mftSize << "\n\n\n";

    mftSize = min(1024 * 1024 * 1024, (ULONG)AttributeDataLength(MftContext->pRecord));

    /*ULONG bufSize = min(1024 * 1024 * 1024, (ULONG)AttributeDataLength(&MftContext->Record));

    cout << "\n\nSize of Master File Table: " << std::dec << mftSize << "\n\n\n";
    cout << "\n" << mftSize / (1024 * 1024) << " megabytes will be allocatted for the MFT.\n";
    
    AnyKey();*/

        
    /*PMFTENTRY mft = (PMFTENTRY)calloc(mftSize / 1024, 1024); //(PMFTENTRY)malloc(mftSize);

    if (mft == NULL)
        cout << "Couldn't allocate memory for mft!!\n";

    ReadAttribute(&Vcb, MftContext, 0, (PCHAR)mft, mftSize);

    if (!SetFilePointerEx(hDrive, mftStart, NULL, FILE_BEGIN))
        cout << "Unable to set file pointer to " << mftStart.QuadPart << "\nError: " << GetLastError() << "\n";
    
    //DWORD Read;
    if (!ReadFile(hDrive, mft, mftSize, &Read, NULL))
    {
        cout << "Unable to perform read. Last error: " << GetLastError() << "\n";
    }*/

    /*for (int i = 0; i < mftSize / 1024; i++)
    {
        if (mft[i].Signature == 0x454c4946)
        {
             NtfsDumpFileAttributes(&Vcb, &mft[i]);
        }
    }*/

    //DumpAttributes(mft[5]);

    int choice = GetMenuChoice(3, 0, "Dump a particular file record", "Browse drive", "Exit");

    if (choice == 0)
    {
        DumpParticularEntry(hDrive);
    }
    else if (choice == 1)
    {
        stringstream drivePath;
        drivePath << driveLetter << ":";
        BrowseDirectory(hDrive, &Vcb, 5, drivePath.str());

        // Dumps the mft entry to an html file
        //DumpMft(mfte, hDrive);
    }

    // find and read the data attribute of mft record [5]
    /*PNTFS_ATTR_CONTEXT rootContext;

    Status = FindAttribute(&Vcb, &mfte, AttributeIndexRoot, L"", 0, &rootContext, NULL);
    if (!NT_SUCCESS(Status))
    {
        cout << "Can't find data $INDEX_ROOT attribute for root directory.\n";
        //free(DeviceExt->MasterFileTable);
        CloseHandle(hDrive);
        return Status;
    }

    PINDEX_ROOT_ATTRIBUTE indexRoot = (PINDEX_ROOT_ATTRIBUTE)malloc(rootContext->Record.Length);

    ReadAttribute(&Vcb, rootContext, 0, (PCHAR)indexRoot, rootContext->Record.Length);    */

    //free(mft);
    /*int entries = 1;

    MFTENTRY entry;
    memset(&entry, 0, 1024); 
    
    if (!ReadFile(hDrive, &entry, 1024, &Read, NULL))
    {
        cout << "unable to read mft entry. Error: " << GetLastError() << "\n";
        return -1;
    }*/

    /*do
    {
        if (!ReadFile(hDrive, &entry, 1024, &Read, NULL))
        {
            cout << "unable to read mft entry. Error: " << GetLastError() << "\n";
            return -1;
        }

        DumpAttributes(entry);

        entries++;
    } while (entry.Signature == FILESIG || entries < 32 );
    
    cout << "Found " << (entries - 1) << " entries in MFT.\n";
    */

    CloseHandle(hDrive);

    return 0;
}

