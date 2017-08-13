#include  <Uefi.h>
#include  <Uefi/UefiSpec.h>
#include  <Library/UefiLib.h>
#include  <Library/PrintLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include  <Library/UefiRuntimeServicesTableLib.h>
#include  <Library/DevicePathLib.h>
#include  <Library/MemoryAllocationLib.h>
#include  <Library/BaseMemoryLib.h>
#include  <Protocol/LoadFile.h>
#include  <Protocol/SimpleFileSystem.h>

#include "Console.h"
#include "File.h"
#include "MemoryMap.h"
#include "Relocation.h"
#include "Graphics.h"

#include "bootparam.h"
#include "elf.h"

EFI_STATUS EFIAPI UefiMain(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
    )
{
    EFI_STATUS Status;

    // Set console size
    Status = gST->ConOut->SetMode(gST->ConOut, FindLargestConMode());
    if (EFI_ERROR(Status)) {
        Print(L"Could not set console mode: %r\n", Status);
        return Status;
    }

    // Get graphic mode
    struct GraphicMode GraphicMode;
    Status = GetGraphicMode(ImageHandle, &GraphicMode);
    if (EFI_ERROR(Status)) {
        Print(L"Could not get graphic mode: %r\n", Status);
        return Status;
    }

    Print(L"Successfully got graphic mode: %p\n", &GraphicMode);

    // Get handle of root directory
    EFI_FILE_PROTOCOL *RootDir = NULL;
    Status = OpenFileProtocolForThisAppRootDir(ImageHandle, &RootDir);
    if (EFI_ERROR(Status)) {
        Print(L"Could not open file protocol for the root: %r\n", Status);
        return Status;
    }

    Print(L"Successfully opened the root dir: %p\n", RootDir);

    // Open kernel file
    UINTN KernelFileSize;
    EFI_FILE_PROTOCOL *KernelFile;
    Status = OpenFileForRead(RootDir, L"\\kernel.elf", &KernelFile, &KernelFileSize);
    if (EFI_ERROR(Status)) {
        Print(L"Could not open '\\kernel.elf': %r\n", Status);
        while (1);
        return Status;
    }
    Print(L"Kernel file size is %lu = 0x%lx\n", KernelFileSize, KernelFileSize);

    // Allocate buffer for the kernel file
    EFI_PHYSICAL_ADDRESS KernelFileAddr = 0x00100000lu;
    Status = gBS->AllocatePages(
        AllocateAddress,
        EfiLoaderData,
        (KernelFileSize + 4095) / 4096,
        &KernelFileAddr);
    if (EFI_ERROR(Status)) {
        Print(L"Could not allocate pages at %08lx: %r\n", KernelFileAddr, Status);
        while (1);
        return Status;
    }

    // Read the kernel file
    Status = KernelFile->Read(KernelFile, &KernelFileSize, (VOID*)KernelFileAddr);
    if (EFI_ERROR(Status)) {
        Print(L"Could not read kernel file: %r\n", Status);
        while (1);
        return Status;
    }

    Elf64_Ehdr *Ehdr = (Elf64_Ehdr*)KernelFileAddr;
    if (Ehdr->e_ident[0] != 0x7f || AsciiStrnCmp((CHAR8*)Ehdr->e_ident + 1, "ELF", 3) != 0) {
        Print(L"Kernel file is not elf.\n");
        return EFI_LOAD_ERROR;
    }

    typedef void (CtorType)(void);
    Elf64_Shdr *CtorsSection = Elf64_FindSection(Ehdr, ".ctors");
    if (CtorsSection == NULL)
    {
        Print(L"Could not find .ctors\n");
    }
    else
    {
        for (UINT64 *Ctor = (UINT64*)CtorsSection->sh_addr;
                Ctor < (UINT64*)(CtorsSection->sh_addr + CtorsSection->sh_size);
                ++Ctor)
        {
            Print(L"Calling a ctor: %08lx\n", Ctor);
            CtorType *F = (CtorType*)*Ctor;
            F();
        }
    }

    //RelocateAll(Ehdr);

    Print(L"Successfully loaded kernel: Buf=0x%08lx Siz=0x%lx\n",
        KernelFileAddr, KernelFileSize);

    typedef unsigned long (EntryPointType)(struct BootParam *param);
    EntryPointType *EntryPoint = (EntryPointType*)Ehdr->e_entry;
    Print(L"Entry point: %08p\n", EntryPoint);

    // Get memory map
    struct MemoryMap MemoryMap;
    Status = AllocateMemoryMap(&MemoryMap, 4096);
    if (EFI_ERROR(Status)) {
        Print(L"Could not allocate memory map: %r\n", Status);
        return Status;
    }

    Status = GetMemoryMap(&MemoryMap);
    if (EFI_ERROR(Status)) {
        Print(L"Could not get current memory map: %r\n", Status);
        return Status;
    }

    Print(L"Successfully got memory map: %08lx %lu dscsize=%lu ver=%lu key=%lu\n",
        (EFI_PHYSICAL_ADDRESS)MemoryMap.Buffer, MemoryMap.MapSize,
        MemoryMap.DescriptorSize, MemoryMap.DescriptorVersion, MemoryMap.MapKey);

    //Pause();

    // Exit boot services
    Status = gBS->ExitBootServices(ImageHandle, MemoryMap.MapKey);
    if (EFI_ERROR(Status)) {
        Status = GetMemoryMap(&MemoryMap);
        if (EFI_ERROR(Status)) {
            Print(L"Could not get memory map: %r\n", Status);
            while (1);
            return Status;
        }
        Status = gBS->ExitBootServices(ImageHandle, MemoryMap.MapKey);
        if (EFI_ERROR(Status)) {
            Print(L"Could not exit boot service: %r\n", Status);
            while (1);
            return Status;
        }
    }

    struct BootParam BootParam;
    BootParam.efi_system_table = SystemTable;
    BootParam.memory_map = MemoryMap.Buffer;
    BootParam.memory_map_size = MemoryMap.MapSize;
    BootParam.memory_descriptor_size = MemoryMap.DescriptorSize;
    BootParam.graphic_mode = &GraphicMode;

    //while (1) __asm__("hlt");
    // Jump to the kernel
    EntryPoint(&BootParam);

    while (1) __asm__("hlt");

    return EFI_SUCCESS;
}
