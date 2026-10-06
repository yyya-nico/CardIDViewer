#pragma once
#include <windows.h>
#include <winscard.h>
#ifdef __cplusplus
extern "C" {
#endif
extern LONG (WINAPI *cvEstablish)(DWORD,LPCVOID,LPCVOID,LPSCARDCONTEXT);
extern LONG (WINAPI *cvRelease)(SCARDCONTEXT);
extern LONG (WINAPI *cvList)(SCARDCONTEXT,LPCWSTR,LPWSTR,LPDWORD);
extern LONG (WINAPI *cvConnect)(SCARDCONTEXT,LPCWSTR,DWORD,DWORD,LPSCARDHANDLE,LPDWORD);
extern LONG (WINAPI *cvDisconnect)(SCARDHANDLE,DWORD);
extern LONG (WINAPI *cvTransmit)(SCARDHANDLE,LPCSCARD_IO_REQUEST,LPCBYTE,DWORD,LPSCARD_IO_REQUEST,LPBYTE,LPDWORD);
extern const SCARD_IO_REQUEST cvPci;
#ifdef __cplusplus
}
#endif
#undef SCardListReaders
#undef SCardConnect
#undef SCARD_PCI_T1
#define SCardEstablishContext cvEstablish
#define SCardReleaseContext cvRelease
#define SCardListReaders cvList
#define SCardConnect cvConnect
#define SCardDisconnect cvDisconnect
#define SCardTransmit cvTransmit
#define SCARD_PCI_T1 (&cvPci)
