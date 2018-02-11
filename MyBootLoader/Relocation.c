#include "Relocation.h"

#include <Library/UefiLib.h>

static VOID RelocateRelaRaw(
        EFI_PHYSICAL_ADDRESS ImageBaseAddress,
        EFI_PHYSICAL_ADDRESS LoadAddress,
        UINTN RelaAddress,
        UINTN RelaCount)
{
    Elf64_Rela *Rela = (Elf64_Rela*)(RelaAddress + LoadAddress);

    Print(L"Image Base %08lx, Load Addr, Rela %08lx\n",
            ImageBaseAddress, LoadAddress, Rela);

    for (UINTN i = 0; i < RelaCount; i++) {
        switch (ELF64_R_TYPE(Rela[i].r_info)) {
        case R_X86_64_RELATIVE:
            {
                UINT64 Value = LoadAddress + Rela[i].r_addend;
                if (i < 5) {
                    Print(L"R_X86_64_RELATIVE: Addend %08lx, New Value %08lx\n",
                            Rela[i].r_addend, Value);
                } else if (i == 5) {
                    Print(L"<Omitted>\n");
                }
                if (ELF64_R_SYM(Rela[i].r_info) == 0) {
                    *(UINT64*)(LoadAddress + Rela[i].r_offset) = Value;
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

VOID RelocateDynamic(
        EFI_PHYSICAL_ADDRESS ImageBaseAddress,
        EFI_PHYSICAL_ADDRESS LoadAddress,
        Elf64_Dyn *Dynamic)
{
    Elf64_Addr RelaAddress = 0;
    Elf64_Xword RelaEntrySize = 0;
    Elf64_Xword RelaEntryCount = 0;

    while (Dynamic->d_tag != DT_NULL) {
        switch (Dynamic->d_tag) {
        case DT_RELA: RelaAddress = Dynamic->d_un.d_ptr; break;
        case DT_RELAENT: RelaEntrySize = Dynamic->d_un.d_val; break;
        case DT_RELACOUNT: RelaEntryCount = Dynamic->d_un.d_val; break;
        }

        Dynamic++;
    }

    if (RelaAddress == 0) {
        Print(L"Not found rela dynamic\n");
        return;
    }

    Print(L"Got dynamic info: RelaAddress = %08x, EntSize = %lu, EntCnt = %lu\n",
            RelaAddress, RelaEntrySize, RelaEntryCount);

    RelocateRelaRaw(ImageBaseAddress,
                    LoadAddress,
                    RelaAddress,
                    RelaEntryCount);
}

VOID AdjustSectionAddr(Elf64_Shdr *Shdr, Elf64_Phdr *LoadedSegment, EFI_PHYSICAL_ADDRESS LoadedTo)
{
    UINTN SectionOffsetFromSegment = Shdr->sh_offset - LoadedSegment->p_offset;
    Shdr->sh_addr = LoadedSegment->p_vaddr + SectionOffsetFromSegment;
}

VOID AdjustSectionsAddr(Elf64_Ehdr *Ehdr, Elf64_Phdr *LoadedSegment, EFI_PHYSICAL_ADDRESS LoadedTo)
{
    Elf64_Shdr* Shdr = ELF64_GET_SHDR(Ehdr);
    for (UINTN i = 0; i < Ehdr->e_shnum; ++i) {
        if (LoadedSegment->p_offset <= Shdr[i].sh_offset + Shdr[i].sh_size ||
                LoadedSegment->p_offset + LoadedSegment->p_filesz <= Shdr[i].sh_offset)
        {
            continue;
        }
        AdjustSectionAddr(&Shdr[i], LoadedSegment, LoadedTo);
    }
}
