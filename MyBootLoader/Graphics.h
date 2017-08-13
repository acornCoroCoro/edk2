#ifndef GRAPHICS_H_
#define GRAPHICS_H_

#include <Uefi/UefiBaseType.h>

struct GraphicMode;

EFI_STATUS GetGraphicMode(
    IN EFI_HANDLE ImageHandle,
    OUT struct GraphicMode *Mode
    );

#endif // GRAPHICS_H_
