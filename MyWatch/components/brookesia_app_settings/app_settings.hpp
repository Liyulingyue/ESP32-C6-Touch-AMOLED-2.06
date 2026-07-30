#pragma once

#include "systems/phone/esp_brookesia_phone.hpp"

namespace esp_brookesia::apps {

class SettingsApp : public systems::phone::App {
public:
    SettingsApp();
    ~SettingsApp() override;

    bool run() override;
    bool back() override;

private:
    struct Impl;
    Impl *impl;
};

}
