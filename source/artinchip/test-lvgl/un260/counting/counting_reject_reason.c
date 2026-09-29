#include "un260/lv_system/ui_i18n.h"
#include "counting_reject_reason.h"

#include <stddef.h>

/* UN260 protocol framework 5, section 10 / 0x0C, codes 0x00-0x31.
 * Meanings follow the Chinese table (several legacy English labels are vague).
 * Causes/actions are conservative operator guidance, not a hardware diagnosis.
 * A reject code alone does not establish authenticity. Keep legacy list labels. */
static const counting_reject_guide_t guides[0x32]={
    [0x00]={UI_N_("Reject pocket empty"),UI_N_("The controller reports no notes in the reject pocket."),UI_N_("No rejected note was reported for this list."),UI_N_("No reject handling is needed. This is not an authenticity verdict.")},
    [0x01]={UI_N_("Infrared image feature 1"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x02]={UI_N_("Infrared image feature 2"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x03]={UI_N_("Infrared image feature 3"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x04]={UI_N_("Infrared image feature 4"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x05]={UI_N_("Infrared image feature 5"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x06]={UI_N_("Infrared image feature 6"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x07]={UI_N_("Infrared image feature 7"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x08]={UI_N_("Infrared image feature 8"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x09]={UI_N_("Infrared image feature 9"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x0a]={UI_N_("Infrared image feature 10"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x0b]={UI_N_("Infrared image feature 11"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x0c]={UI_N_("Infrared image feature 12"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x0d]={UI_N_("Infrared image feature 13"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x0e]={UI_N_("Infrared image feature 14"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x0f]={UI_N_("Infrared image feature 15"),UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x10]={UI_N_("Stacker full"),UI_N_("Additional notes arrived after the stacker became full."),UI_N_("The stacker was not emptied before more notes arrived."),UI_N_("Wait for the motor to stop, empty the stacker and recount the rejected notes.")},
    [0x11]={UI_N_("Side magnetic strength"),UI_N_("Side magnetic signal strength was outside the accepted range."),UI_N_("An obscured or worn magnetic area, an incorrect currency, or an abnormal magnetic feature."),UI_N_("Check the currency. Recount separately; persistent detection requires verification.")},
    [0x12]={UI_N_("Side magnetic position"),UI_N_("The side magnetic signal was at an unexpected position."),UI_N_("Skewed feeding, note damage, or an abnormal magnetic pattern."),UI_N_("Align the guides and recount separately. Verify a repeated warning.")},
    [0x13]={UI_N_("Centre magnetic strength"),UI_N_("Central magnetic signal strength was outside the accepted range."),UI_N_("A worn note, an incorrect currency, or an abnormal central magnetic feature."),UI_N_("Check the currency and note condition. Recount separately and verify if repeated.")},
    [0x14]={UI_N_("Central magnetic code"),UI_N_("The central magnetic code did not match the expected pattern."),UI_N_("An incorrect currency, damaged magnetic features, or an abnormal code."),UI_N_("Recount separately. Follow your organisation's verification procedure if repeated.")},
    [0x15]={UI_N_("Ultraviolet fluorescence"),UI_N_("The UV response was outside the accepted range."),UI_N_("Contamination, note condition, incorrect currency, or an abnormal UV feature."),UI_N_("Check the currency and note condition. Verify persistent warnings; do not lower sensitivity.")},
    [0x16]={UI_N_("Overlapping notes 1"),UI_N_("More than one note may have passed together."),UI_N_("Notes stuck together or an uneven stack."),UI_N_("Separate and straighten the notes. Reload with aligned guides and recount.")},
    [0x17]={UI_N_("Overlapping notes 2"),UI_N_("A second overlap check detected possible double feeding."),UI_N_("Stuck notes, folds or uneven feeding."),UI_N_("Separate and straighten the notes; recount slowly. Power off before any cleaning, following the manual.")},
    [0x18]={UI_N_("Note too wide"),UI_N_("The measured note width exceeded the expected range."),UI_N_("Overlapping notes, skewed feeding or an incorrect currency."),UI_N_("Separate the stack, align the guides and check the selected currency.")},
    [0x19]={UI_N_("Note too short"),UI_N_("The measured note length was below the expected range."),UI_N_("A folded or damaged note, poor feeding or an incorrect currency."),UI_N_("Flatten without damaging the note. Check the currency and recount separately.")},
    [0x1a]={UI_N_("Insufficient note gap"),UI_N_("The gap between consecutive notes was too small."),UI_N_("Chained or stuck notes, or uneven feeding."),UI_N_("Separate the stack and reload it evenly. Retry at low speed.")},
    [0x1b]={UI_N_("Image data timeout"),UI_N_("Expected note information did not arrive in time."),UI_N_("A temporary image-processing or communication delay."),UI_N_("Recount after the motor stops. If repeated, retain the code and contact service.")},
    [0x1c]={UI_N_("Unknown note size"),UI_N_("The measured note size was not recognised."),UI_N_("Incorrect currency, damaged notes, skewed feeding or an unsupported note type."),UI_N_("Check currency and alignment. Ask service about support if the same note type repeats.")},
    [0x1d]={UI_N_("Unknown orientation"),UI_N_("The note orientation could not be recognised."),UI_N_("Obscured image features, folds, damage or skewed feeding."),UI_N_("Flatten and align the note. Recount separately; verify repeated warnings.")},
    [0x1e]={UI_N_("Different denomination"),UI_N_("The denomination differs from the first reference note."),UI_N_("Mixed denominations were fed in a single-denomination task."),UI_N_("Separate denominations or select Mixed if your task allows it.")},
    [0x1f]={UI_N_("Face mismatch"),UI_N_("The note face does not match the selected face rule."),UI_N_("A note was loaded with the opposite face."),UI_N_("Arrange notes to the required face and recount, or change the rule if appropriate.")},
    [0x20]={UI_N_("Orientation mismatch"),UI_N_("The note direction does not match the selected rule."),UI_N_("A note was loaded in the opposite direction."),UI_N_("Align the notes to the required direction and recount.")},
    [0x21]={UI_N_("Skewed note"),UI_N_("The note passed at an excessive angle."),UI_N_("Uneven loading, guide spacing or a folded corner."),UI_N_("Straighten the stack and adjust the guides. Recount at low speed.")},
    [0x22]={UI_N_("Optically variable feature"),UI_N_("The OVD check did not match the expected response."),UI_N_("A worn or obscured feature, incorrect currency, or an abnormal security feature."),UI_N_("Check the currency. Recount separately and verify a repeated warning.")},
    [0x23]={UI_N_("Infrared security thread"),UI_N_("The infrared thread check did not match the expected response."),UI_N_("A damaged or obscured thread, incorrect currency, or an abnormal feature."),UI_N_("Check the currency and note condition. Verify persistent warnings.")},
    [0x24]={UI_N_("Possible hole"),UI_N_("The controller reports a suspected hole in the note."),UI_N_("A perforated or damaged note, or an obscured image."),UI_N_("Set the note aside for manual condition review.")},
    [0x25]={UI_N_("Possible folded corner"),UI_N_("The controller reports a suspected folded corner."),UI_N_("A corner may be folded over or damaged."),UI_N_("Flatten an intact corner carefully and recount. Keep damaged notes aside.")},
    [0x26]={UI_N_("Dirt detected"),UI_N_("The condition check detected dirt."),UI_N_("Stains or contamination on the note."),UI_N_("Set the note aside for condition review. Do not clean currency in the machine.")},
    [0x27]={UI_N_("Tape detected"),UI_N_("The condition check indicates adhesive tape."),UI_N_("A repaired or taped note."),UI_N_("Keep the note separate for manual review.")},
    [0x28]={UI_N_("Tear detected"),UI_N_("The condition check indicates a tear."),UI_N_("A torn or damaged note."),UI_N_("Keep the note separate; follow your damaged-note procedure.")},
    [0x29]={UI_N_("Crumpled note"),UI_N_("The condition check indicates heavy creasing."),UI_N_("A crumpled or heavily folded note."),UI_N_("Flatten gently and recount if safe. Keep damaged notes aside.")},
    [0x2a]={UI_N_("Missing ink"),UI_N_("The image check indicates missing or faded ink."),UI_N_("Wear, abrasion or an abnormal printed area."),UI_N_("Recount separately and inspect the printed features under your verification procedure.")},
    [0x2b]={UI_N_("Soiled note"),UI_N_("The condition check indicates soiling."),UI_N_("General wear, staining or contamination."),UI_N_("Set the note aside for manual condition review.")},
    [0x2c]={UI_N_("Limp note"),UI_N_("The condition check indicates excessive limpness."),UI_N_("A heavily worn note with reduced stiffness."),UI_N_("Keep the note separate for condition review.")},
    [0x2d]={UI_N_("Face and orientation"),UI_N_("The note does not satisfy the combined face and direction rule."),UI_N_("The face or direction differs from the reference."),UI_N_("Arrange notes to the required face and direction, then recount.")},
    [0x2e]={UI_N_("Duplicate serial"),UI_N_("The controller reported a repeated serial number."),UI_N_("A serial may have repeated or been read incorrectly."),UI_N_("Compare the serials and recount separately. A duplicate code alone is not an authenticity verdict.")},
    [0x2f]={UI_N_("Missing serial"),UI_N_("The controller could not obtain a required serial number."),UI_N_("The serial area may be folded, obscured, damaged or unreadable."),UI_N_("Check the serial area and recount at low speed. Missing data alone does not prove a counterfeit.")},
    [0x30]={UI_N_("Blacklisted serial"),UI_N_("The controller reported a serial matching its blacklist."),UI_N_("The read serial matched an entry in the configured blacklist."),UI_N_("Check the serial manually and follow your organisation's escalation procedure.")},
    [0x31]={UI_N_("Reserved double check"),UI_N_("The controller reported a reserved double-note code."),UI_N_("The protocol does not define the detailed trigger."),UI_N_("Separate the notes and recount. Retain the code for service if repeated.")},
};
const counting_reject_guide_t *counting_reject_guide_get(uint8_t code)
{
    static const counting_reject_guide_t unknown={UI_N_("Unknown controller code"),UI_N_("This code is not defined in the available protocol."),UI_N_("The cause cannot be determined from this code."),UI_N_("Keep the code and contact service. Do not infer an authenticity result.")};
    return code<0x32?&guides[code]:&unknown;
}

static const char *const g_counting_reject_reasons[0x32] = {
    [0x00] = UI_N_("No Error"),
    [0x01] = UI_N_("IMG F1"),
    [0x02] = UI_N_("IMG F2"),
    [0x03] = UI_N_("IMG F3"),
    [0x04] = UI_N_("IMG F4"),
    [0x05] = UI_N_("IMG F5"),
    [0x06] = UI_N_("IMG F6"),
    [0x07] = UI_N_("IMG F7"),
    [0x08] = UI_N_("IMG F8"),
    [0x09] = UI_N_("IMG F9"),
    [0x0A] = UI_N_("IMG F10"),
    [0x0B] = UI_N_("IMG F11"),
    [0x0C] = UI_N_("IMG F12"),
    [0x0D] = UI_N_("IMG F13"),
    [0x0E] = UI_N_("IMG F14"),
    [0x0F] = UI_N_("IMG F15"),
    [0x10] = UI_N_("ST Full"),
    [0x11] = UI_N_("MG Qty"),
    [0x12] = UI_N_("MG Pos"),
    [0x13] = UI_N_("MT Qty"),
    [0x14] = UI_N_("MT Code"),
    [0x15] = UI_N_("UV"),
    [0x16] = UI_N_("Double 1"),
    [0x17] = UI_N_("Double 2"),
    [0x18] = UI_N_("Long"),
    [0x19] = UI_N_("Short"),
    [0x1A] = UI_N_("GAP"),
    [0x1B] = UI_N_("Time out"),
    [0x1C] = UI_N_("Size Unknow"),
    [0x1D] = UI_N_("Ort Unknow"),
    [0x1E] = UI_N_("Version Unknow"),
    [0x1F] = UI_N_("Face Error"),
    [0x20] = UI_N_("Ort Error"),
    [0x21] = UI_N_("ANGLE"),
    [0x22] = UI_N_("IR-OVD"),
    [0x23] = UI_N_("IR-MT"),
    [0x24] = UI_N_("Hole"),
    [0x25] = UI_N_("DogEar"),
    [0x26] = UI_N_("DIRT"),
    [0x27] = UI_N_("Tape"),
    [0x28] = UI_N_("Tears"),
    [0x29] = UI_N_("Crumples"),
    [0x2A] = UI_N_("De_ink"),
    [0x2B] = UI_N_("Soiling"),
    [0x2C] = UI_N_("Error_Limpness"),
    [0x2D] = UI_N_("IMG F&O Err"),
    [0x2E] = UI_N_("OCR1 Err"),
    [0x2F] = UI_N_("OCR2 Err"),
    [0x30] = UI_N_("OCR3 Err"),
    [0x31] = UI_N_("Double Rsv"),
};

const char *counting_reject_reason_get(uint8_t code)
{
    if (code < sizeof(g_counting_reject_reasons) /
                   sizeof(g_counting_reject_reasons[0]) &&
        g_counting_reject_reasons[code] != NULL) {
        return g_counting_reject_reasons[code];
    }
    return UI_N_("Unknown Error");
}
