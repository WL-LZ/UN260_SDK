#include "counting_reject_reason.h"

#include <stddef.h>

static const char *const g_counting_reject_reasons[0x32] = {
    [0x00] = "No Error",
    [0x01] = "IMG F1",
    [0x02] = "IMG F2",
    [0x03] = "IMG F3",
    [0x04] = "IMG F4",
    [0x05] = "IMG F5",
    [0x06] = "IMG F6",
    [0x07] = "IMG F7",
    [0x08] = "IMG F8",
    [0x09] = "IMG F9",
    [0x0A] = "IMG F10",
    [0x0B] = "IMG F11",
    [0x0C] = "IMG F12",
    [0x0D] = "IMG F13",
    [0x0E] = "IMG F14",
    [0x0F] = "IMG F15",
    [0x10] = "ST Full",
    [0x11] = "MG Qty",
    [0x12] = "MG Pos",
    [0x13] = "MT Qty",
    [0x14] = "MT Code",
    [0x15] = "UV",
    [0x16] = "Double 1",
    [0x17] = "Double 2",
    [0x18] = "Long",
    [0x19] = "Short",
    [0x1A] = "GAP",
    [0x1B] = "Time out",
    [0x1C] = "Size Unknow",
    [0x1D] = "Ort Unknow",
    [0x1E] = "Version Unknow",
    [0x1F] = "Face Error",
    [0x20] = "Ort Error",
    [0x21] = "ANGLE",
    [0x22] = "IR-OVD",
    [0x23] = "IR-MT",
    [0x24] = "Hole",
    [0x25] = "DogEar",
    [0x26] = "DIRT",
    [0x27] = "Tape",
    [0x28] = "Tears",
    [0x29] = "Crumples",
    [0x2A] = "De_ink",
    [0x2B] = "Soiling",
    [0x2C] = "Error_Limpness",
    [0x2D] = "IMG F&O Err",
    [0x2E] = "OCR1 Err",
    [0x2F] = "OCR2 Err",
    [0x30] = "OCR3 Err",
    [0x31] = "Double Rsv",
};

const char *counting_reject_reason_get(uint8_t code)
{
    if (code < sizeof(g_counting_reject_reasons) /
                   sizeof(g_counting_reject_reasons[0]) &&
        g_counting_reject_reasons[code] != NULL) {
        return g_counting_reject_reasons[code];
    }
    return "Unknown Error";
}
