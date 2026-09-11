#include "currency_metadata.h"
#include <stddef.h>
#include <string.h>

typedef struct {
    const char *code;
    const char *symbol;
} currency_metadata_t;

/* Standard currency symbols used below the amount on the main page. */
static const currency_metadata_t g_currency_metadata[] = {
    {"EUR", "€"}, {"USD", "$"}, {"CNY", "¥"}, {"RUB", "₽"},
    {"TRY", "₺"}, {"GBP", "£"}, {"MXN", "$"}, {"CAD", "$"},
    {"ILS", "₪"}, {"AED", "د.ا"}, {"SAR", "ر.س"}, {"IRR", "﷼"},
    {"JPY", "¥"}, {"HKD", "$"}, {"AUD", "$"}, {"SGD", "$"},
    {"CHF", "Fr"}, {"ZAR", "R"}, {"IDR", "Rp"}, {"MOP", "P"},
    {"PKR", "₨"}, {"QAR", "ر.ق"}, {"THB", "฿"}, {"XOF", "Fr"},
    {"XAF", "Fr"}, {"TJS", "ЅМ"}, {"UAH", "₴"}, {"AMD", "դր."},
    {"AZN", "₼"}, {"LBP", "ل.ل"}, {"EGP", "ج.م"}, {"KRW", "₩"},
    {"PHP", "₱"}, {"INR", "₹"}, {"ISK", "kr"}, {"SOS", "Sh"},
    {"MAD", "د.م."}, {"DZD", "د.ج"}, {"OMR", "ر.ع."},
    {"IQD", "ع.د"}, {"PLN", "zł"},
};

static const currency_metadata_t *currency_metadata_find(const char *code)
{
    if (code == NULL) return NULL;
    for (size_t i = 0; i < sizeof(g_currency_metadata) /
                               sizeof(g_currency_metadata[0]); i++) {
        if (strcmp(code, g_currency_metadata[i].code) == 0)
            return &g_currency_metadata[i];
    }
    return NULL;
}

const char *currency_metadata_symbol(const char *code)
{
    const currency_metadata_t *metadata = currency_metadata_find(code);
    return metadata != NULL ? metadata->symbol : NULL;
}
