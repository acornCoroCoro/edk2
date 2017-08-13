#include "Console.h"

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>

UINTN FindLargestConMode() {
    UINTN Columns = 0, Rows = 0, Mode = 0;

    for (UINTN i = 0; i < gST->ConOut->Mode->MaxMode; i++) {
        UINTN C, R;
        EFI_STATUS Status = gST->ConOut->QueryMode(gST->ConOut, i, &C, &R);
        if (!EFI_ERROR(Status)) {
            if (C >= Columns || R >= Rows) {
                Columns = C;
                Rows = R;
                Mode = i;
            }
        }
    }

    return Mode;
}

EFI_STATUS Pause() {
    EFI_STATUS Status;
    EFI_INPUT_KEY Key;
    UINTN Index;

    Print(L"Press any key> ");

    Status = gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
    if (EFI_ERROR(Status)) {
        Print(L"Could not wait for key %r\n", Status);
        return Status;
    }

    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    if (EFI_ERROR(Status)) {
        Print(L"Could not read key stroke %r\n", Status);
        return Status;
    }

    if (Key.ScanCode == 0x00 && Key.UnicodeChar == 0x000d) {
        Print(L"CR\n");
    } else if (Key.ScanCode == 0x00 && Key.UnicodeChar == 0x000a) {
        Print(L"LF\n");
    } else {
        Print(L"%lc\n", Key.UnicodeChar);
    }

    return EFI_SUCCESS;
}
