#include "../src/main.cpp"
#include <iostream>
int main(){
    if(formatDisplayCardId({0,0,0,0})!=L"0000 0000 0000 0000 0000")return 10;
    if(formatDisplayCardId({0,0,(uint64_t{1}<<45)|12345678901234ULL,7})!=L"1123 4567 8901 2340 0007")return 11;
    std::vector<DisplayCardId> parsed;
    std::vector<uint8_t> response(19,0);
    response[6]=1;response[15]=0x12;response[16]=0x34;response[17]=0x90;
    if(!parseDisplayCardIds(response.data(),response.size(),parsed)||parsed[0].check!=0x1234)return 12;
    if(parseDisplayCardIds(response.data(),response.size()-1,parsed)||!parsed.empty())return 13;
    response[17]=0x6F;
    if(parseDisplayCardIds(response.data(),response.size(),parsed))return 14;
    response[17]=0x90;response[6]=2;
    if(parseDisplayCardIds(response.data(),response.size(),parsed))return 15;
    if(parseDisplayCardIds(nullptr,0,parsed))return 16;
    wchar_t path[32768]; GetFullPathNameW(L"build\\test",32768,path,nullptr); directory=path;
    if(!load(true)) return 1;
    SCARDCONTEXT context; if(cvEstablish(SCARD_SCOPE_USER,nullptr,nullptr,&context))return 2;
    DWORD len=0; if(cvList(context,nullptr,nullptr,&len)||len==0)return 3;
    std::vector<wchar_t> names(len); if(cvList(context,nullptr,names.data(),&len))return 4;
    cvRelease(context);
    auto result=fetch(names.data());
    if(result.find(L"2000 0000 0000 0011 2345")==std::wstring::npos || result.find(L"7351 8437 2088 8316 5535")==std::wstring::npos) return 5;
    if(fetch(L"Missing reader").find(L"libaribb25:")==std::wstring::npos)return 6;
    systemProvider=true; // Exercise system message formatting with controlled PC/SC failures.
    const auto originalConnect=providerConnect;
    providerConnect=[](SCARDCONTEXT,LPCWSTR,DWORD,DWORD,LPSCARDHANDLE,LPDWORD)->LONG { return SCARD_E_NO_SMARTCARD; };
    const auto missing=fetch(names.data());
    providerConnect=originalConnect;
    if(missing.find(L"0x8010000C")==std::wstring::npos || missing.find(error(SCARD_E_NO_SMARTCARD))==std::wstring::npos)return 17;
    if(fetch(names.data()).find(L"PC/SC エラー:")!=std::wstring::npos)return 18;
    wchar_t* expected=nullptr;
    DWORD size=FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,SCARD_E_NO_SMARTCARD,0,reinterpret_cast<LPWSTR>(&expected),0,nullptr);
    if(!size || !expected)return 19;
    std::wstring text(expected,size); LocalFree(expected);
    while(!text.empty() && iswspace(text.back()))text.pop_back();
    const auto readable=error(SCARD_E_NO_SMARTCARD);
    if(readable.find(L"0x8010000C")==std::wstring::npos || readable.find(text)==std::wstring::npos)return 20;
    if(error(static_cast<LONG>(0xDEADBEEF)).find(L"説明メッセージを取得できません。")==std::wstring::npos)return 21;
    systemProvider=false;
    if(error(SCARD_E_NO_SMARTCARD).find(text)!=std::wstring::npos)return 22;
    if(module){FreeLibrary(module);module=nullptr;}
    if(!load(false))return 7;
    std::cout<<"PASS: local DLL, reader enumeration, ARIB ID format, check codes, malformed responses, two IDs, unknown reader, system DLL, readable Windows errors\n";
}


