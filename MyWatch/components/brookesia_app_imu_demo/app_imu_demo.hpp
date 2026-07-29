#ifndef BROOKESIA_APP_IMU_DEMO_HPP
#define BROOKESIA_APP_IMU_DEMO_HPP

#include "systems/phone/esp_brookesia_phone.hpp"

#ifdef __cplusplus
extern "C" {
#endif

namespace esp_brookesia::apps {

class IMUDemo : public systems::phone::App {
public:
    static IMUDemo *requestInstance();
    virtual ~IMUDemo();

    bool run(void) override;
    bool back(void) override;

private:
    IMUDemo();

    static IMUDemo *_instance;
};

}

#ifdef __cplusplus
}
#endif

#endif
