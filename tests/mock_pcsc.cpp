#include <windows.h>
#include <winscard.h>
#include <cstring>
#pragma comment(linker,"/export:SCardEstablishContext=mockEstablish")
#pragma comment(linker,"/export:SCardReleaseContext=mockRelease")
#pragma comment(linker,"/export:SCardListReadersW=mockList")
#pragma comment(linker,"/export:SCardConnectW=mockConnect")
#pragma comment(linker,"/export:SCardDisconnect=mockDisconnect")
#pragma comment(linker,"/export:SCardTransmit=mockTransmit")
extern "C" LONG WINAPI mockEstablish(DWORD,LPCVOID,LPCVOID,LPSCARDCONTEXT c){*c=1;return 0;}
extern "C" LONG WINAPI mockRelease(SCARDCONTEXT){return 0;}
extern "C" LONG WINAPI mockList(SCARDCONTEXT,LPCWSTR,LPWSTR b,LPDWORD n){const wchar_t s[]=L"Mock reader\0";if(!b){*n=_countof(s);return 0;}if(*n<_countof(s))return SCARD_E_INSUFFICIENT_BUFFER;memcpy(b,s,sizeof(s));*n=_countof(s);return 0;}
extern "C" LONG WINAPI mockConnect(SCARDCONTEXT,LPCWSTR name,DWORD,DWORD,LPSCARDHANDLE h,LPDWORD p){if(wcscmp(name,L"Mock reader"))return SCARD_E_UNKNOWN_READER;*h=1;*p=SCARD_PROTOCOL_T1;return 0;}
extern "C" LONG WINAPI mockDisconnect(SCARDHANDLE,DWORD){return 0;}
extern "C" LONG WINAPI mockTransmit(SCARDHANDLE,LPCSCARD_IO_REQUEST,LPCBYTE s,DWORD,LPSCARD_IO_REQUEST,LPBYTE b,LPDWORD n){if(*n<57)return SCARD_E_INSUFFICIENT_BUFFER;memset(b,0,57);if(s[1]==0x30){b[4]=0x21;*n=57;}else if(s[1]==0x32){b[6]=2;b[7]=0x42;b[8]=1;b[9]=0x40;b[14]=1;b[15]=0x30;b[16]=0x39;b[17]=0x42;b[18]=1;memset(b+19,0xFF,8);b[27]=0x90;*n=29;}else return SCARD_E_INVALID_PARAMETER;return 0;}

