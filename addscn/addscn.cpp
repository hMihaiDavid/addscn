// addscn.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <windows.h>

using namespace std;

// Macros

// Source: https://blogs.oracle.com/jwadams/macros-and-powers-of-two
// align x down to the nearest multiple of align. align must be a power of 2.
#define P2ALIGNDOWN(x, align) ((x) & -(align))
// align x up to the nearest multiple of align. align must be a power of 2.
#define P2ALIGNUP(x, align) (-(-(x) & -(align)))


// Globals

HANDLE hFile = NULL, hFileMapping = NULL;
PBYTE pView = NULL;
DWORD dwFileSizeLow, dwFileSizeHigh;


WORD numberOfSections;;
DWORD sectionAlignment, fileAlignment;

PBYTE MapFileReadOnly() {


	__try {


		hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);

		if (hFileMapping == INVALID_HANDLE_VALUE) __leave;

		pView = (PBYTE)MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
	}
	__finally {
		if (hFileMapping == INVALID_HANDLE_VALUE) {
			CloseHandle(hFile);
		}
	}

	return pView;

}

PBYTE MapFileRWNewSize(DWORD newSize) {
	__try {

		hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, newSize, NULL);

		if (hFileMapping == INVALID_HANDLE_VALUE) __leave;

		pView = (PBYTE) MapViewOfFile(hFileMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
	}
	__finally {
		if (hFileMapping == INVALID_HANDLE_VALUE) {
			CloseHandle(hFile);
		}
	}

	return pView;
}

BOOL Unmap() {
	BOOL b1 = UnmapViewOfFile((PVOID)pView);
	BOOL b2 = CloseHandle(hFileMapping);

	if (b1 && b2) return TRUE;
	return FALSE;
}



VOID UnmapAndExit(UINT uExitCode) {
	if (!Unmap()) {
		wcout << L"Error in Unmap (" << hex << GetLastError() << L")" << endl;
		ExitProcess(EXIT_FAILURE);
	}

	CloseHandle(hFile);
	ExitProcess(uExitCode);
}

VOID AppendNewSectionHeader(PSTR name, DWORD VirtualSize, DWORD Characteristics) {


	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)pView;
	PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((UINT_PTR)pView + dosHeader->e_lfanew);
	WORD sizeOfOptionalHeader = ntHeaders->FileHeader.SizeOfOptionalHeader;
	PIMAGE_FILE_HEADER fileHeader = &(ntHeaders->FileHeader);
	PIMAGE_SECTION_HEADER firstSectionHeader = (PIMAGE_SECTION_HEADER)( ((UINT_PTR)fileHeader) + sizeof(IMAGE_FILE_HEADER) + sizeOfOptionalHeader);


	// We asssume there is room for a new section header.

	PIMAGE_SECTION_HEADER newSectionHeader = &firstSectionHeader[numberOfSections]; // Right after last section header.

	PIMAGE_SECTION_HEADER lastSectionHeader = &firstSectionHeader[numberOfSections - 1];

	memset(newSectionHeader, 0, sizeof(IMAGE_SECTION_HEADER));
	memcpy(&newSectionHeader->Name, name, min(strlen(name), 8));
	newSectionHeader->Misc.VirtualSize = VirtualSize;
	newSectionHeader->VirtualAddress = P2ALIGNUP(lastSectionHeader->VirtualAddress + lastSectionHeader->Misc.VirtualSize, sectionAlignment);
	newSectionHeader->SizeOfRawData = P2ALIGNUP(VirtualSize, fileAlignment);
	newSectionHeader->PointerToRawData = dwFileSizeLow; // at the end of the file before expanding its size.
	// this also works:
	//newSectionHeader->PointerToRawData = (DWORD)(lastSectionHeader->PointerToRawData + lastSectionHeader->SizeOfRawData);
	newSectionHeader->Characteristics = Characteristics;

	numberOfSections++;
	ntHeaders->FileHeader.NumberOfSections = numberOfSections;
	ntHeaders->OptionalHeader.SizeOfImage = P2ALIGNUP(newSectionHeader->VirtualAddress + newSectionHeader->Misc.VirtualSize, sectionAlignment);
	// this also works:
	//ntHeaders->OptionalHeader.SizeOfImage = P2ALIGNUP(ntHeaders->OptionalHeader.SizeOfImage + VirtualSize, sectionAlignment);

	memset( (PVOID)((UINT_PTR)pView + newSectionHeader->PointerToRawData), 0, newSectionHeader->SizeOfRawData);

	wcout	<< L"You can proceed to copy your raw section data to file offset 0x" << hex << newSectionHeader->PointerToRawData
			<< L" up to a length of 0x" << hex << VirtualSize << endl;

}

int wmain(int argc, wchar_t *argv[])
{
	if (argc < 5) {
		wcout << L"USAGE: " << argv[0] << L" <path to PE file> <section name> <VirtualSize> <Characteristics>" << endl << endl

			<< L"VirtualSize can be in decimal (ex: 5021) or in hex (ex. 0x12c)" << endl
			<< L"Characteristics can either be a hex DWORD like this: 0xC0000040 " << endl
			<< L"or the strings \"text\", \"data\" or \"rdata\" which mean: " << endl << endl

			<< L"text:  0x60000020: IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ" << endl
			<< L"data:  0xC0000040: IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE" << endl
			<< L"rdata: 0x40000040: IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ " << endl << endl

#ifdef _WIN64
			<< L"Note: This program works on 64 bit executables only. If you want to work with 32 bit executables "
			<< L"you need a 32 bit version of this program."
#else
			<< L"Note: This program works on 32 bit executables only. If you want to work with 64 bit executables "
			<< L"you need a 64 bit version of this program."
#endif
			<< endl;


		return EXIT_SUCCESS;
	}

	// Parsing parameters:

	PWSTR path = argv[1];
	PWSTR wc_section_name = argv[2];
	CHAR str_section_name[9] = { 0 };

	WideCharToMultiByte(CP_UTF8, 0, wc_section_name, -1, str_section_name, 9, NULL, NULL);

	DWORD VirtualSize = 0;
	DWORD Characteristics = 0;
	PWSTR str_VirtualSize = argv[3];
	PWSTR str_Characteristics = argv[4];

	if (str_VirtualSize[0] == L'0' && str_VirtualSize[1] == L'x') {
		VirtualSize = wcstoul(str_VirtualSize + 2, 0, 16);
	}
	else {
		VirtualSize = wcstoul(str_VirtualSize, 0, 10);
	}

	if (str_Characteristics[0] == L'0' && str_Characteristics[1] == L'x') {
		Characteristics = wcstoul(str_Characteristics + 2, 0, 16);
	}
	else {
		if (wcscmp(str_Characteristics, L"text") == 0) {
			Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;
		}
		else if (wcscmp(str_Characteristics, L"data") == 0) {
			Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
		}
		else if (wcscmp(str_Characteristics, L"rdata") == 0) {
			Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
		}
	}

	// opening file
	hFile = CreateFile(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		wcout << L"Cannot open file (0x" << hex << GetLastError() << L")" << endl;
		return EXIT_SUCCESS;
	}

	dwFileSizeLow = GetFileSize(hFile, &dwFileSizeHigh);
	if (dwFileSizeHigh != NULL) {
		CloseHandle(hFile);
		wcout << L"Big files not supported." << endl;
	}
	wcout << L"File size in bytes: " << dwFileSizeLow << endl;

	// Mapping the file read-only
	MapFileReadOnly();


	if (pView == NULL) {
		wcout << L"Error in MapFileReadOnly (" << GetLastError() << L")" << endl;
		return EXIT_FAILURE;
	}

	// Checking the file.

	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)pView;
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		wcout << L"Invalid PE file" << endl;
		UnmapAndExit(EXIT_SUCCESS);
	}

	PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((UINT_PTR)pView + dosHeader->e_lfanew);


#ifdef _WIN64
	#define MACHINE IMAGE_FILE_MACHINE_AMD64
#else
	#define MACHINE IMAGE_FILE_MACHINE_I386
#endif

	if (ntHeaders->Signature != IMAGE_NT_SIGNATURE || ntHeaders->FileHeader.Machine != MACHINE) {
		wcout << L"Invalid PE file" << endl;
		UnmapAndExit(EXIT_SUCCESS);
	}

	// Extracting data for some global variables that will be used later.
	numberOfSections = ntHeaders->FileHeader.NumberOfSections;
	sectionAlignment = ntHeaders->OptionalHeader.SectionAlignment;
	fileAlignment = ntHeaders->OptionalHeader.FileAlignment;

	PIMAGE_FILE_HEADER fileHeader = &(ntHeaders->FileHeader);

	WORD sizeOfOptionalHeader = ntHeaders->FileHeader.SizeOfOptionalHeader;
	PIMAGE_SECTION_HEADER firstSectionHeader = (PIMAGE_SECTION_HEADER)( ((UINT_PTR)fileHeader) + sizeof(IMAGE_FILE_HEADER) + sizeOfOptionalHeader );
	PIMAGE_SECTION_HEADER newSectionHeader = &firstSectionHeader[numberOfSections]; // Right after last section header.
	PBYTE firstByteOfSectionData = (PBYTE)( ((DWORD)firstSectionHeader->PointerToRawData) + (UINT_PTR)pView );

	size_t available_space = ((UINT_PTR)firstByteOfSectionData) - ((UINT_PTR)newSectionHeader);
	if (available_space < sizeof(IMAGE_SECTION_HEADER)) {
		wcout	<< L"There is no room for the new section header. Functionality to make room is not yet implemented so "
				<< L"the program will abort. No change has been made to the file." <<endl;
		UnmapAndExit(EXIT_SUCCESS);
	}

	// Unmaping the file.
	// Since file mappings are fixed size, we need to close this read-only one and create a bigger RW one to 
	// be able to add the section header and expand the size of the file.
	Unmap();

	DWORD newSize = P2ALIGNUP(dwFileSizeLow + VirtualSize, fileAlignment);
	MapFileRWNewSize(newSize);

	if (pView == NULL) {
		wcout << L"Error in MapFileRWNewSize (" << GetLastError() << L")" << endl;
		return EXIT_FAILURE;
	}

	// Appending section header.
	AppendNewSectionHeader(str_section_name, VirtualSize, Characteristics);
	
	

	wcout << L"New file size in bytes: " << newSize << endl << L"Operation completed successfully." << endl;
	UnmapAndExit(EXIT_SUCCESS);

	return 0;
}

