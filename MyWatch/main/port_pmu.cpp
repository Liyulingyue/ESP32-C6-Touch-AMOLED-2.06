#include "port_pmu.h"
#include "port_i2c.h"
#include "esp_log.h"

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

static const char *TAG = "PMU";

static int pmu_register_read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
{
    if (i2c_bus_handle == NULL) {
        return -1;
    }
    i2c_master_dev_handle_t dev_handle;
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = devAddr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &dev_handle);

    uint8_t reg = regAddr;
    esp_err_t ret = i2c_master_transmit_receive(dev_handle, &reg, 1, data, len, 1000);
    i2c_master_bus_rm_device(dev_handle);
    return ret == ESP_OK ? 0 : -1;
}

static int pmu_register_write_byte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
{
    if (i2c_bus_handle == NULL) {
        return -1;
    }
    i2c_master_dev_handle_t dev_handle;
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = devAddr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &dev_handle);

    uint8_t *buffer = (uint8_t *)malloc(len + 1);
    if (!buffer) return -1;
    buffer[0] = regAddr;
    memcpy(&buffer[1], data, len);

    esp_err_t ret = i2c_master_transmit(dev_handle, buffer, len + 1, 1000);
    free(buffer);
    i2c_master_bus_rm_device(dev_handle);
    return ret == ESP_OK ? 0 : -1;
}

static XPowersPMU PMU;

esp_err_t pmu_init(void)
{
    if (!PMU.begin(AXP2101_SLAVE_ADDRESS, pmu_register_read, pmu_register_write_byte)) {
        ESP_LOGE(TAG, "Failed to init PMU");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "PMU AXP2101 initialized successfully");

    PMU.disableDC2();
    PMU.disableDC3();
    PMU.disableDC4();
    PMU.disableDC5();
    PMU.disableALDO1();
    PMU.disableALDO2();
    PMU.disableALDO3();
    PMU.disableALDO4();
    PMU.disableBLDO1();
    PMU.disableBLDO2();
    PMU.disableCPUSLDO();
    PMU.disableDLDO1();
    PMU.disableDLDO2();

    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();
    PMU.enableTemperatureMeasure();
    PMU.disableTSPinMeasure();

    PMU.clearIrqStatus();
    PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    PMU.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ | XPOWERS_AXP2101_BAT_REMOVE_IRQ |
        XPOWERS_AXP2101_VBUS_INSERT_IRQ | XPOWERS_AXP2101_VBUS_REMOVE_IRQ |
        XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ |
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ | XPOWERS_AXP2101_BAT_CHG_START_IRQ
    );

    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_400MA);
    PMU.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

    ESP_LOGI(TAG, "Battery: %d%%", PMU.getBatteryPercent());

    return ESP_OK;
}

void pmu_isr_handler(void)
{
    if (PMU.getIrqStatus()) {
        ESP_LOGI(TAG, "isCharging: %s", PMU.isCharging() ? "YES" : "NO");
        ESP_LOGI(TAG, "isDischarge: %s", PMU.isDischarge() ? "YES" : "NO");
        ESP_LOGI(TAG, "isVbusIn: %s", PMU.isVbusIn() ? "YES" : "NO");
        ESP_LOGI(TAG, "getBattVoltage: %d mV", PMU.getBattVoltage());
        if (PMU.isBatteryConnect()) {
            ESP_LOGI(TAG, "Battery: %d%%", PMU.getBatteryPercent());
        }
        PMU.clearIrqStatus();
    }
}
