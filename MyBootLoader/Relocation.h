#ifndef RELOCATION_H_
#define RELOCATION_H_

#include <Uefi/UefiBaseType.h>
#include "elf.h"

VOID RelocateAll(Elf64_Ehdr *Ehdr);
VOID RelocateDynamic(EFI_PHYSICAL_ADDRESS ImageBaseAddress,
                     EFI_PHYSICAL_ADDRESS LoadAddress,
                     Elf64_Dyn *Dynamic);
VOID AdjustSectionsAddr(Elf64_Ehdr *Ehdr, Elf64_Phdr *LoadedSegment, EFI_PHYSICAL_ADDRESS LoadedTo);

#endif // RELOCATION_H_
