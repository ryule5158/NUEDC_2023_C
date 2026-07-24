#ifndef MSP_LITO_G3507_RUNTIME_CONFIG_H
#define MSP_LITO_G3507_RUNTIME_CONFIG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Apply the one board setting that TI SysConfig cannot represent directly:
 * SPI0 Motorola 4-wire framing with an external GPIO software chip select.
 */
void MSP_LITO_G3507_ApplyRuntimeConfig(void);

/* Return true when SPI0 contains the hardware-validated frame format. */
bool MSP_LITO_G3507_IsFpgaSpiConfigValid(void);

#ifdef __cplusplus
}
#endif

#endif /* MSP_LITO_G3507_RUNTIME_CONFIG_H */
