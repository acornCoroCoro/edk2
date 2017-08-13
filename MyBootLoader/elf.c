#include "elf.h"
#include  <Library/UefiLib.h>

Elf32_Shdr* Elf32_FindSection(Elf32_Ehdr* ehdr, const char* secname)
{
    Elf32_Shdr* shdr = ELF32_GET_SHDR(ehdr);
    const char* strtab = ELF32_GET_SHSTRTAB(ehdr);
    int i;
    for (i = 0; i < ehdr->e_shnum; i++) {
        if (AsciiStrCmp(secname, strtab + shdr[i].sh_name) == 0) {
            return &shdr[i];
        }
    }
    return 0;
}

Elf64_Shdr* Elf64_FindSection(Elf64_Ehdr* ehdr, const char* secname)
{
    Elf64_Shdr* shdr = ELF64_GET_SHDR(ehdr);
    const char* strtab = ELF64_GET_SHSTRTAB(ehdr);
    int i;
    for (i = 0; i < ehdr->e_shnum; i++) {
        if (AsciiStrCmp(secname, strtab + shdr[i].sh_name) == 0) {
            return &shdr[i];
        }
    }
    return 0;
}
