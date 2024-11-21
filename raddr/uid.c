#include <stdint.h>
#include <stdio.h>
#include <py32f0xx_hal.h>
#include "uid.h"

#define ARRAY_SIZE(a)  (sizeof(a)/sizeof(a[0]))

#define UID_SIZE    16
union unique_id {
    uint8_t raw_bytes[UID_SIZE];
    uint32_t raw_int[UID_SIZE / sizeof(uint32_t)];
    struct {
        uint8_t lot_number_0;
        uint8_t lot_number_1;
        uint8_t lot_number_2;
        uint8_t lot_number_3;
        uint8_t wafer_number;
        uint8_t lot_number_4;
        uint8_t lot_number_5;
        uint8_t lot_number_6;
        uint8_t internal_encoding_0;
        uint8_t y_low;
        uint8_t x_low;
        uint8_t xy_high;
        uint8_t constant_0x78; //Should always be 0x78
        uint8_t internal_encoding_1;
        uint8_t internal_encoding_2;
        uint8_t internal_encoding_3;
    };
};

#define UID ((union unique_id*)UID_BASE)

void uid_print(void)
{
#if defined(USE_SEMIHOSTING)
    printf("Raw hex: ");
    for( int i= 0; i < ARRAY_SIZE(UID->raw_bytes); i++) {
        printf("%x ", UID->raw_bytes[i]);
    }
    printf("\r\n");

    printf("Lot number: "
            "%c%c%c%c%c%c%c ",
            UID->lot_number_0,
            UID->lot_number_1,
            UID->lot_number_2,
            UID->lot_number_3,
            UID->lot_number_4,
            UID->lot_number_5,
            UID->lot_number_6
            );

    printf("Wafer %d coord: "
            "%dx%d ",
            UID->wafer_number,
            UID->x_low + (((UID->xy_high >> 0) & 0xF) << 8), //Lower nibble
            UID->y_low + (((UID->xy_high >> 4) & 0xF) << 8)  //Upper nibble
            );

    printf("Internal encoding:"
            "%d %d %d %d",
            UID->internal_encoding_0,
            UID->internal_encoding_1,
            UID->internal_encoding_2,
            UID->internal_encoding_3
            );

    printf("\r\n");
#endif
}
