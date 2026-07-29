#ifndef PORT_PMU_H
#define PORT_PMU_H

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t pmu_init(void);
void pmu_isr_handler(void);

#ifdef __cplusplus
}
#endif

#endif
