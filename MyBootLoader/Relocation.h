#ifndef RELOCATION_H_
#define RELOCATION_H_

#include <Uefi/UefiBaseType.h>
#include "elf.h"

VOID RelocateAll(Elf64_Ehdr *Ehdr);

#endif // RELOCATION_H_
