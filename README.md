# addscn
Adds an empty section of a specified VirtualSize and Characteristics to a PE file. The section's content will be placed right at the end of the file and
will be initially set to zero. The section will be loaded in memory at the next possible RVA after the former last section.
After the operation, the program will output the file offset of the section in the file and RVA where it will be placed in memory.
You can then procceed to copy whatever raw data you want to this file offset up to a length of VirtualSize.

The section header will be appended at the end of the section header table. If there is no room for it, the program will terminate and no
modifications will be made to the target file. In a future version it may make room for it.
## 32 bit and 64 bit files

This programs works on both 32 bit pe files (executables, dlls...) and 64 bit pe files, however, you need to compile the code for
the corresponding target architecture. If you compile this code to a 64 bit executable it will work ONLY on 64 bit pe files and
and not on 32 bit files. The same happens for 32 bit files.

## Usage
```
USAGE: addscn.exe <path to PE file> <section name> <VirtualSize> <Characteristics>

VirtualSize can be in decimal (ex: 5021) or in hex (ex. 0x12c)
Characteristics can either be a hex DWORD like this: 0xC0000040
or the strings "text", "data" or "rdata" which mean:

text:  0x60000020: IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ
data:  0xC0000040: IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE
rdata: 0x40000040: IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ
```

Example:
```
C:\>addscn.exe target_file.exe .TEST 0x231 rdata
File size in bytes: 449024
You can proceed to copy your raw section data to file offset 0x6da00 up to a length of 0x231
The section will be mapped at RVA 0x73000
New file size in bytes: 6de00
Operation completed successfully.

```

## Issue with certificate table

As specified in the [Microsoft PE File Format Specification](https://msdn.microsoft.com/en-us/library/windows/desktop/ms680547(v=vs.85).aspx#other_contents_of_the_file),
```
[...] attribute certificate and debug information must be placed at the very end of an image file, 
with the attribute certificate table immediately preceding the debug section [...]
```

This program places the new section raw data at the end of the file, AFTER the certificate table, thus breaking the rule above
which renders the pe file unsuable. You must avoid using this program on files with a certificate table OR eliminate the certificate
table from the file before using this program.

There is no point in putting the new section before the certificate table because the digital signature contained in the certificate
would still be broken.
