#include "pcsc_bridge.h"
#include "b_cas_card.h"
#include "display_id.h"
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <cstdio>
LONG (WINAPI *cvEstablish)(DWORD,LPCVOID,LPCVOID,LPSCARDCONTEXT);
LONG (WINAPI *cvRelease)(SCARDCONTEXT);
LONG (WINAPI *cvList)(SCARDCONTEXT,LPCWSTR,LPWSTR,LPDWORD);
LONG (WINAPI *cvConnect)(SCARDCONTEXT,LPCWSTR,DWORD,DWORD,LPSCARDHANDLE,LPDWORD);
LONG (WINAPI *cvDisconnect)(SCARDHANDLE,DWORD);
LONG (WINAPI *cvTransmit)(SCARDHANDLE,LPCSCARD_IO_REQUEST,LPCBYTE,DWORD,LPSCARD_IO_REQUEST,LPBYTE,LPDWORD);
extern const SCARD_IO_REQUEST cvPci = {SCARD_PROTOCOL_T1,sizeof(SCARD_IO_REQUEST)};
// Preserve display metadata that libaribb25's B_CAS_ID API omits.
static decltype(cvTransmit) providerTransmit;
static decltype(cvEstablish) providerEstablish;
static decltype(cvList) providerList;
static decltype(cvConnect) providerConnect;
static thread_local LONG lastPcscError=SCARD_S_SUCCESS;
static LONG recordPcscError(LONG rc) {
    if(rc!=SCARD_S_SUCCESS) lastPcscError=rc;
    return rc;
}
static thread_local std::vector<DisplayCardId> displayIds;
static LONG WINAPI transmitAndCapture(SCARDHANDLE card,LPCSCARD_IO_REQUEST sendPci,
    LPCBYTE command,DWORD commandSize,LPSCARD_IO_REQUEST receivePci,LPBYTE response,LPDWORD responseSize) {
    const bool acquire=command && commandSize==5 && command[0]==0x90 && command[1]==0x32;
    if(acquire) displayIds.clear();
    const LONG rc=recordPcscError(providerTransmit(card,sendPci,command,commandSize,receivePci,response,responseSize));
    if(rc==SCARD_S_SUCCESS && acquire) {
        if(!responseSize || !parseDisplayCardIds(response,*responseSize,displayIds)) return SCARD_E_NOT_TRANSACTED;
    }
    return rc;
}
static HMODULE module;
static HWND window, providers, readers, output, scan, readButton, copyButton;
static std::wstring directory;
static bool busy;
static constexpr UINT Completed = WM_APP + 1;
static std::wstring error(LONG code, bool win32=false) {
    wchar_t b[80];
    swprintf_s(b,L"%s エラー: 0x%08lX",win32?L"Windows":L"PC/SC",static_cast<unsigned long>(code));
    std::wstring result=b;
    {
        wchar_t* message=nullptr;
        DWORD flags=FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS;
        DWORD length=FormatMessageW(flags,nullptr,static_cast<DWORD>(code),0,
                                    reinterpret_cast<LPWSTR>(&message),0,nullptr);
        if(!length && module && !win32) {
            length=FormatMessageW(flags|FORMAT_MESSAGE_FROM_HMODULE,module,static_cast<DWORD>(code),0,
                                 reinterpret_cast<LPWSTR>(&message),0,nullptr);
        }
        if(length && message) {
            std::wstring description(message,length);
            while(!description.empty() && iswspace(description.back())) description.pop_back();
            result+=L" — "+description;
        } else result+=L" — 説明メッセージを取得できません。";
        if(message) LocalFree(message);
    }
    return result;
}
static std::wstring communicationError() {
    return lastPcscError==SCARD_S_SUCCESS?L"":L"\r\n"+error(lastPcscError);
}
static bool load(bool local) {
    if(module) { FreeLibrary(module); module=nullptr; }
    auto path=directory+L"\\WinSCard.dll";
    if(!local) { wchar_t system[MAX_PATH]; GetSystemDirectoryW(system,MAX_PATH); path=std::wstring(system)+L"\\WinSCard.dll"; }
    module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module) { SetWindowTextW(output,(L"DLL を読み込めません: "+path+L"\r\n"+error(GetLastError(),true)+L"\r\nEXE と DLL の x64 / x86 を一致させてください。").c_str()); return false; }
#define BIND(var,name) var=reinterpret_cast<decltype(var)>(GetProcAddress(module,name)); if(!var) { SetWindowTextW(output,L"DLL に必要な PC/SC 関数がありません。"); FreeLibrary(module); module=nullptr; return false; }
    BIND(providerEstablish,"SCardEstablishContext")
    cvEstablish=[](DWORD scope,LPCVOID a,LPCVOID b,LPSCARDCONTEXT c)->LONG { return recordPcscError(providerEstablish(scope,a,b,c)); };
    BIND(cvRelease,"SCardReleaseContext")
    BIND(providerList,"SCardListReadersW")
    cvList=[](SCARDCONTEXT c,LPCWSTR groups,LPWSTR names,LPDWORD count)->LONG { return recordPcscError(providerList(c,groups,names,count)); };
    BIND(providerConnect,"SCardConnectW")
    cvConnect=[](SCARDCONTEXT c,LPCWSTR name,DWORD share,DWORD protocol,LPSCARDHANDLE card,LPDWORD active)->LONG { return recordPcscError(providerConnect(c,name,share,protocol,card,active)); };
    BIND(cvDisconnect,"SCardDisconnect")
    BIND(providerTransmit,"SCardTransmit")
    cvTransmit=transmitAndCapture;
#undef BIND
    return true;
}
static void enumerate() {
    SendMessageW(readers,CB_RESETCONTENT,0,0);
    EnableWindow(readButton,FALSE);
    if(!load(SendMessageW(providers,CB_GETCURSEL,0,0)==1)) return;
    SCARDCONTEXT context=0;
    LONG rc=cvEstablish(SCARD_SCOPE_USER,nullptr,nullptr,&context);
    if(rc!=SCARD_S_SUCCESS) { SetWindowTextW(output,error(rc).c_str()); return; }
    DWORD count=0;
    rc=cvList(context,nullptr,nullptr,&count);
    std::vector<wchar_t> names(count+2,0);
    if(rc==SCARD_S_SUCCESS && count) rc=cvList(context,nullptr,names.data(),&count);
    cvRelease(context);
    if(rc!=SCARD_S_SUCCESS) { SetWindowTextW(output,(error(rc)+L"\r\nリーダーの接続と Smart Card サービスを確認してください。").c_str()); return; }
    for(auto p=names.data(); *p; p+=wcslen(p)+1) SendMessageW(readers,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(p));
    bool found=SendMessageW(readers,CB_GETCOUNT,0,0)>0;
    SendMessageW(readers,CB_SETCURSEL,0,0); EnableWindow(readButton,found);
    SetWindowTextW(output,found?L"リーダーを選択して「ID取得」を押してください。":L"カードリーダーが見つかりません。");
}
static std::wstring fetch(const std::wstring& reader) {
    lastPcscError=SCARD_S_SUCCESS;
    if(override_card_reader_name_pattern(reader.c_str())<0) return L"リーダー名が長すぎます。";
    auto release=[](B_CAS_CARD* c){c->release(c);};
    std::unique_ptr<B_CAS_CARD,decltype(release)> card(create_b_cas_card(),release);
    if(!card) return L"メモリを確保できません。";
    int rc=card->init(card.get());
    if(rc<0) return L"カード初期化に失敗しました (libaribb25: "+std::to_wstring(rc)+L")。"+communicationError()+L"\r\nカードの挿入・向き・対応カードかを確認してください。";
    B_CAS_ID ids{};
    lastPcscError=SCARD_S_SUCCESS;
    rc=card->get_id(card.get(),&ids);
    if(rc<0) return L"カードID取得に失敗しました (libaribb25: "+std::to_wstring(rc)+L")。"+communicationError();
    std::wstring result=L"リーダー: "+reader+L"\r\n\r\n";
    if(ids.count<=0 || !ids.data) return result+L"カードIDがありません。";
    if(displayIds.size()!=static_cast<size_t>(ids.count)) return L"表示用チェックコードを取得できませんでした。";
    for(int i=0;i<ids.count;i++) {
        const auto& metadata=displayIds[i];
        if(metadata.raw!=static_cast<uint64_t>(ids.data[i])) return L"カードIDの応答が一致しません。再取得してください。";
        wchar_t hex[32];
        swprintf_s(hex,L"%012llX",static_cast<unsigned long long>(metadata.raw));
        result+=(i==0?L"カードID: ":L"グループID "+std::to_wstring(i)+L": ");
        result+=formatDisplayCardId(metadata);
        result+=L"\r\nHEX: "+std::wstring(hex)+L"\r\n\r\n";
    }
    return result;
}
static HWND control(const wchar_t* cls,const wchar_t* text,DWORD style,int id) {
    HWND h=CreateWindowW(cls,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);
    SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE); return h;
}
static LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_CREATE) {
        window=h;
        providers=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,1);
        SendMessageW(providers,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Windows 標準 PC/SC"));
        const DWORD localAttributes=GetFileAttributesW((directory+L"\\WinSCard.dll").c_str());
        const bool hasLocalDll=localAttributes!=INVALID_FILE_ATTRIBUTES && !(localAttributes&FILE_ATTRIBUTE_DIRECTORY);
        if(hasLocalDll) SendMessageW(providers,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"EXE と同じ場所の WinSCard.dll"));
        SendMessageW(providers,CB_SETCURSEL,hasLocalDll?1:0,0);
        readers=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,2);
        scan=control(L"BUTTON",L"再検索",WS_TABSTOP,3);
        readButton=control(L"BUTTON",L"ID取得",WS_TABSTOP,4);
        copyButton=control(L"BUTTON",L"コピー",WS_TABSTOP,5);
        output=control(L"EDIT",L"",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL|WS_BORDER|WS_TABSTOP,6);
        enumerate(); return 0;
    }
    if(msg==WM_GETMINMAXINFO) { auto info=reinterpret_cast<MINMAXINFO*>(l); info->ptMinTrackSize={480,320}; return 0; }
    if(msg==WM_SIZE) {
        int width=LOWORD(l),height=HIWORD(l);
        MoveWindow(providers,16,16,width-140,200,TRUE); MoveWindow(scan,width-112,16,96,28,TRUE);
        MoveWindow(readers,16,56,width-32,260,TRUE);
        MoveWindow(readButton,16,96,100,30,TRUE); MoveWindow(copyButton,128,96,100,30,TRUE);
        MoveWindow(output,16,142,width-32,height-158,TRUE); return 0;
    }
    if(msg==WM_COMMAND && !busy) {
        int id=LOWORD(w);
        if(id==3 || (id==1 && HIWORD(w)==CBN_SELCHANGE)) enumerate();
        if(id==4) {
            LRESULT selection=SendMessageW(readers,CB_GETCURSEL,0,0);
            if(selection==CB_ERR) return 0;
            LRESULT len=SendMessageW(readers,CB_GETLBTEXTLEN,selection,0);
            std::wstring name(static_cast<size_t>(len)+1,L'\0');
            SendMessageW(readers,CB_GETLBTEXT,selection,reinterpret_cast<LPARAM>(name.data())); name.resize(len);
            busy=true; for(HWND c:{providers,readers,scan,readButton,copyButton}) EnableWindow(c,FALSE);
            SetWindowTextW(output,L"カードIDを取得しています…");
            std::thread([name,h]{ auto result=new std::wstring(fetch(name)); if(!PostMessageW(h,Completed,0,reinterpret_cast<LPARAM>(result))) delete result; }).detach();
        }
        if(id==5) {
            int len=GetWindowTextLengthW(output); HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,(len+1)*sizeof(wchar_t));
            if(memory) { auto p=static_cast<wchar_t*>(GlobalLock(memory)); if(p) { GetWindowTextW(output,p,len+1); GlobalUnlock(memory); if(OpenClipboard(h)) { EmptyClipboard(); if(SetClipboardData(CF_UNICODETEXT,memory)) memory=nullptr; CloseClipboard(); } } if(memory) GlobalFree(memory); }
        }
        return 0;
    }
    if(msg==Completed) { std::unique_ptr<std::wstring> result(reinterpret_cast<std::wstring*>(l)); SetWindowTextW(output,result->c_str()); busy=false; for(HWND c:{providers,readers,scan,readButton,copyButton}) EnableWindow(c,TRUE); return 0; }
    if(msg==WM_CLOSE && busy) return 0;
    if(msg==WM_DESTROY) { if(module) FreeLibrary(module); PostQuitMessage(0); return 0; }
    return DefWindowProcW(h,msg,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    wchar_t exe[32768]; GetModuleFileNameW(nullptr,exe,32768); directory=exe; directory.resize(directory.find_last_of(L"\\/"));
    WNDCLASSW wc{}; wc.lpfnWndProc=proc; wc.hInstance=instance; wc.lpszClassName=L"CardIDViewer"; wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1); RegisterClassW(&wc);
    HWND h=CreateWindowW(wc.lpszClassName,L"カードIDビューアー — libaribb25",WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,CW_USEDEFAULT,720,460,nullptr,nullptr,instance,nullptr);
    if(!h) return 1; ShowWindow(h,show);
    MSG msg; while(GetMessageW(&msg,nullptr,0,0)>0) { if(!IsDialogMessageW(h,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } } return static_cast<int>(msg.wParam);
}


