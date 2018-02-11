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

EFI_PHYSICAL_ADDRESS AlignAddress(EFI_PHYSICAL_ADDRESS Address, UINTN Alignment)
{
    CONST EFI_PHYSICAL_ADDRESS Mask = Alignment - 1;
    return (Address + Mask) & ~Mask;
}

EFI_PHYSICAL_ADDRESS GetSegmentEndAddress(Elf64_Phdr *Phdr)
{
    return AlignAddress(Phdr->p_vaddr + Phdr->p_memsz, Phdr->p_align);
}

VOID CalcSegmentStartAndSize(Elf64_Ehdr *Ehdr,
                             EFI_PHYSICAL_ADDRESS *SegmentStartAddr,
                             UINTN *SegmentSize)
{
    UINTN i = 0;
    Elf64_Phdr *Phdr = ELF64_GET_PHDR(Ehdr);

    for (; i < Ehdr->e_phnum && Phdr[i].p_type != PT_LOAD; ++i);

    EFI_PHYSICAL_ADDRESS Start = Phdr[i].p_vaddr;
    EFI_PHYSICAL_ADDRESS End = GetSegmentEndAddress(&Phdr[i]);
    ++i;

    for (; i < Ehdr->e_phnum; ++i) {
        if (Phdr[i].p_type != PT_LOAD) continue;

        if (Start > Phdr[i].p_vaddr) {
            Start = Phdr[i].p_vaddr;
        }

        CONST EFI_PHYSICAL_ADDRESS SegEnd = GetSegmentEndAddress(&Phdr[i]);
        if (End < SegEnd) {
            End = SegEnd;
        }
    }

    *SegmentStartAddr = Start;
    *SegmentSize = End - Start;
}

VOID CopyOneSegment(Elf64_Phdr *Phdr,
                    EFI_PHYSICAL_ADDRESS ImageBaseAddr,
                    EFI_PHYSICAL_ADDRESS LoadAddr)
{
    if (Phdr->p_filesz > 0) {
        CopyMem((VOID*)(Phdr->p_vaddr + LoadAddr), // destination
                (VOID*)(ImageBaseAddr + Phdr->p_offset), // source
                Phdr->p_filesz); // length
        Print(L"Copied image block %08x -> %08x (%08x bytes)\n",
                ImageBaseAddr + Phdr->p_offset,
                Phdr->p_vaddr + LoadAddr, Phdr->p_filesz);
    }
    if (Phdr->p_memsz > Phdr->p_filesz) {
        ZeroMem((VOID*)(Phdr->p_vaddr + LoadAddr + Phdr->p_filesz),
                Phdr->p_memsz - Phdr->p_filesz);
    }
}

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
    EFI_PHYSICAL_ADDRESS KernelFileAddr = 0;
    Status = gBS->AllocatePages(
        AllocateAnyPages,
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
    if (AsciiStrnCmp((CHAR8*)Ehdr->e_ident, "\x7f" "ELF", 4) != 0) {
        Print(L"Kernel file is not elf.\n");
        return EFI_LOAD_ERROR;
    }

    // PIE or not
    BOOLEAN IsPIEKernel = FALSE;
    if (Ehdr->e_type == ET_DYN) { // PIE
        IsPIEKernel = TRUE;
    }

    // Calculate memory size for all PT_LOAD segments
    EFI_PHYSICAL_ADDRESS SegmentStartAddr;
    UINTN SegmentSize;
    CalcSegmentStartAndSize(Ehdr, &SegmentStartAddr, &SegmentSize);

    CONST UINTN MemAllocPages = (SegmentSize + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS MemAllocAddr = SegmentStartAddr;

    // Allocate memory block for all PT_LOAD segments
    Status = gBS->AllocatePages(
        IsPIEKernel ? AllocateAnyPages : AllocateAddress,
        EfiLoaderData,
        MemAllocPages,
        &MemAllocAddr);
    if (EFI_ERROR(Status)) {
        Print(L"Could not allocate pages for dynamic load: %r\n", Status);
        while (1);
        return Status;
    }

    EFI_PHYSICAL_ADDRESS LoadAddr = IsPIEKernel ? MemAllocAddr : 0;

    Print(L"Kernel is %s, Load Addr %08lx, Segment Addr %08lx - %08lx, \n",
            IsPIEKernel ? L"PIE" : L"No-PIE",
            LoadAddr, SegmentStartAddr, SegmentStartAddr + SegmentSize);

    // Copy segments and apply relocation
    Elf64_Phdr *Phdr = ELF64_GET_PHDR(Ehdr);
    for (UINTN i = 0; i < Ehdr->e_phnum; ++i) {
        Print(L"Processing a program header %lu, type = %lu\n", i, Phdr[i].p_type);
        if (Phdr[i].p_type == PT_LOAD) {
            Print(L"Program header %lu: vaddr %08x, offset %x, filesz %x, memsz %x\n",
                    i, Phdr[i].p_vaddr, Phdr[i].p_offset, Phdr[i].p_filesz, Phdr[i].p_memsz);
            CopyOneSegment(&Phdr[i], KernelFileAddr, LoadAddr);
        } else if (Phdr[i].p_type == PT_DYNAMIC) {
            Elf64_Dyn *Dynamic = (Elf64_Dyn*)(KernelFileAddr + Phdr[i].p_offset);
            RelocateDynamic(KernelFileAddr, LoadAddr, Dynamic);
        }
    }

    // Call init functions
    typedef void (CtorType)(void);
    Elf64_Shdr *CtorsSection = Elf64_FindSection(Ehdr, ".ctors");
    if (CtorsSection == NULL)
    {
        Print(L"Could not find .ctors\n");
    }
    else
    {
        EFI_PHYSICAL_ADDRESS CtorsAddr = CtorsSection->sh_addr + LoadAddr;

        for (UINT64 *Ctor = (UINT64*)CtorsAddr;
                Ctor < (UINT64*)(CtorsAddr + CtorsSection->sh_size);
                ++Ctor)
        {
            CtorType *F = (CtorType*)*Ctor;
            F();
        }
    }

    Print(L"Successfully loaded kernel\n");

    // Get entry point address
    typedef unsigned long (EntryPointType)(struct BootParam *param);
    EntryPointType *EntryPoint = (EntryPointType*)(Ehdr->e_entry + LoadAddr);
    Print(L"Entry point: %08p\n", EntryPoint);

    // Open memory map file
    EFI_FILE_PROTOCOL *MemoryMapFile;
    Status = RootDir->Open(RootDir, &MemoryMapFile, L"\\memmap",
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Could not open memory map file: %r\n", Status);
        Print(L"Ignoreing...\n");
        MemoryMapFile = NULL;
    }

    // Get memory map
    struct MemoryMap MemoryMap = {0, NULL, 0, 0, 0, 0};
    Status = GetMemoryMap(&MemoryMap);
    if (EFI_ERROR(Status)) {
        Print(L"Could not get current memory map: %r\n", Status);
        return Status;
    }

    Print(L"Successfully got memory map: %08lx %lu dscsize=%lu ver=%lu key=%lu\n",
        (EFI_PHYSICAL_ADDRESS)MemoryMap.Buffer, MemoryMap.MapSize,
        MemoryMap.DescriptorSize, MemoryMap.DescriptorVersion, MemoryMap.MapKey);

    if (MemoryMapFile) {
        Status = SaveMemoryMap(&MemoryMap, MemoryMapFile);
        if (EFI_ERROR(Status)) {
            Print(L"Could not save current memory map: %r\n", Status);
            return Status;
        }

        Status = MemoryMapFile->Close(MemoryMapFile);
        if (EFI_ERROR(Status)) {
            Print(L"Could not close memory map file: %r\n", Status);
            return Status;
        }
    }

    Pause();

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

    EntryPoint(&BootParam);

    while (1) __asm__("hlt");

    return EFI_SUCCESS;
}
