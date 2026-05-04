#include "arm/arm64/arm64.h"
#include <redasm/redasm.h>

void rd_plugin_create(void) {
    rd_register_processor(&ARM64_LE);
    rd_register_processor(&ARM64_BE);
}
