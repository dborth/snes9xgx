#ifndef _TABLETDESC_H_
#define _TABLETDESC_H_

/*
 * Reads a pen tablet's HID report descriptor, only as far as needed to find
 * the pen's fields. It is not a general HID parser. Only used while a tablet
 * is being attached: the parsed table (tabletdesc_table, ~7 KB) is allocated
 * for the parse and freed again, and the few fields the pen needs are copied
 * out by value (tabletdesc_pen).
 *
 * Every usage is stored as (page << 16) | id, after folding Wacom's vendor
 * page onto the standard pages it mirrors (see equivalent()).
 */

#ifdef HW_RVL

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TABLETDESC_MAX_FIELDS  256

#define TABLETDESC_INPUT   0
#define TABLETDESC_FEATURE 1

typedef struct
{
	uint32_t usage;
	uint32_t app;        // the enclosing application collection's usage
	uint8_t type;        // TABLETDESC_INPUT or TABLETDESC_FEATURE
	uint8_t report_id;   // 0 when the device uses no report IDs
	uint16_t bit_offset; // from the start of the report, after the ID byte
	uint8_t bit_size;    // 0: the device has no such field
	int32_t logical_min, logical_max;
} tabletdesc_field;

typedef struct
{
	tabletdesc_field fields[TABLETDESC_MAX_FIELDS];
	int nfields;
	bool uses_ids;
	// Report lengths in bits, without the ID byte, by type and ID
	uint16_t bits[2][256];
} tabletdesc_table;

// The pen's report, with each field copied out of the tabletdesc_table
typedef struct
{
	bool uses_ids;
	uint8_t report_id;
	tabletdesc_field x, y, pressure;
	tabletdesc_field tip, barrel, barrel2, eraser, invert, in_range, valid;
	tabletdesc_field input_mode;     // a feature to set to 2 (tablet mode), if any
	uint32_t input_mode_bytes; // the length of its report, ID byte included
} tabletdesc_pen;

// False on a descriptor too broken to use. Fields past TABLETDESC_MAX_FIELDS are
// dropped rather than failing the parse.
bool TabletDesc_Parse(const uint8_t *desc, uint32_t len, tabletdesc_table *out);

// Finds the report carrying the pen: X and Y inside a digitizer, pen or
// stylus collection. False when the descriptor describes no pen.
bool TabletDesc_FindPen(const tabletdesc_table *d, tabletdesc_pen *pen);

// A field's value from a report, `data` starting after the ID byte.
// 0 for a field the device does not have.
int32_t TabletDesc_Get(const tabletdesc_field *f, const uint8_t *data, uint32_t len);
void TabletDesc_Put(const tabletdesc_field *f, uint8_t *data, uint32_t len, int32_t value);

#ifdef __cplusplus
}
#endif

#endif // HW_RVL
#endif
