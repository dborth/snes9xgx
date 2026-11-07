#include <string.h>
#include "tabletdesc.h"

#define MAX_USAGES 64
#define MAX_DEPTH  16
#define MAX_PUSH   4

// Standard usages
#define TABLETDESC_GD_X              0x00010030u
#define TABLETDESC_GD_Y              0x00010031u
#define TABLETDESC_DG_DIGITIZER      0x000D0001u
#define TABLETDESC_DG_PEN            0x000D0002u
#define TABLETDESC_DG_STYLUS         0x000D0020u
#define TABLETDESC_DG_TIP_PRESSURE   0x000D0030u
#define TABLETDESC_DG_IN_RANGE       0x000D0032u
#define TABLETDESC_DG_INVERT         0x000D003Cu
#define TABLETDESC_DG_TIP_SWITCH     0x000D0042u
#define TABLETDESC_DG_BARREL         0x000D0044u
#define TABLETDESC_DG_ERASER         0x000D0045u
#define TABLETDESC_DG_INPUT_MODE     0x000D0052u
#define TABLETDESC_DG_BARREL2        0x000D005Au

// Wacom's own usages that have no standard equivalent
#define TABLETDESC_WD_DISTANCE       0xFF0D0132u
#define TABLETDESC_WD_REPORT_VALID   0xFF0D01D0u

typedef struct
{
	uint32_t page;
	int32_t logical_min;
	uint32_t max_raw; // Logical Maximum as written, and its size
	int max_size;
	uint32_t report_size, report_count;
	uint8_t report_id;
} globals;

// Wacom's 0xFF0D page repeats the standard ones: 0xFF0D00xx is the digitizer
// page, 0xFF0D01xx generic desktop, 0xFF0D09xx buttons, and so on. The ones
// Wacom kept to itself are left alone, as Linux's wacom_equivalent_usage has it.
static uint32_t equivalent(uint32_t usage)
{
	if ((usage & 0xFFFF0000u) != 0xFF0D0000u)
		return usage;

	uint32_t subpage = (usage & 0xFF00u) << 8;
	uint32_t id = usage & 0xFFu;
	if (subpage == 0x00040000u || subpage == 0x00090000u ||
	    subpage == 0x000D0000u || subpage == 0x00100000u ||
	    usage == 0xFF0D0036u || usage == 0xFF0D005Cu ||
	    usage == 0xFF0D0077u || usage == TABLETDESC_WD_DISTANCE ||
	    usage == 0xFF0D0136u || usage == 0xFF0D0137u ||
	    usage == 0xFF0D0138u || usage == 0xFF0D0139u ||
	    usage == TABLETDESC_WD_REPORT_VALID || usage == 0xFF0D005Du ||
	    usage == 0xFF0D0220u)
		return usage;

	if (subpage == 0)
		subpage = 0x000D0000u;
	return subpage | id;
}

static uint32_t item_u(const uint8_t *p, int size)
{
	switch (size)
	{
		case 1: return p[0];
		case 2: return (uint32_t)(p[0] | (p[1] << 8));
		case 4: return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		               ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	}
	return 0;
}

static int32_t item_s(const uint8_t *p, int size)
{
	switch (size)
	{
		case 1: return (int8_t)p[0];
		case 2: return (int16_t)(p[0] | (p[1] << 8));
		case 4: return (int32_t)item_u(p, 4);
	}
	return 0;
}

// Logical Maximum reads as signed only when the minimum is negative: 0x25 0xFF
// means 255 next to a minimum of 0, and -1 next to a minimum of -128.
static int32_t logical_max(const globals *g)
{
	if (g->logical_min >= 0)
		return (int32_t)g->max_raw;
	if (g->max_size == 1)
		return (int8_t)g->max_raw;
	if (g->max_size == 2)
		return (int16_t)g->max_raw;
	return (int32_t)g->max_raw;
}

bool TabletDesc_Parse(const uint8_t *desc, uint32_t len, tabletdesc_table *out)
{
	globals g, stack[MAX_PUSH];
	int sp = 0;
	uint32_t usages[MAX_USAGES];
	int nusages = 0;
	uint32_t umin = 0, umax = 0;
	bool have_min = false, have_max = false;
	uint32_t apps[MAX_DEPTH];
	int depth = 0;
	uint32_t off = 0;

	memset(out, 0, sizeof(*out));
	memset(&g, 0, sizeof(g));

	while (off < len)
	{
		uint8_t b = desc[off];

		if (b == 0xFE)
		{
			// Long item: never used by real devices; step over it
			if (off + 2 >= len)
				return false;
			off += 3u + desc[off + 1];
			continue;
		}

		int size = b & 3;
		if (size == 3)
			size = 4;
		int type = (b >> 2) & 3;
		int tag = b >> 4;
		const uint8_t *data = desc + off + 1;
		if (off + 1 + size > len)
			return false;
		off += 1u + size;

		if (type == 1) // global
		{
			switch (tag)
			{
				case 0x0: g.page = item_u(data, size); break;
				case 0x1: g.logical_min = item_s(data, size); break;
				case 0x2:
					g.max_raw = item_u(data, size);
					g.max_size = size;
					break;
				case 0x7: g.report_size = item_u(data, size); break;
				case 0x8:
					g.report_id = (uint8_t)item_u(data, size);
					out->uses_ids = true;
					break;
				case 0x9: g.report_count = item_u(data, size); break;
				case 0xA:
					if (sp < MAX_PUSH)
						stack[sp++] = g;
					break;
				case 0xB:
					if (sp > 0)
						g = stack[--sp];
					break;
			}
			continue;
		}

		if (type == 2) // local
		{
			uint32_t u = item_u(data, size);

			if (size <= 2 && tag <= 2)
				u |= g.page << 16;
			switch (tag)
			{
				case 0x0:
					if (nusages < MAX_USAGES)
						usages[nusages++] = u;
					break;
				case 0x1: umin = u; have_min = true; break;
				case 0x2: umax = u; have_max = true; break;
			}
			continue;
		}

		if (type != 0)
			continue;

		// Usage Minimum/Maximum: expand into the list
		if (have_min && have_max && umax >= umin)
		{
			for (uint32_t u = umin; u <= umax && nusages < MAX_USAGES; u++)
				usages[nusages++] = u;
		}

		switch (tag)
		{
			case 0x8:   // Input
			case 0xB: { // Feature
				uint8_t kind = tag == 0x8 ? TABLETDESC_INPUT : TABLETDESC_FEATURE;
				uint16_t *bits = &out->bits[kind][g.report_id];
				uint32_t flags = item_u(data, size);
				uint32_t count = g.report_count;

				if (g.report_size > 32 ||
				    (uint64_t)g.report_size * g.report_count > 0xFFFFu - *bits)
					return false;

				// A long run under one usage is a blob (firmware data,
				// calibration); keep only its first element so it does
				// not crowd the table.
				if (count > (uint32_t)nusages && count > 32)
					count = 1;

				// Variable, non-constant fields only; arrays (keyboards'
				// key lists) are stepped over.
				if ((flags & 0x03) == 0x02 && nusages > 0)
				{
					for (uint32_t i = 0; i < count; i++)
					{
						uint32_t u = equivalent(
							usages[i < (uint32_t)nusages ? i : (uint32_t)nusages - 1]);

						// Of the features, only the mode switch is used;
						// tablets have dozens of others.
						if (kind == TABLETDESC_FEATURE && u != TABLETDESC_DG_INPUT_MODE)
							continue;
						if (out->nfields >= TABLETDESC_MAX_FIELDS)
							break;
						tabletdesc_field *f = &out->fields[out->nfields++];
						f->usage = u;
						f->app = depth > 0 ? apps[depth - 1] : 0;
						f->type = kind;
						f->report_id = g.report_id;
						f->bit_offset = (uint16_t)(*bits + i * g.report_size);
						f->bit_size = (uint8_t)g.report_size;
						f->logical_min = g.logical_min;
						f->logical_max = logical_max(&g);
					}
				}
				*bits = (uint16_t)(*bits + g.report_size * g.report_count);
				break;
			}
			case 0xA: { // Collection
				uint32_t u = nusages ? equivalent(usages[0]) : 0;
				uint32_t app = depth > 0 ? apps[depth - 1] : 0;

				if (depth >= MAX_DEPTH)
					return false;
				// Only application collections name the device
				apps[depth++] = (item_u(data, size) == 0x01) ? u : app;
				break;
			}
			case 0xC: // End Collection
				if (depth > 0)
					depth--;
				break;
		}

		nusages = 0;
		have_min = have_max = false;
	}

	return true;
}

// The first field of a type with a usage, optionally in one report (-1 for
// any), copied into `out`; left zeroed (bit_size 0) when there is none.
static bool find(const tabletdesc_table *d, uint8_t type, uint32_t usage, int report_id, tabletdesc_field *out)
{
	for (int i = 0; i < d->nfields; i++)
	{
		const tabletdesc_field *f = &d->fields[i];

		if (f->type == type && f->usage == usage &&
		    (report_id < 0 || f->report_id == report_id))
		{
			*out = *f;
			return true;
		}
	}
	return false;
}

static bool pen_app(uint32_t app)
{
	return app == TABLETDESC_DG_DIGITIZER || app == TABLETDESC_DG_PEN || app == TABLETDESC_DG_STYLUS;
}

bool TabletDesc_FindPen(const tabletdesc_table *d, tabletdesc_pen *pen)
{
	const tabletdesc_field *x = NULL;
	tabletdesc_field unused;

	memset(pen, 0, sizeof(*pen));

	for (int i = 0; i < d->nfields && !x; i++)
	{
		const tabletdesc_field *f = &d->fields[i];

		if (f->type == TABLETDESC_INPUT && f->usage == TABLETDESC_GD_X && pen_app(f->app) &&
		    find(d, TABLETDESC_INPUT, TABLETDESC_GD_Y, f->report_id, &unused))
			x = f;
	}
	if (!x)
		return false;

	int id = x->report_id;
	pen->uses_ids = d->uses_ids;
	pen->report_id = x->report_id;
	pen->x = *x;
	find(d, TABLETDESC_INPUT, TABLETDESC_GD_Y, id, &pen->y);
	find(d, TABLETDESC_INPUT, TABLETDESC_DG_TIP_PRESSURE, id, &pen->pressure);
	find(d, TABLETDESC_INPUT, TABLETDESC_DG_TIP_SWITCH, id, &pen->tip);
	find(d, TABLETDESC_INPUT, TABLETDESC_DG_BARREL, id, &pen->barrel);
	find(d, TABLETDESC_INPUT, TABLETDESC_DG_BARREL2, id, &pen->barrel2);
	find(d, TABLETDESC_INPUT, TABLETDESC_DG_ERASER, id, &pen->eraser);
	find(d, TABLETDESC_INPUT, TABLETDESC_DG_INVERT, id, &pen->invert);
	find(d, TABLETDESC_INPUT, TABLETDESC_DG_IN_RANGE, id, &pen->in_range);
	find(d, TABLETDESC_INPUT, TABLETDESC_WD_REPORT_VALID, id, &pen->valid);
	if (find(d, TABLETDESC_FEATURE, TABLETDESC_DG_INPUT_MODE, -1, &pen->input_mode))
	{
		pen->input_mode_bytes = (d->bits[TABLETDESC_FEATURE][pen->input_mode.report_id] + 7u) / 8u +
			(d->uses_ids ? 1u : 0u);
	}
	return true;
}

int32_t TabletDesc_Get(const tabletdesc_field *f, const uint8_t *data, uint32_t len)
{
	uint32_t v = 0;

	for (unsigned i = 0; i < f->bit_size; i++)
	{
		uint32_t bit = f->bit_offset + i;

		if ((bit >> 3) >= len)
			break;
		v |= (uint32_t)((data[bit >> 3] >> (bit & 7)) & 1) << i;
	}

	if (f->logical_min < 0 && f->bit_size > 0 && f->bit_size < 32 &&
	    (v & (1u << (f->bit_size - 1))))
		v |= ~0u << f->bit_size;
	return (int32_t)v;
}

void TabletDesc_Put(const tabletdesc_field *f, uint8_t *data, uint32_t len, int32_t value)
{
	for (unsigned i = 0; i < f->bit_size; i++)
	{
		uint32_t bit = f->bit_offset + i;
		uint8_t mask = (uint8_t)(1u << (bit & 7));

		if ((bit >> 3) >= len)
			break;
		if (((uint32_t)value >> i) & 1)
			data[bit >> 3] |= mask;
		else
			data[bit >> 3] &= (uint8_t)~mask;
	}
}
