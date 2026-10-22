#include "un260/lv_system/ui_i18n.h"
#include "counting_reject_reason.h"

#include <stddef.h>

/* UN260 protocol section 10 / command 0x0C, codes 0x00-0x31.
 * Titles are protocol identifiers, identical in every display/report language.
 * The reserved 0x31 has no English name in the protocol; show its code.
 * Guidance remains localized and does not establish a note's authenticity. */
static const counting_reject_guide_t guides[0x32]={
    [0x00]={"No Error",UI_N_("The controller reports no notes in the reject pocket."),UI_N_("No rejected note was reported for this list."),UI_N_("No reject handling is needed. This is not an authenticity verdict.")},
    [0x01]={"IMG Feature1 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x02]={"IMG Feature2 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x03]={"IMG Feature3 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x04]={"IMG Feature4 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x05]={"IMG Feature5 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x06]={"IMG Feature6 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x07]={"IMG Feature7 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x08]={"IMG Feature8 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x09]={"IMG Feature9 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x0a]={"IMG Feature10 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x0b]={"IMG Feature11 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x0c]={"IMG Feature12 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x0d]={"IMG Feature13 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x0e]={"IMG Feature14 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x0f]={"IMG Feature15 Suspect",UI_N_("An infrared image feature did not match the expected pattern."),UI_N_("A worn, folded or obscured note, an incorrect currency selection, or an abnormal security feature."),UI_N_("Check the currency and note condition. Recount separately; if repeated, follow your verification procedure.")},
    [0x10]={"ST Full",UI_N_("Additional notes arrived after the stacker became full."),UI_N_("The stacker was not emptied before more notes arrived."),UI_N_("Wait for the motor to stop, empty the stacker and recount the rejected notes.")},
    [0x11]={"MG Quantity Suspect",UI_N_("Side magnetic signal strength was outside the accepted range."),UI_N_("An obscured or worn magnetic area, an incorrect currency, or an abnormal magnetic feature."),UI_N_("Check the currency. Recount separately; persistent detection requires verification.")},
    [0x12]={"MG Position Suspect",UI_N_("The side magnetic signal was at an unexpected position."),UI_N_("Skewed feeding, note damage, or an abnormal magnetic pattern."),UI_N_("Align the guides and recount separately. Verify a repeated warning.")},
    [0x13]={"MT Quantity Suspect",UI_N_("Central magnetic signal strength was outside the accepted range."),UI_N_("A worn note, an incorrect currency, or an abnormal central magnetic feature."),UI_N_("Check the currency and note condition. Recount separately and verify if repeated.")},
    [0x14]={"MT Code Suspect",UI_N_("The central magnetic code did not match the expected pattern."),UI_N_("An incorrect currency, damaged magnetic features, or an abnormal code."),UI_N_("Recount separately. Follow your organisation's verification procedure if repeated.")},
    [0x15]={"UV Suspect",UI_N_("The UV response was outside the accepted range."),UI_N_("Contamination, note condition, incorrect currency, or an abnormal UV feature."),UI_N_("Check the currency and note condition. Verify persistent warnings; do not lower sensitivity.")},
    [0x16]={"Double Suspect 1",UI_N_("More than one note may have passed together."),UI_N_("Notes stuck together or an uneven stack."),UI_N_("Separate and straighten the notes. Reload with aligned guides and recount.")},
    [0x17]={"Double Suspect 2",UI_N_("A second overlap check detected possible double feeding."),UI_N_("Stuck notes, folds or uneven feeding."),UI_N_("Separate and straighten the notes; recount slowly. Power off before any cleaning, following the manual.")},
    [0x18]={"Long",UI_N_("The measured note width exceeded the expected range."),UI_N_("Overlapping notes, skewed feeding or an incorrect currency."),UI_N_("Separate the stack, align the guides and check the selected currency.")},
    [0x19]={"Short",UI_N_("The measured note length was below the expected range."),UI_N_("A folded or damaged note, poor feeding or an incorrect currency."),UI_N_("Flatten without damaging the note. Check the currency and recount separately.")},
    [0x1a]={"GAP",UI_N_("The gap between consecutive notes was too small."),UI_N_("Chained or stuck notes, or uneven feeding."),UI_N_("Separate the stack and reload it evenly. Retry at low speed.")},
    [0x1b]={"IMG Time out",UI_N_("Expected note information did not arrive in time."),UI_N_("A temporary image-processing or communication delay."),UI_N_("Recount after the motor stops. If repeated, retain the code and contact service.")},
    [0x1c]={"IMG Size Unknow",UI_N_("The measured note size was not recognised."),UI_N_("Incorrect currency, damaged notes, skewed feeding or an unsupported note type."),UI_N_("Check currency and alignment. Ask service about support if the same note type repeats.")},
    [0x1d]={"IMG Ort Unknow",UI_N_("The note orientation could not be recognised."),UI_N_("Obscured image features, folds, damage or skewed feeding."),UI_N_("Flatten and align the note. Recount separately; verify repeated warnings.")},
    [0x1e]={"IMG Version Unknow",UI_N_("The denomination differs from the first reference note."),UI_N_("Mixed denominations were fed in a single-denomination task."),UI_N_("Separate denominations or select Mixed if your task allows it.")},
    [0x1f]={"IMG Face Error",UI_N_("The note face does not match the selected face rule."),UI_N_("A note was loaded with the opposite face."),UI_N_("Arrange notes to the required face and recount, or change the rule if appropriate.")},
    [0x20]={"IMG Ort Error",UI_N_("The note direction does not match the selected rule."),UI_N_("A note was loaded in the opposite direction."),UI_N_("Align the notes to the required direction and recount.")},
    [0x21]={"ANGLE",UI_N_("The note passed at an excessive angle."),UI_N_("Uneven loading, guide spacing or a folded corner."),UI_N_("Straighten the stack and adjust the guides. Recount at low speed.")},
    [0x22]={"IR-OVD",UI_N_("The OVD check did not match the expected response."),UI_N_("A worn or obscured feature, incorrect currency, or an abnormal security feature."),UI_N_("Check the currency. Recount separately and verify a repeated warning.")},
    [0x23]={"IR –MT",UI_N_("The infrared thread check did not match the expected response."),UI_N_("A damaged or obscured thread, incorrect currency, or an abnormal feature."),UI_N_("Check the currency and note condition. Verify persistent warnings.")},
    [0x24]={"Hole",UI_N_("The controller reports a suspected hole in the note."),UI_N_("A perforated or damaged note, or an obscured image."),UI_N_("Set the note aside for manual condition review.")},
    [0x25]={"DogEar",UI_N_("The controller reports a suspected folded corner."),UI_N_("A corner may be folded over or damaged."),UI_N_("Flatten an intact corner carefully and recount. Keep damaged notes aside.")},
    [0x26]={"DIRT",UI_N_("The condition check detected dirt."),UI_N_("Stains or contamination on the note."),UI_N_("Set the note aside for condition review. Do not clean currency in the machine.")},
    [0x27]={"Tape",UI_N_("The condition check indicates adhesive tape."),UI_N_("A repaired or taped note."),UI_N_("Keep the note separate for manual review.")},
    [0x28]={"Tears",UI_N_("The condition check indicates a tear."),UI_N_("A torn or damaged note."),UI_N_("Keep the note separate; follow your damaged-note procedure.")},
    [0x29]={"Crumples",UI_N_("The condition check indicates heavy creasing."),UI_N_("A crumpled or heavily folded note."),UI_N_("Flatten gently and recount if safe. Keep damaged notes aside.")},
    [0x2a]={"De_ink",UI_N_("The image check indicates missing or faded ink."),UI_N_("Wear, abrasion or an abnormal printed area."),UI_N_("Recount separately and inspect the printed features under your verification procedure.")},
    [0x2b]={"Soiling",UI_N_("The condition check indicates soiling."),UI_N_("General wear, staining or contamination."),UI_N_("Set the note aside for manual condition review.")},
    [0x2c]={"eError_Limpness",UI_N_("The condition check indicates excessive limpness."),UI_N_("A heavily worn note with reduced stiffness."),UI_N_("Keep the note separate for condition review.")},
    [0x2d]={"IMG Face&Ort Error",UI_N_("The note does not satisfy the combined face and direction rule."),UI_N_("The face or direction differs from the reference."),UI_N_("Arrange notes to the required face and direction, then recount.")},
    [0x2e]={"OCR1 Error",UI_N_("The controller reported a repeated serial number."),UI_N_("A serial may have repeated or been read incorrectly."),UI_N_("Compare the serials and recount separately. A duplicate code alone is not an authenticity verdict.")},
    [0x2f]={"OCR2 Error",UI_N_("The controller could not obtain a required serial number."),UI_N_("The serial area may be folded, obscured, damaged or unreadable."),UI_N_("Check the serial area and recount at low speed. Missing data alone does not prove a counterfeit.")},
    [0x30]={"OCR3 Error",UI_N_("The controller reported a serial matching its blacklist."),UI_N_("The read serial matched an entry in the configured blacklist."),UI_N_("Check the serial manually and follow your organisation's escalation procedure.")},
    [0x31]={"0x31",UI_N_("The controller reported a reserved double-note code."),UI_N_("The protocol does not define the detailed trigger."),UI_N_("Separate the notes and recount. Retain the code for service if repeated.")},
};
const counting_reject_guide_t *counting_reject_guide_get(uint8_t code)
{
    static const counting_reject_guide_t unknown={"Unknown Error",UI_N_("This code is not defined in the available protocol."),UI_N_("The cause cannot be determined from this code."),UI_N_("Keep the code and contact service. Do not infer an authenticity result.")};
    return code<0x32?&guides[code]:&unknown;
}

const char *counting_reject_reason_get(uint8_t code)
{
    return counting_reject_guide_get(code)->title;
}
