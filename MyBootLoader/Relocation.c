#include "Relocation.h"

#include <Library/UefiLib.h>


static VOID RelocateRela(Elf64_Ehdr *Ehdr, Elf64_Shdr *Shdr)
{
    Elf64_Rela *Rela = (Elf64_Rela*)((char *)Ehdr + Shdr->sh_offset);
    for (UINTN i = 0; i < Shdr->sh_size / sizeof(Elf64_Rela); i++) {
        switch (ELF64_R_TYPE(Rela[i].r_info)) {
        case R_X86_64_RELATIVE:
            {
                UINT64 Value = (UINT64)Ehdr + Rela[i].r_addend;
                Print(L"R_X86_64_RELATIVE: Base %08lx Addend %08lx New Value %08lx\n",
                        (UINT64)Ehdr, Rela[i].r_addend, Value);
                if (ELF64_R_SYM(Rela[i].r_info) == 0) {
                    *(UINT64*)((UINT64)Ehdr + Rela[i].r_offset) = Value;
                } else {
                    Print(L"not implemented symbol type\n");
                }
            }
            break;
        default:
            Print(L"unknown relocation type %d\n", ELF64_R_TYPE(Rela[i].r_info));
            return;
        }
    }
}

static VOID RelocateRel(Elf64_Ehdr *Ehdr, Elf64_Shdr *Shdr)
{
    for (UINTN i = 0; i < Shdr->sh_size / sizeof(Elf64_Rel); i++) {
    }
}

VOID RelocateAll(Elf64_Ehdr *Ehdr)
{
    Elf64_Shdr *Shdr = ELF64_GET_SHDR(Ehdr);
    for (UINTN i = 0; i < Ehdr->e_shnum; i++) {
        if (Shdr[i].sh_type == SHT_RELA) {
            RelocateRela(Ehdr, Shdr + i);
        } else if (Shdr[i].sh_type == SHT_REL) {
            RelocateRel(Ehdr, Shdr + i);
        }
    }
}
