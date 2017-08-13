#include "Graphics.h"

#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/GraphicsOutput.h>

#include "bootparam.h"

EFI_STATUS GetGraphicMode(
    IN EFI_HANDLE ImageHandle,
    OUT struct GraphicMode *Mode
    )
{
    EFI_STATUS Status;
    UINTN NumGopHandles = 0;
    EFI_HANDLE *GopHandles = NULL;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;

    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        &NumGopHandles,
        &GopHandles);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = gBS->OpenProtocol(
        GopHandles[0],
        &gEfiGraphicsOutputProtocolGuid,
        (VOID**)&Gop,
        ImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    FreePool(GopHandles);

    // Find largest resolution mode
    UINT32 GraphicModeInfoIndex = 0;
    UINT64 Resolution = 0;
    UINTN SizeOfInfo;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *GraphicModeInfo;

    for (UINT32 i = 0; i < Gop->Mode->MaxMode; i++) {
        Status = Gop->QueryMode(Gop, i, &SizeOfInfo, &GraphicModeInfo);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        UINT32 H = GraphicModeInfo->HorizontalResolution;
        UINT32 V = GraphicModeInfo->VerticalResolution;
        FreePool(GraphicModeInfo);

        // Allow modes 16:9 or 4:3 only
        if (!(H * 9 == V * 16 || H * 3 == V * 4)) {
            continue;
        }

        if (Resolution < H * V) {
            GraphicModeInfoIndex = i;
            Resolution = H * V;
        }
    }

    Print(L"Current Mode %u, %ux%u\n", Gop->Mode->Mode,
            Gop->Mode->Info->HorizontalResolution,
            Gop->Mode->Info->VerticalResolution);
    Print(L"Max Mode %u\n", GraphicModeInfoIndex);

    Status = Gop->SetMode(Gop, GraphicModeInfoIndex);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Mode->frame_buffer_base = Gop->Mode->FrameBufferBase;
    Mode->frame_buffer_size = Gop->Mode->FrameBufferSize;
    Mode->horizontal_resolution = Gop->Mode->Info->HorizontalResolution;
    Mode->vertical_resolution = Gop->Mode->Info->VerticalResolution;
    switch (Gop->Mode->Info->PixelFormat) {
    case PixelRedGreenBlueReserved8BitPerColor:
        Mode->pixel_format = kPixelRedGreenBlueReserved8BitPerColor;
        break;
    case PixelBlueGreenRedReserved8BitPerColor:
        Mode->pixel_format = kPixelBlueGreenRedReserved8BitPerColor;
        break;
    case PixelBitMask:
        Mode->pixel_format = kPixelBitMask;
        break;
    case PixelBltOnly:
        Mode->pixel_format = kPixelBltOnly;
        break;
    case PixelFormatMax:
        Mode->pixel_format = kPixelFormatMax;
        break;
    }
    Mode->pixel_information.RedMask = Gop->Mode->Info->PixelInformation.RedMask;
    Mode->pixel_information.GreenMask = Gop->Mode->Info->PixelInformation.GreenMask;
    Mode->pixel_information.BlueMask = Gop->Mode->Info->PixelInformation.BlueMask;
    Mode->pixel_information.ReservedMask = Gop->Mode->Info->PixelInformation.ReservedMask;
    Mode->pixels_per_scan_line = Gop->Mode->Info->PixelsPerScanLine;

    return EFI_SUCCESS;
}
