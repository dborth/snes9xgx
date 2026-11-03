#ifndef _HIDPARSE_H_
#define _HIDPARSE_H_

/*
 * A small HID report descriptor parser, enough to find a pen tablet's fields.
 * Only used while a tablet is being attached: the parsed table (hidp_desc,
 * ~7 KB) is allocated for the parse and freed again, and the few fields the
 * pen needs are copied out by value (hidp_pen).
 *
 * Every usage is stored as (page << 16) | id, after folding Wacom's vendor
 * page onto the standard pages it mirrors (see hidp_equivalent).
 */

#if defined(HW_RVL) && defined(HAVE_TABLET)

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIDP_MAX_FIELDS  256

#define HIDP_INPUT   0
#define HIDP_FEATURE 1

typedef struct
{
	uint32_t usage;
	uint32_t app;        // the enclosing application collection's usage
	uint8_t type;        // HIDP_INPUT or HIDP_FEATURE
	uint8_t report_id;   // 0 when the device uses no report IDs
	uint16_t bit_offset; // from the start of the report, after the ID byte
	uint8_t bit_size;    // 0: the device has no such field
	int32_t logical_min, logical_max;
} hidp_field;

typedef struct
{
	hidp_field fields[HIDP_MAX_FIELDS];
	int nfields;
	bool uses_ids;
	// Report lengths in bits, without the ID byte, by type and ID
	uint16_t bits[2][256];
} hidp_desc;

// The pen's report, with each field copied out of the hidp_desc
typedef struct
{
	bool uses_ids;
	uint8_t report_id;
	hidp_field x, y, pressure;
	hidp_field tip, barrel, barrel2, eraser, invert, in_range, valid;
	hidp_field input_mode;     // a feature to set to 2 (tablet mode), if any
	uint32_t input_mode_bytes; // the length of its report, ID byte included
} hidp_pen;

// False on a descriptor too broken to use. Fields past HIDP_MAX_FIELDS are
// dropped rather than failing the parse.
bool hidp_parse(const uint8_t *desc, uint32_t len, hidp_desc *out);

// Finds the report carrying the pen: X and Y inside a digitizer, pen or
// stylus collection. False when the descriptor describes no pen.
bool hidp_find_pen(const hidp_desc *d, hidp_pen *pen);

// A field's value from a report, `data` starting after the ID byte.
// 0 for a field the device does not have.
int32_t hidp_get(const hidp_field *f, const uint8_t *data, uint32_t len);
void hidp_put(const hidp_field *f, uint8_t *data, uint32_t len, int32_t value);

#ifdef __cplusplus
}
#endif

#endif // HW_RVL && HAVE_TABLET
#endif
