

#ifndef XDRV_91
#define XDRV_91 91

#include <vector>
#include <list>
#include <TasmotaSerial.h>

namespace {
    TasmotaSerial *Core200SSerial = nullptr;

    class Core200S;
    Core200S* core200s = nullptr;

    enum class FanSpeedMode : uint8_t {
        FAN_SPEED_LOW = 0x1,
        FAN_SPEED_MED = 0x2,
        FAN_SPEED_FULL = 0x3
    };

    const char* to_readable(const FanSpeedMode& mode) {
        switch(mode) {
            case FanSpeedMode::FAN_SPEED_LOW: return "low";
            case FanSpeedMode::FAN_SPEED_MED: return "med";
            case FanSpeedMode::FAN_SPEED_FULL: return "full";
        }
        return "";
    }

    enum class FanMode : uint8_t {
        FAN_OFF = 0x0,
        FAN_ON = 0x1,
    };

    const char* to_readable(const FanMode& mode) {
        switch(mode) {
            case FanMode::FAN_OFF: return "off";
            case FanMode::FAN_ON: return "on";
        }
        return "";
    }

    enum class NightLightMode : uint8_t {
        NL_OFF = 0x0,
        NL_LOW = 0x32,
        NL_FULL = 0x64
    };

    const char* to_readable(const NightLightMode& mode) {
        switch(mode) {
            case NightLightMode::NL_OFF: return "off";
            case NightLightMode::NL_LOW: return "low";
            case NightLightMode::NL_FULL: return "full";
        }
        return "";
    }

    enum class ChildLockMode : uint8_t {
        CL_OFF = 0x0,
        CL_ON = 0x1
    };

    const char* to_readable(const ChildLockMode& mode) {
        switch(mode) {
            case ChildLockMode::CL_OFF: return "off";
            case ChildLockMode::CL_ON: return "on";
        }
        return "";
    }

    enum class SleepModeMode : uint8_t {
        SL_OFF = 0x0,
        SL_ON = 0x1
    };

    const char* to_readable(const SleepModeMode& mode) {
        switch(mode) {
            case SleepModeMode::SL_OFF: return "off";
            case SleepModeMode::SL_ON: return "on";
        }
        return "";
    }

    enum class DisplayMode : uint8_t {
        DISPLAY_OFF = 0x0,
        DISPLAY_ON = 0x64
    };

    const char* to_readable(const DisplayMode& mode) {
        switch(mode) {
            case DisplayMode::DISPLAY_OFF: return "off";
            case DisplayMode::DISPLAY_ON: return "on";
        }
        return "";
    }

    enum class DisplayAutoOff : uint8_t {
        DISPLAY_AUTOOFF_ON = 0x0,
        DISPLAY_AUTOOFF_OFF = 0x1
    };

    const char* to_readable(const DisplayAutoOff& mode) {
        switch(mode) {
            case DisplayAutoOff::DISPLAY_AUTOOFF_OFF: return "off";
            case DisplayAutoOff::DISPLAY_AUTOOFF_ON: return "on";
        }
        return "";
    }

    void printMessage(const char* prefix, const std::vector<uint8_t>& message) {
        std::vector<char> buffer({0});
        size_t i = 0;
        for (auto& m : message) {
            buffer.resize(buffer.size() + 3);
            int n = snprintf(buffer.data() + i*3, 4, "%02x ", m);
            i++;
        }
        AddLog(LOG_LEVEL_DEBUG, PSTR("C2S: %s %s"), prefix, buffer.data());
    }

    uint8_t calcMessageChecksum(const std::vector<uint8_t>& message) {
        uint8_t checksum = 0xff;
        for(size_t index = 0; index < message.size(); index++) {
            // the checksum will be placed at index 5 so we rather ignore it that field
            if (index != 5) {
                checksum -= message[index];
            }
        }
        return checksum;
    }

    class Core200S {
    public:
        void init_mcu() {
            msg_count = 0;
            Core200SSerial->flush();
            delay(1000);

            send({0xa5, 0x22, 0x01, 0x05, 0x00, 0xaa, 0x01, 0xe2, 0xa5, 0x00, 0x00});

            delay(200);
            wait_for_message();

            delay(200);
            send_raw({0xa5, 0x12, 0x00, 0x04, 0x00, 0xa3, 0x01, 0x60, 0x40, 0x00});

            delay(200);
            send({0xa5, 0x22, 0x02, 0x04, 0x00, 0x90, 0x01, 0x61, 0x40, 0x00});

            delay(200);
            set_wifi_led(false);

            initialized = true;

            delay(200);
            queryState();

        }

        void set_wifi_led(bool on) {
            if (initialized && wifi_on != on) {
                std::vector<uint8_t> cmd;
                if (on) {
                    cmd = {0xa5, 0x22, 0x05, 0x0a, 0x00, 0x63, 0x01, 0x29, 0xa1, 0x00, 0x01, 0x7d, 0x00, 0x7d, 0x00, 0x00};
                } else {
                    cmd = {0xa5, 0x22, 0x03, 0x0a, 0x00, 0x76, 0x01, 0x29, 0xa1, 0x00, 0x00, 0xf4, 0x01, 0xf4, 0x01, 0x00};
                }
                send(cmd);
                wifi_on = on;
            }
        }

        void queryState() {
            if (!initialized) {
                return;
            }
            std::vector<uint8_t> status = send({0xa5, 0x22, 0x06, 0x04, 0x00, 0x8c, 0x01, 0x61, 0x40, 0x00});
            parseStatusUpdate(status);
        }

        void parseStatusUpdate(std::vector<uint8_t> message) {
            const std::vector<uint8_t> filter_reset ({0xa5, 0x22, 0x00, 0x05, 0x00, 0xa8, 0x01, 0xe4, 0xa5, 0x00, 0x01});

            // Status update has length of 22 bytes
            if (message.size() == 22) {
                fan_mode = static_cast<FanMode>(message[13]);
                sl_mode = static_cast<SleepModeMode>(message[14]);
                fan_speed_mode = static_cast<FanSpeedMode>(message[15]);
                dp_mode = static_cast<DisplayMode>(message[16]);
                dp_auto_off = static_cast<DisplayAutoOff>(message[17]);
                cl_mode = static_cast<ChildLockMode>(message[20]);
                nl_mode = static_cast<NightLightMode>(message[21]);
            } else if (message == filter_reset) {
                // Filter handling not yet implemented since the filter state is maintained on the
                // esp not the MCU
                AddLog(LOG_LEVEL_INFO, PSTR("C2S: Detected Filter Reset. Not Implemented..."));
            }
        }

        void handleIncomingMessages() {
            if (!initialized) {
                return;
            }
            std::vector<uint8_t> message = wait_for_message(5, true);
            while(!message.empty()) {
                if (message[1] == '\x22') {
                    printMessage("Got incoming message", message);
                    send_raw({0xa5, 0x12, 0x00, 0x04, 0x00, 0xa3, 0x01, 0x60, 0x40, 0x00});
                    parseStatusUpdate(message);
                }
                message = wait_for_message(5, true);
            }
        }

        void handleErrors() {
            if (error_count > 10) {
                error_count = 0;
                init_mcu();
            }
        }

        void jsonAppend() {
            if (!initialized) {
                return;
            }
            ResponseAppend_P(PSTR(",\"Core200S\":{"));

            // Fan Speed Mode
            ResponseAppend_P(PSTR("\"FanSpeedMode\":\""));
            ResponseAppend_P(to_readable(fan_speed_mode));
            ResponseAppend_P(PSTR("\""));

            // Fan Mode
            ResponseAppend_P(PSTR(",\"FanMode\":\""));
            ResponseAppend_P(to_readable(fan_mode));
            ResponseAppend_P(PSTR("\""));

            // Night Light Mode
            ResponseAppend_P(PSTR(",\"NightLightMode\":\""));
            ResponseAppend_P(to_readable(nl_mode));
            ResponseAppend_P(PSTR("\""));

            // Child Lock Mode
            ResponseAppend_P(PSTR(",\"ChildLockMode\":\""));
            ResponseAppend_P(to_readable(cl_mode));
            ResponseAppend_P(PSTR("\""));

            // Sleep Mode
            ResponseAppend_P(PSTR(",\"SleepMode\":\""));
            ResponseAppend_P(to_readable(sl_mode));
            ResponseAppend_P(PSTR("\""));

            // Display Mode
            ResponseAppend_P(PSTR(",\"DisplayMode\":\""));
            ResponseAppend_P(to_readable(dp_mode));
            ResponseAppend_P(PSTR("\""));

            // Display Auto Off
            ResponseAppend_P(PSTR(",\"DisplayAutoOff\":\""));
            ResponseAppend_P(to_readable(dp_auto_off));
            ResponseAppend_P(PSTR("\""));

            ResponseAppend_P(PSTR("}"));
        }

        void showWeb() {
            #ifdef USE_WEBSERVER
            const char* table_row = PSTR("{s} %s {m} %s {e}");
            WSContentSend_P(PSTR("<hr></table>{t}"));
            WSContentSend_P(table_row, "Fan Mode", to_readable(fan_mode));
            WSContentSend_P(table_row, "Fan Speed", to_readable(fan_speed_mode));
            WSContentSend_P(table_row, "Night Light", to_readable(nl_mode));
            WSContentSend_P(table_row, "Child Lock", to_readable(cl_mode));
            WSContentSend_P(table_row, "Sleep Mode", to_readable(sl_mode));
            WSContentSend_P(table_row, "Display", to_readable(dp_mode));
            WSContentSend_P(table_row, "Display Auto Off", to_readable(dp_auto_off));
            #endif
        }

        bool set_fan_speed(FanSpeedMode mode) {
            std::vector<uint8_t> msg = {0xa5, 0x22, 0x07, 0x07, 0x00, 0x25, 0x01, 0x60, 0xa2, 0x00, 0x00, 0x01, static_cast<uint8_t>(mode)};
            return !send(msg).empty();
        }

    private:

        std::vector<uint8_t> send(std::vector<uint8_t> message) {
            // increate and set message counter
            msg_count++;
            message[2] = msg_count;

            // Insert checksum
            message[5] = calcMessageChecksum(message);

            send_raw(message);
            return wait_for_message();
        }

        void send_raw(const std::vector<uint8_t>& message) {
            for (auto b : message) {
                Core200SSerial->write(b);
            }
            printMessage("Send", message);
        }

        std::vector<uint8_t> wait_for_message(size_t remaining_read_attempts = 10, bool expect_no_message = false) {
            // 6 chars is the minimum length to hold all the metadata
            std::vector<uint8_t> message({0, 0, 0, 0, 0, 0});
            delay(50);
            size_t read_bytes = 0; // char read that belong to the answer

            while (message.at(0) != '\xa5' && !(message.at(1) == '\x12' || message.at(1) == '\22')) {
                if (Core200SSerial->available() >= 2) {
                    Core200SSerial->read(message.data(), 1);
                    if (message.at(0) == '\xa5') {
                        // In case of multiple consecutive \xa5 we read until we get something else
                        do {
                            Core200SSerial->read(message.data() + 1, 1);
                        } while(message.at(1) == '\xa5');
                    }
                } else {
                    if (remaining_read_attempts == 0) {
                        if (!expect_no_message) {
                            error_count++;
                            AddLog(LOG_LEVEL_ERROR, PSTR("###### CORE200: Too many read attempts (couldn't read message magic)"));
                        }
                        return {};
                    }
                    remaining_read_attempts--;
                    delay(50);
                }
            }
            // read header data
            read_bytes = 2;
            while (read_bytes != message.size()) {
                if (Core200SSerial->available()) {
                    read_bytes += Core200SSerial->read(message.data() + read_bytes, message.size() - read_bytes);
                } else {
                    if (remaining_read_attempts == 0) {
                        if(!expect_no_message) {
                            AddLog(LOG_LEVEL_ERROR, PSTR("###### CORE200: Too many read attempts (couldn't read message header)"));
                            error_count++;
                        }
                        return {};
                    }
                    remaining_read_attempts--;
                    delay(50);
                }

            }

            printMessage("Received Header", message);

            // Inspect payload length field
            message.resize(6 + message.at(3));

            // Read payload
            while (read_bytes != message.size()) {
                if (Core200SSerial->available()) {
                    read_bytes += Core200SSerial->read(message.data() + read_bytes, message.size() - read_bytes);
                } else {
                    if (remaining_read_attempts == 0) {
                        AddLog(LOG_LEVEL_ERROR, PSTR("###### CORE200: Too many read attempts (got %d bytes, expected %d)"), read_bytes, message.size());
                        error_count++;
                        return {};
                    }
                    remaining_read_attempts--;
                    delay(50);
                }
            }

            printMessage("Received Full Message", message);

            // Validate checksum
            uint8_t checksum = calcMessageChecksum(message);
            if (checksum != message[5]) {
                AddLog(LOG_LEVEL_DEBUG, PSTR("###### CORE200: Invalid Checksum"));
                return {};
            }

            return message;
        }

        uint8_t msg_count = 0;
        bool initialized = false;
        bool wifi_on = false;
        unsigned error_count = 0;

        FanSpeedMode fan_speed_mode = FanSpeedMode::FAN_SPEED_LOW;
        FanMode fan_mode = FanMode::FAN_OFF;
        NightLightMode nl_mode = NightLightMode::NL_OFF;
        ChildLockMode cl_mode = ChildLockMode::CL_OFF;
        SleepModeMode sl_mode = SleepModeMode::SL_OFF;
        DisplayMode dp_mode = DisplayMode::DISPLAY_OFF;
        DisplayAutoOff dp_auto_off = DisplayAutoOff::DISPLAY_AUTOOFF_OFF;
    };

    #define GPIO_CORE200S_RX2 16
    #define GPIO_CORE200S_TX2 17

    void Core200SInit() {
        int baudrate = 115200;
        Core200SSerial = new TasmotaSerial(GPIO_CORE200S_RX2, GPIO_CORE200S_TX2, 0);
        if (Core200SSerial->begin(baudrate)) {
            if (Core200SSerial->hardwareSerial()) {
                ClaimSerial();
            }
        }
    }

} // namespace

bool Xdrv91(uint32_t function) {
    static int count_update = 0;
    bool result = false;

    switch (function) {
    case FUNC_MODULE_INIT:
        if (!core200s) {
            core200s = new Core200S();
        }
        break;
    case FUNC_PRE_INIT:
        Core200SInit();
        break;
    case FUNC_INIT:
        core200s->init_mcu();
        break;
    case FUNC_NETWORK_DOWN:
    case FUNC_NETWORK_UP:
        if (core200s) {
            core200s->set_wifi_led(function == FUNC_NETWORK_UP);
        }
        break;
    case FUNC_EVERY_SECOND:
        if (core200s) {
            core200s->handleErrors();
            core200s->handleIncomingMessages();
            // Once per minute query state manually, just to get in sync again if
            // we for some reason lost a message
            if (count_update == 60) {
                core200s->queryState();
                count_update = 0;
            } else {
                count_update++;
            }
        }
        break;
    case FUNC_JSON_APPEND:
        if (core200s) {
            core200s->jsonAppend();
        }
        break;
    case FUNC_WEB_SENSOR:
        if (core200s) {
            core200s->showWeb();
        }
        break;
    }

    return result;
}

#endif // XDRV_91
