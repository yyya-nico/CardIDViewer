#pragma once
#include <cstdint>
#include <string>
#include <vector>

// ARIB STD-B25 v6.7-E1, Table 4-52 (printed page 100).
struct DisplayCardId {
    uint8_t manufacturer;
    uint8_t version;
    uint64_t raw;
    uint16_t check;
};

inline bool parseDisplayCardIds(const uint8_t* response, size_t size,
                               std::vector<DisplayCardId>& ids) {
    ids.clear();
    if(!response || size<9) return false;
    const size_t count=response[6];
    // Seven header bytes, ten bytes per ID, and SW1/SW2.
    if(count==0 || size!=9+10*count || response[size-2]!=0x90 || response[size-1]!=0x00) return false;
    for(size_t i=0;i<count;i++) {
        const auto p=response+7+10*i;
        uint64_t raw=0;
        for(int j=2;j<8;j++) raw=(raw<<8)|p[j];
        ids.push_back({p[0],p[1],raw,static_cast<uint16_t>((p[8]<<8)|p[9])});
    }
    return true;
}

inline std::wstring formatDisplayCardId(const DisplayCardId& card) {
    // The upper three bits are a separate ID identifier, not part of the
    // decimal conversion of the lower 45-bit ID.
    const uint64_t identifier=card.raw>>45;
    const uint64_t id=card.raw&((uint64_t{1}<<45)-1);
    wchar_t digits[21];
    swprintf_s(digits,L"%llu%014llu%05u",
               static_cast<unsigned long long>(identifier),
               static_cast<unsigned long long>(id),static_cast<unsigned>(card.check));
    std::wstring result;
    for(int i=0;i<20;i++) { if(i && i%4==0) result+=L' '; result+=digits[i]; }
    return result;
}
