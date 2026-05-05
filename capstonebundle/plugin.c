#include "arm/arm32.h"
#include "arm/arm64.h"
#include "arm/thumb.h"
#include <redasm/redasm.h>

void rd_plugin_create(void) {
    rd_register_processor(&THUMB_LE);
    rd_register_processor(&THUMB_BE);

    rd_register_processor(&ARM32_LE);
    rd_register_processor(&ARM32_BE);

    rd_register_processor(&ARM64_LE);
    rd_register_processor(&ARM64_BE);
}
