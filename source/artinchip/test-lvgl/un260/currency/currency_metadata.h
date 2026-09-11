#ifndef UN260_CURRENCY_METADATA_H
#define UN260_CURRENCY_METADATA_H

/* Presentation metadata only. NULL means use the neutral currency icon;
 * never substitute the previous active currency's symbol. */
const char *currency_metadata_symbol(const char *code);

#endif
