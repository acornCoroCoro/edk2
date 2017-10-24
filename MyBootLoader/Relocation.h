#ifndef RELOCATION_H_
#define RELOCATION_H_

#include <Uefi/UefiBaseType.h>
#include "elf.h"

VOID RelocateAll(Elf64_Ehdr *Ehdr);
VOID RelocateDynamic(EFI_PHYSICAL_ADDRESS BaseAddr, Elf64_Dyn *Dynamic);

#endif // RELOCATION_H_
