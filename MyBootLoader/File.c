#include "File.h"

#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>

EFI_STATUS OpenSimpleFileSystemProtocol(
    IN EFI_HANDLE ImageHandle,
    OUT EFI_SIMPLE_FILE_SYSTEM_PROTOCOL **SimpleFileSystem,
    OUT EFI_HANDLE *OpenerHandle
    )
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImageForThisApp;

    Status = gBS->OpenProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&LoadedImageForThisApp,
        ImageHandle, // AgentHandle: For UEFI App, this is the image handle of the app.
        NULL, // ControllerHandle: NULL if the agent doesn't follow the UEFI Driver Model.
        EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    // Handle of the device on which the image of this app lies.
    *OpenerHandle = LoadedImageForThisApp->DeviceHandle;
    Status = gBS->OpenProtocol(
        *OpenerHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)SimpleFileSystem,
        ImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = gBS->CloseProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        ImageHandle,
        NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS OpenFileProtocolForThisAppRootDir(
    IN EFI_HANDLE ImageHandle,
    OUT EFI_FILE_PROTOCOL **RootDir
    )
{
    EFI_STATUS Status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystem;
    EFI_HANDLE SimpleFileSystemOpener;

    Status = OpenSimpleFileSystemProtocol(
        ImageHandle,
        &SimpleFileSystem,
        &SimpleFileSystemOpener);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = SimpleFileSystem->OpenVolume(SimpleFileSystem, RootDir);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = gBS->CloseProtocol(
        SimpleFileSystemOpener,
        &gEfiSimpleFileSystemProtocolGuid,
        ImageHandle,
        NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS OpenFileForRead(
    IN EFI_FILE_PROTOCOL *Dir,
    IN CHAR16 *FilePath,
    OUT EFI_FILE_PROTOCOL **File,
    OUT UINTN *FileSize
    )
{
    EFI_STATUS Status;

    Status = Dir->Open(Dir, File, FilePath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    UINTN FileInfoBufferSize = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * StrLen(FilePath) + 2;
    UINT8 FileInfoBuffer[FileInfoBufferSize];
    Status = (*File)->GetInfo(*File, &gEfiFileInfoGuid, &FileInfoBufferSize, FileInfoBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"Could not get file info: %r\n", Status);
        return Status;
    }

    EFI_FILE_INFO *FileInfo = (EFI_FILE_INFO*)FileInfoBuffer;
    *FileSize = FileInfo->FileSize;
    return EFI_SUCCESS;
}
