#include "currency_metadata.h"
#include <stddef.h>
#include <string.h>

const char *currency_metadata_symbol(const char *code)
{
    static const struct { const char *code, *symbol; } units[] = {
        {"USD", "$"}, {"CNY", "¥"}, {"JPY", "¥"}, {"EUR", "€"},
        {"GBP", "£"}, {"EGP", "£"}, {"KRW", "₩"}, {"PHP", "₱"},
        {"TRY", "₺"}, {"INR", "₹"}, {"PKR", "Rs"}, {"ISK", "kr"},
        {"SOS", "Sh"}, {"MAD", "DH"}, {"DZD", "DA"},
        {"AED", "DH"}, {"SAR", "SR"}, {"OMR", "RO"},
        {"QAR", "QR"}, {"IQD", "ID"}
    };
    if (!code) return NULL;
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); ++i)
        if (strcmp(code, units[i].code) == 0) return units[i].symbol;
    return NULL;
}
