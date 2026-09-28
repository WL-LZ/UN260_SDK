#include "counting_reject_reason.h"

#include <stddef.h>

/* Catalogue follows protocol9, reject reply 0x00-0x31. Legacy short labels stay unchanged. */
static const counting_reject_guide_t guides[0x32]={
    [0x00]={"No rejection","No rejected note was reported.","No fault is indicated.","No action is required."},
    [0x01]={"Infrared image feature 1","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x02]={"Infrared image feature 2","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x03]={"Infrared image feature 3","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x04]={"Infrared image feature 4","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x05]={"Infrared image feature 5","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x06]={"Infrared image feature 6","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x07]={"Infrared image feature 7","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x08]={"Infrared image feature 8","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x09]={"Infrared image feature 9","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x0a]={"Infrared image feature 10","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x0b]={"Infrared image feature 11","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x0c]={"Infrared image feature 12","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x0d]={"Infrared image feature 13","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x0e]={"Infrared image feature 14","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x0f]={"Infrared image feature 15","An infrared image feature did not match the expected pattern.","A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature.","Check the currency and note condition. Recount separately; if repeated, follow your verification procedure."},
    [0x10]={"Stacker full","Additional notes arrived after the stacker became full.","The stacker was not emptied before more notes arrived.","Wait for the motor to stop, empty the stacker and recount the rejected notes."},
    [0x11]={"Side magnetic strength","Side magnetic signal strength was outside the accepted range.","An obscured or worn magnetic area, an incorrect currency, or an abnormal magnetic feature.","Check the currency. Recount separately; persistent detection requires verification."},
    [0x12]={"Side magnetic position","The side magnetic signal was at an unexpected position.","Skewed feeding, note damage, or an abnormal magnetic pattern.","Align the guides and recount separately. Verify a repeated warning."},
    [0x13]={"Centre magnetic strength","Central magnetic signal strength was outside the accepted range.","A worn note, an incorrect currency, or an abnormal central magnetic feature.","Check the currency and note condition. Recount separately and verify if repeated."},
    [0x14]={"Central magnetic code","The central magnetic code did not match the expected pattern.","An incorrect currency, damaged magnetic features, or an abnormal code.","Recount separately. Follow your organisation's verification procedure if repeated."},
    [0x15]={"Ultraviolet fluorescence","The UV response was outside the accepted range.","Contamination, note condition, incorrect currency, or an abnormal UV feature.","Check the currency and note condition. Verify persistent warnings; do not lower sensitivity."},
    [0x16]={"Overlapping notes 1","More than one note may have passed together.","Notes stuck together or an uneven stack.","Separate and straighten the notes. Reload with aligned guides and recount."},
    [0x17]={"Overlapping notes 2","A second overlap check detected possible double feeding.","Stuck notes, folds or uneven feeding.","Separate and straighten the notes. Retry at low speed; clean the feed path if repeated."},
    [0x18]={"Note too wide","The measured note width exceeded the expected range.","Overlapping notes, skewed feeding or an incorrect currency.","Separate the stack, align the guides and check the selected currency."},
    [0x19]={"Note too short","The measured note length was below the expected range.","A folded or damaged note, poor feeding or an incorrect currency.","Flatten without damaging the note. Check the currency and recount separately."},
    [0x1a]={"Insufficient note gap","The gap between consecutive notes was too small.","Chained or stuck notes, or uneven feeding.","Separate the stack and reload it evenly. Retry at low speed."},
    [0x1b]={"Image data timeout","Expected note information did not arrive in time.","A temporary image-processing or communication delay.","Recount after the motor stops. If repeated, retain the code and contact service."},
    [0x1c]={"Unknown note size","The measured note size was not recognised.","Incorrect currency, damaged notes, skewed feeding or an unsupported note type.","Check currency and alignment. Ask service about support if the same note type repeats."},
    [0x1d]={"Unknown orientation","The note orientation could not be recognised.","Obscured image features, folds, damage or skewed feeding.","Flatten and align the note. Recount separately; verify repeated warnings."},
    [0x1e]={"Different denomination","The denomination differs from the first reference note.","Mixed denominations were fed in a single-denomination task.","Separate denominations or select Mixed if your task allows it."},
    [0x1f]={"Face mismatch","The note face does not match the selected face rule.","A note was loaded with the opposite face.","Arrange notes to the required face and recount, or change the rule if appropriate."},
    [0x20]={"Orientation mismatch","The note direction does not match the selected rule.","A note was loaded in the opposite direction.","Align the notes to the required direction and recount."},
    [0x21]={"Skewed note","The note passed at an excessive angle.","Uneven loading, guide spacing or a folded corner.","Straighten the stack and adjust the guides. Recount at low speed."},
    [0x22]={"Optically variable feature","The OVD check did not match the expected response.","A worn or obscured feature, incorrect currency, or an abnormal security feature.","Check the currency. Recount separately and verify a repeated warning."},
    [0x23]={"Infrared security thread","The infrared thread check did not match the expected response.","A damaged or obscured thread, incorrect currency, or an abnormal feature.","Check the currency and note condition. Verify persistent warnings."},
    [0x24]={"Hole detected","The note image indicates a hole.","A perforated or damaged note.","Set the note aside for manual condition review."},
    [0x25]={"Folded corner","The note image indicates a folded corner.","A corner is folded over or missing.","Flatten an intact corner carefully and recount. Keep damaged notes aside."},
    [0x26]={"Dirt detected","The condition check detected dirt.","Stains or contamination on the note.","Set the note aside for condition review. Do not clean currency in the machine."},
    [0x27]={"Tape detected","The condition check indicates adhesive tape.","A repaired or taped note.","Keep the note separate for manual review."},
    [0x28]={"Tear detected","The condition check indicates a tear.","A torn or damaged note.","Keep the note separate; follow your damaged-note procedure."},
    [0x29]={"Crumpled note","The condition check indicates heavy creasing.","A crumpled or heavily folded note.","Flatten gently and recount if safe. Keep damaged notes aside."},
    [0x2a]={"Missing ink","The image check indicates missing or faded ink.","Wear, abrasion or an abnormal printed area.","Recount separately and inspect the printed features under your verification procedure."},
    [0x2b]={"Soiled note","The condition check indicates soiling.","General wear, staining or contamination.","Set the note aside for manual condition review."},
    [0x2c]={"Limp note","The condition check indicates excessive limpness.","A heavily worn note with reduced stiffness.","Keep the note separate for condition review."},
    [0x2d]={"Face and orientation","The note does not satisfy the combined face and direction rule.","The face or direction differs from the reference.","Arrange notes to the required face and direction, then recount."},
    [0x2e]={"Duplicate serial","The controller reported a repeated serial number.","A serial may have repeated or been read incorrectly.","Compare the serials and recount separately. A duplicate code alone is not an authenticity verdict."},
    [0x2f]={"Missing serial","The controller could not obtain a required serial number.","The serial area may be folded, obscured, damaged or unreadable.","Check the serial area and recount at low speed. Missing data alone does not prove a counterfeit."},
    [0x30]={"Blacklisted serial","The controller reported a serial matching its blacklist.","The read serial matched an entry in the configured blacklist.","Check the serial manually and follow your organisation's escalation procedure."},
    [0x31]={"Reserved double check","The controller reported a reserved double-note code.","The protocol does not define the detailed trigger.","Separate the notes and recount. Retain the code for service if repeated."},
};
const counting_reject_guide_t *counting_reject_guide_get(uint8_t code)
{
    static const counting_reject_guide_t unknown={"Unknown controller code","This code is not defined in the available protocol.","The cause cannot be determined from this code.","Keep the code and contact service. Do not infer an authenticity result."};
    return code<0x32?&guides[code]:&unknown;
}

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
