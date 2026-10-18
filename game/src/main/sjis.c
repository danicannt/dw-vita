#include <string.h>

#include <dw/sjis.h>
#include <dw/types.h>

typedef struct {
	uint16_t shiftJIS;
	uint16_t ascii;
} Ascii2ShiftJIS;

Ascii2ShiftJIS CHAR_MAPPING_NORMAL[3] = {
	{ 0x824f, 0x0030 },
	{ 0x8260, 0x0041 },
	{ 0x8281, 0x0061 },
};

Ascii2ShiftJIS CHAR_MAPPING_SPECIAL[33] = {
	{ 0x8140, 0x0020 },
	{ 0x8149, 0x0021 },
	{ 0x8168, 0x0022 },
	{ 0x8194, 0x0023 },
	{ 0x8190, 0x0024 },
	{ 0x8193, 0x0025 },
	{ 0x8195, 0x0026 },
	{ 0x8166, 0x0027 },
	{ 0x8169, 0x0028 },
	{ 0x816a, 0x0029 },
	{ 0x8196, 0x002a },
	{ 0x817b, 0x002b },
	{ 0x8143, 0x002c },
	{ 0x817c, 0x002d },
	{ 0x8142, 0x002e },
	{ 0x815e, 0x002f },
	{ 0x8146, 0x003a },
	{ 0x8147, 0x003b },
	{ 0x8171, 0x003c },
	{ 0x8181, 0x003d },
	{ 0x8172, 0x003e },
	{ 0x8148, 0x003f },
	{ 0x8197, 0x0040 },
	{ 0x816d, 0x005b },
	{ 0x815f, 0x005c },
	{ 0x816e, 0x005d },
	{ 0x814f, 0x005e },
	{ 0x8151, 0x005f },
	{ 0x8165, 0x0060 },
	{ 0x816f, 0x007b },
	{ 0x8162, 0x007c },
	{ 0x8170, 0x007d },
	{ 0x8150, 0x007e },
};

int32_t isAsciiEncoded(const char *value)
{
	if ((*value >> 7) == 0) {
		return 1;
	}

	return 0;
}

// clang-format off
int32_t convertAsciiToJis(input)
	uint8_t input;
// clang-format on
{
	uint8_t charType;
	uint8_t specialOffset;
	int32_t offset;

	specialOffset = 0;

	if ((input >= 0x20) && (input <= 0x2f)) {
		specialOffset = 0x1;
	} else if ((input >= 0x30) && (input <= 0x39)) {
		charType = 0x0;
	} else if ((input >= 0x3a) && (input <= 0x40)) {
		specialOffset = 0xb;
	} else if ((input >= 0x41) && (input <= 0x5a)) {
		charType = 0x1;
	} else if ((input >= 0x5b) && (input <= 0x60)) {
		specialOffset = 0x25;
	} else if ((input >= 0x61) && (input <= 0x7a)) {
		charType = 0x2;
	} else if ((input >= 0x7b) && (input <= 0x7e)) {
		specialOffset = 0x3f;
	} else {
		return 0;
	}

	if (specialOffset != 0) {
		offset = (input - 0x20) - (specialOffset - 1);
		return CHAR_MAPPING_SPECIAL[offset].shiftJIS;
	} else {
		return (uint16_t)(input +
		                  CHAR_MAPPING_NORMAL[charType].shiftJIS -
		                  CHAR_MAPPING_NORMAL[charType].ascii);
	}
}

void asciiToShiftJIS(uint8_t *src, uint16_t *dst)
{
	int32_t i;
	int32_t len;
	uint16_t jis;

	len = strlen(src);

	for (i = 0; i < len; i++) {
		jis = convertAsciiToJis(src[i]);
		dst[i] = ((jis >> 8) & 0xff) | (((uint32_t)jis & 0xff) << 8);
	}

	dst[i] = 0;
}

int32_t swapShortBytes(uint16_t input)
{
	return ((input >> 8) | (input << 8)) & 0xffff;
}
