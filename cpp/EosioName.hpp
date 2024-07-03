#pragma once

#include <QString>

// This code adapted from eosio, to convert name strings to and from integers
namespace eosio {
inline static uint64_t char_to_symbol(char c) {
    if(c >= 'a' && c <= 'z')
        return (c - 'a') + 6;
    if(c >= '1' && c <= '5')
        return (c - '1') + 1;
    return 0;
}

inline static uint64_t string_to_uint64_t(QString str) {
    uint64_t n = 0;
    int i;
    for (i = 0 ; i < str.length() && i < 12; ++i) {
        // NOTE: char_to_symbol() returns char type, and without this explicit
        // expansion to uint64 type, the compilation fails at the point of usage
        // of string_to_name(), where the usage requires constant (compile time) expression.
        n |= (char_to_symbol(str[i].toLatin1()) & 0x1f) << (64 - 5 * (i + 1));
    }

    // The for-loop encoded up to 60 high bits into uint64 'name' variable,
    // if (strlen(str) > 12) then encode str[12] into the low (remaining)
    // 4 bits of 'name'
    if (i == 12)
        n |= char_to_symbol(str[12].toLatin1()) & 0x0F;
    return n;
}

inline static QString name_to_string(uint64_t name) {
    static const char* charmap = ".12345abcdefghijklmnopqrstuvwxyz";

    QString str(13,'.');

    uint64_t tmp = name;
    for(uint32_t i = 0; i <= 12; ++i) {
        char c = charmap[tmp & (i == 0 ? 0x0f : 0x1f)];
        str[12-i] = c;
        tmp >>= (i == 0 ? 4 : 5);
    }

    while (str.endsWith('.')) str.chop(1);
    return str;
}
} // namespace eosio
