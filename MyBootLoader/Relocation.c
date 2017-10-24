#include "Relocation.h"

#include <Library/UefiLib.h>


static VOID RelocateRelaRaw(
        EFI_PHYSICAL_ADDRESS ImageBase,
        UINTN RelaOffset,
        UINTN RelaCount)
{
    Elf64_Rela *Rela = (Elf64_Rela*)(ImageBase + RelaOffset);
    for (UINTN i = 0; i < RelaCount; i++) {
        switch (ELF64_R_TYPE(Rela[i].r_info)) {
        case R_X86_64_RELATIVE:
            {
                UINT64 Value = ImageBase + Rela[i].r_addend;
                if (i < 5) {
                    Print(L"R_X86_64_RELATIVE: Base %08lx Addend %08lx New Value %08lx\n",
                            ImageBase, Rela[i].r_addend, Value);
                } else if (i == 5) {
                    Print(L"<Omitted>\n");
                }
                if (ELF64_R_SYM(Rela[i].r_info) == 0) {
                    *(UINT64*)(ImageBase + Rela[i].r_offset) = Value;
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

static VOID RelocateRela(Elf64_Ehdr *Ehdr, Elf64_Shdr *Shdr)
{
    RelocateRelaRaw(
            (EFI_PHYSICAL_ADDRESS)Ehdr,
            Shdr->sh_offset,
            Shdr->sh_size / sizeof(Elf64_Rela));
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

VOID RelocateDynamic(EFI_PHYSICAL_ADDRESS BaseAddr, Elf64_Dyn *Dynamic)
{
    Elf64_Addr RelaOffset = 0;
    Elf64_Xword RelaEntrySize = 0;
    Elf64_Xword RelaEntryCount = 0;

    while (Dynamic->d_tag != DT_NULL) {
        switch (Dynamic->d_tag) {
        case DT_RELA: RelaOffset = Dynamic->d_un.d_ptr; break;
        case DT_RELAENT: RelaEntrySize = Dynamic->d_un.d_val; break;
        case DT_RELACOUNT: RelaEntryCount = Dynamic->d_un.d_val; break;
        }

        Dynamic++;
    }

    if (RelaOffset == 0) {
        Print(L"Not found rela dynamic\n");
        return;
    }

    Print(L"Got dynamic info: RelaOffset = %08x, EntSize = %lu, EntCnt = %lu\n",
            RelaOffset, RelaEntrySize, RelaEntryCount);

    RelocateRelaRaw(BaseAddr, RelaOffset, RelaEntryCount);
}
