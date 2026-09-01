#include "arm/arm32/arm32.h"
#include "arm/arm32/thumb.h"
#include "arm/arm64.h"
#include "mos65xx/mos6502.h"
// #include "xtensa/xtensa.h"
#include <capstone/capstone.h>
#include <redasm/redasm.h>

#define capstonebundle_register(arch, ...)                                     \
    do {                                                                       \
        if(cs_support(arch)) {                                                 \
            __VA_ARGS__                                                        \
        }                                                                      \
        else                                                                   \
            RD_LOG_WARN("missing " #arch " support, skipping registration");   \
    } while(0)

static void capstone_module_load(void) {
    capstonebundle_register(CS_ARCH_ARM, {
        rd_register_processor(&THUMB_LE);
        rd_register_processor(&THUMB_BE);

        rd_register_processor(&ARM32_LE);
        rd_register_processor(&ARM32_BE);
    });

    capstonebundle_register(CS_ARCH_AARCH64, {
        rd_register_processor(&ARM64_LE);
        rd_register_processor(&ARM64_BE);
    });

    capstonebundle_register(CS_ARCH_MOS65XX,
                            { rd_register_processor(&MOS6502); });

    /* capstone 6.0.0-alpha10 => Xtensa support is broken, so:
     * - don't test and register the processor plugin for now
     * - 'next' seems working
     */

    // capstonebundle_register(CS_ARCH_XTENSA, {
    //     rd_register_processor(&XTENSA_ESP8266);
    //     rd_register_processor(&XTENSA_ESP32);
    //     rd_register_processor(&XTENSA_ESP32S2);
    // });
}

RD_MODULE_EXPORT = {
    .api_version = RD_API_VERSION,
    .load = capstone_module_load,
};
