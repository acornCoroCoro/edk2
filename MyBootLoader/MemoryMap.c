#include "MemoryMap.h"

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>

const CHAR16 *GetMemoryTypeUnicode(EFI_MEMORY_TYPE Type) {
    switch (Type) {
        case EfiReservedMemoryType: return L"EfiReservedMemoryType";
        case EfiLoaderCode: return L"EfiLoaderCode";
        case EfiLoaderData: return L"EfiLoaderData";
        case EfiBootServicesCode: return L"EfiBootServicesCode";
        case EfiBootServicesData: return L"EfiBootServicesData";
        case EfiRuntimeServicesCode: return L"EfiRuntimeServicesCode";
        case EfiRuntimeServicesData: return L"EfiRuntimeServicesData";
        case EfiConventionalMemory: return L"EfiConventionalMemory";
        case EfiUnusableMemory: return L"EfiUnusableMemory";
        case EfiACPIReclaimMemory: return L"EfiACPIReclaimMemory";
        case EfiACPIMemoryNVS: return L"EfiACPIMemoryNVS";
        case EfiMemoryMappedIO: return L"EfiMemoryMappedIO";
        case EfiMemoryMappedIOPortSpace: return L"EfiMemoryMappedIOPortSpace";
        case EfiPalCode: return L"EfiPalCode";
        case EfiPersistentMemory: return L"EfiPersistentMemory";
        case EfiMaxMemoryType: return L"EfiMaxMemoryType";
        default: return L"InvalidMemoryType";
    }
}

EFI_STATUS AllocateMemoryMap(
    IN struct MemoryMap *Map,
    IN UINTN BufferSize
    )
{
    Map->BufferSize = BufferSize;

    return gBS->AllocatePool(EfiLoaderData, BufferSize, &Map->Buffer);
}

EFI_STATUS GetMemoryMap(
    IN struct MemoryMap *Map
    )
{
    EFI_STATUS Status;

    if (Map->BufferSize == 0 && Map->Buffer == NULL) {
        Status = AllocateMemoryMap(Map, 4096);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    for (;;) {
        Map->MapSize = Map->BufferSize;
        Status = gBS->GetMemoryMap(
            &Map->MapSize,
            (EFI_MEMORY_DESCRIPTOR*)Map->Buffer,
            &Map->MapKey,
            &Map->DescriptorSize,
            &Map->DescriptorVersion);

        if (Status == EFI_BUFFER_TOO_SMALL) {
            const UINTN NewBufferSize = (Map->MapSize + 4095) & ~(UINTN)4095;
            Print(L"Re-allocating buf. New size = %lu: %r\n", NewBufferSize, Status);

            Status = gBS->FreePool(Map->Buffer);
            if (EFI_ERROR(Status)) {
                return Status;
            }
            Status = AllocateMemoryMap(Map, NewBufferSize);
            if (EFI_ERROR(Status)) {
                return Status;
            }
            continue;
        }
        return Status;
    }
}

EFI_STATUS SaveMemoryMap(
    IN struct MemoryMap *Map,
    IN EFI_FILE_PROTOCOL *File
    )
{
    CHAR8 Buffer[256];
    EFI_STATUS Status;
    UINTN Len;

    CHAR8 *Header =
        "| Index | Type     | PhysicalStart | VirtualStart  | NumberOfPages | Attribute |\n"
        "|-------|----------|---------------|---------------|---------------|-----------|\n";
    Len = AsciiStrLen(Header);
    Status = File->Write(File, &Len, Header);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EFI_PHYSICAL_ADDRESS Iter;
    int i;
    for (Iter = (EFI_PHYSICAL_ADDRESS)Map->Buffer, i = 0;
            Iter < (EFI_PHYSICAL_ADDRESS)Map->Buffer + Map->MapSize;
            Iter += Map->DescriptorSize, i++) {
        EFI_MEMORY_DESCRIPTOR *Desc = (EFI_MEMORY_DESCRIPTOR*)Iter;
        Len = AsciiSPrint(Buffer, sizeof(Buffer),
            "| %2u | %x %-26ls | %08lx | %08lx | %4lx | %2ls %5lx |\n",
            i,
            Desc->Type, GetMemoryTypeUnicode(Desc->Type),
            Desc->PhysicalStart,
            Desc->VirtualStart,
            Desc->NumberOfPages,
            (Desc->Attribute & EFI_MEMORY_RUNTIME) ? L"RT" : L"",
            Desc->Attribute & 0xffffflu);
        Status = File->Write(File, &Len, Buffer);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    return EFI_SUCCESS;
}

VOID RemapVirtual(
    IN struct MemoryMap *Map
    )
{
    //EFI_VIRTUAL_ADDRESS OSMemoryAddress = 0xf0000000lu;
    //EFI_VIRTUAL_ADDRESS OSMemoryAddress = 0x00000000lu;

    EFI_PHYSICAL_ADDRESS Iter;
    for (Iter = (EFI_PHYSICAL_ADDRESS)Map->Buffer;
            Iter < (EFI_PHYSICAL_ADDRESS)Map->Buffer + Map->MapSize;
            Iter += Map->DescriptorSize) {
        EFI_MEMORY_DESCRIPTOR *Desc = (EFI_MEMORY_DESCRIPTOR*)Iter;

        Desc->VirtualStart = Desc->PhysicalStart;
        //if (OSMemoryAddress < 0x100000000lu - Desc->NumberOfPages * 4096) {
        //if (OSMemoryAddress < 0x03000000lu - Desc->NumberOfPages * 4096) {
        //    switch (Desc->Type) {
        //    case EfiBootServicesCode:
        //    case EfiBootServicesData:
        //    case EfiConventionalMemory:
        //        Desc->VirtualStart = OSMemoryAddress;
        //        OSMemoryAddress += Desc->NumberOfPages * 4096;
        //        break;
        //    default:
        //        //Desc->VirtualStart = Desc->PhysicalStart;
        //        Desc->VirtualStart = 0x03000000lu + Desc->PhysicalStart;
        //    }
        //} else {
        //    Desc->VirtualStart = 0x03000000lu + Desc->PhysicalStart;
        //}
    }
}
