#ifndef FILE_H_
#define FILE_H_

#include <Uefi/UefiBaseType.h>
#include <Protocol/SimpleFileSystem.h>

/** Open a simple file system protocol.
 *
 * @param ImageHandle The firmware allocated handle for the UEFI image.
 * @param SimpleFileSystem A pointer to the opened protocol.
 *
 * @retval EFI_SUCCESS  Successfully opened the protocol.
 * @retval Other        An error occurred.
***/
EFI_STATUS OpenSimpleFileSystemProtocol(
    IN EFI_HANDLE ImageHandle,
    OUT EFI_SIMPLE_FILE_SYSTEM_PROTOCOL **SimpleFileSystem,
    OUT EFI_HANDLE *OpenerHandle
    );

/** Open a file protocol for the root directory of this UEFI app.
 *
 * @param ImageHandle The firmware allocated handle for the UEFI image.
 * @param RootDir A pointer to the opened protocol.
 *
 * @retval EFI_SUCCESS  Successfully opened the protocol.
 * @retval Other        An error occurred.
***/
EFI_STATUS OpenFileProtocolForThisAppRootDir(
    IN EFI_HANDLE ImageHandle,
    OUT EFI_FILE_PROTOCOL **RootDir
    );

/** Open a file specified by the given path and store its size into FileSize argument.
 *
 * @param Dir  Source location. This would typically be a directory.
 * @param FilePath Path to a file to be opened.
 * @param File Handle for the file.
 * @param FileSize The number of bytes of the file.
 *
 * @retval EFI_SUCCESS  Successfully opened the file.
 * @retval Other        An error occurred.
 */
EFI_STATUS OpenFileForRead(
    IN EFI_FILE_PROTOCOL *Dir,
    IN CHAR16 *FilePath,
    OUT EFI_FILE_PROTOCOL **File,
    OUT UINTN *FileSize
    );

#endif // FILE_H_
