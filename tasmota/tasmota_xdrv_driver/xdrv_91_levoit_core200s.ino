

#ifndef XDRV_91
#define XDRV_91 91

#include <vector>
#include <tuple>
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

    auto convertSecondsToHHMMSS(uint32_t seconds) {
        uint32_t hours = seconds / 3600;
        seconds -= hours*3600;

        uint32_t minutes = seconds / 60;
        seconds -= minutes*60;

        return std::make_tuple(hours, minutes, seconds);
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

        void queryTimer() {
            if (!initialized) {
                return;
            }
            std::vector<uint8_t> state = send({0xa5, 0x22, 0x07, 0x04, 0x00, 0x25, 0x01, 0x65, 0xa2, 0x00});
            if (!state.empty()) {
                timer_remaining = state.at(10) | state.at(11) << 8 | state.at(12) << 16;
                timer_total = state.at(14) | state.at(15) << 8 | state.at(16) << 16;
            }
        }

        bool set_timer(uint32_t seconds) {
            // check it timer if set and don't try to clean timer if not set
            // (The stock firmware also checks if timer is set before it cleans it)
            queryTimer();
            delay(50);
            if (seconds == 0 && timer_total == 0) {
                return true;
            }
            AddLog(LOG_LEVEL_INFO, PSTR("C2S: Set timer to %ds"), seconds);
            uint8_t byte3 = seconds & 0xFF;
            uint8_t byte2 = (seconds >> 8) & 0xFF;
            uint8_t byte1 = (seconds >> 16) & 0xFF;
            std::vector<uint8_t> res = send({0xa5, 0x22, 0x08, 0x08, 0x00, 0x21, 0x01, 0x64, 0xa2, 0x00, byte3, byte2, byte1, 0x00});

            return !res.empty();
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

                // When timer is set it send a status message (but not a message that actually containes the timer)
                // Thus on each status update we should query if a timer has been set
                delay(50);
                queryTimer();
            } else if (message == filter_reset) {
                // Filter handling not yet implemented since the filter state is maintained on the
                // esp not the MCU
                AddLog(LOG_LEVEL_INFO, PSTR("C2S: Detected Filter Reset. Not Implemented..."));
            } else {
                printMessage("Couldn't parse meassge ", message);
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

            // Timer
            ResponseAppend_P(PSTR(",\"Timer\": {"));
            ResponseAppend_P(PSTR("\"Remaining\":%d"), timer_remaining);
            ResponseAppend_P(PSTR(",\"Total\":%d"), timer_total);
            ResponseAppend_P(PSTR("}"));

            ResponseAppend_P(PSTR("}"));
        }

        void showWeb() {
            #ifdef USE_WEBSERVER
            const char* table_row = PSTR("{s} %s {m} %s {e}");
            const char* table_row_timer = PSTR("{s} %s {m} %02d:%02d:%02d {e}");
            WSContentSend_P(PSTR("<hr></table>{t}"));
            WSContentSend_P(table_row, "Fan Mode", to_readable(fan_mode));
            WSContentSend_P(table_row, "Fan Speed", to_readable(fan_speed_mode));
            WSContentSend_P(table_row, "Night Light", to_readable(nl_mode));
            WSContentSend_P(table_row, "Child Lock", to_readable(cl_mode));
            WSContentSend_P(table_row, "Sleep Mode", to_readable(sl_mode));
            WSContentSend_P(table_row, "Display", to_readable(dp_mode));
            WSContentSend_P(table_row, "Display Auto Off", to_readable(dp_auto_off));
            if (timer_total != 0) {
                auto total_split = convertSecondsToHHMMSS(timer_total);
                WSContentSend_P(table_row_timer, "Timer Total", std::get<0>(total_split), std::get<1>(total_split), std::get<2>(total_split));

                auto remaining_split = convertSecondsToHHMMSS(timer_remaining);
                WSContentSend_P(table_row_timer, "Timer Remaining", std::get<0>(remaining_split), std::get<1>(remaining_split), std::get<2>(remaining_split));
            } else {
                WSContentSend_P(table_row, "Timer", "not active");
            }
            #endif
        }

        bool set_fan_speed(FanSpeedMode mode) {
            std::vector<uint8_t> res = send({0xa5, 0x22, 0x07, 0x07, 0x00, 0x25, 0x01, 0x60, 0xa2, 0x00, 0x00, 0x01, static_cast<uint8_t>(mode)});
            return !res.empty();
        }

        bool toggle_fan_speed() {
            FanSpeedMode mode;
            switch(fan_speed_mode) {
                case FanSpeedMode::FAN_SPEED_LOW:
                    mode = FanSpeedMode::FAN_SPEED_MED;
                    break;
                case FanSpeedMode::FAN_SPEED_MED:
                    mode = FanSpeedMode::FAN_SPEED_FULL;
                    break;
                case FanSpeedMode::FAN_SPEED_FULL:
                    mode = FanSpeedMode::FAN_SPEED_LOW;
                    break;
            }
            return set_fan_speed(mode);
        }

        bool set_fan_mode(FanMode mode) {
            std::vector<uint8_t> res = send({0xa5, 0x22, 0x07, 0x05, 0x00, 0x8a, 0x01, 0x00, 0xa0, 0x00, static_cast<uint8_t>(mode)});
            return !res.empty();
        }

        bool toggle_fan_mode() {
            FanMode mode;
            switch (fan_mode) {
                case FanMode::FAN_ON:
                    mode = FanMode::FAN_OFF;
                    break;
                case FanMode::FAN_OFF:
                    mode = FanMode::FAN_ON;
                    break;
            }
            return set_fan_mode(mode);
        }

        bool set_night_light_mode(NightLightMode mode) {
            std::vector<uint8_t> res = send({0xa5, 0x22, 0x09, 0x06, 0x00, 0x21, 0x01, 0x03, 0xa0, 0x00, 0x00, static_cast<uint8_t>(mode)});
            return !res.empty();
        }

        bool toggle_night_light() {
            NightLightMode mode;
            switch (nl_mode) {
                case NightLightMode::NL_OFF:
                    mode = NightLightMode::NL_FULL;
                    break;
                case NightLightMode::NL_LOW:
                    mode = NightLightMode::NL_OFF;
                    break;
                case NightLightMode::NL_FULL:
                    mode = NightLightMode::NL_LOW;
                    break;
            }
            return set_night_light_mode(mode);
        }

        bool set_child_lock_mode(ChildLockMode mode) {
            std::vector<uint8_t> res = send({0xa5, 0x22, 0x07, 0x05, 0x00, 0x5a, 0x01, 0x00, 0xd1, 0x00, static_cast<uint8_t>(mode)});
            return !res.empty();
        }

        bool toggle_child_lock_mode() {
            ChildLockMode mode;
            switch (cl_mode) {
                case ChildLockMode::CL_ON:
                    mode = ChildLockMode::CL_OFF;
                    break;
                case ChildLockMode::CL_OFF:
                    mode = ChildLockMode::CL_ON;
                    break;
            }
            return set_child_lock_mode(mode);
        }

        bool set_sleep_mode(SleepModeMode mode) {
            std::vector<uint8_t> res;
            switch (mode) {
                case SleepModeMode::SL_ON:
                    res = send({0xa5, 0x22, 0x07, 0x05, 0x00, 0xa5, 0x01, 0xe0, 0xa5, 0x00, 0x01});
                    break;
                case SleepModeMode::SL_OFF:
                    return set_fan_speed(fan_speed_mode);
            }
            return !res.empty();
        }

        bool toggle_sleep_mode() {
            SleepModeMode mode;
            switch(sl_mode) {
                case SleepModeMode::SL_ON:
                    mode = SleepModeMode::SL_OFF;
                    break;
                case SleepModeMode::SL_OFF:
                    mode = SleepModeMode::SL_ON;
                    break;
            }
            return set_sleep_mode(mode);
        }

        bool set_display_autooff_mode(DisplayAutoOff mode) {
            std::vector<uint8_t> res;
            switch (mode) {
                case DisplayAutoOff::DISPLAY_AUTOOFF_OFF:
                    res = send({0xa5, 0x22, 0x07, 0x05, 0x00, 0x21, 0x01, 0x05, 0xa1, 0x00, 0x64});
                    break;
                case DisplayAutoOff::DISPLAY_AUTOOFF_ON:
                    res = send({0xa5, 0x22, 0x07, 0x05, 0x00, 0x85, 0x01, 0x05, 0xa1, 0x00, 0x00});
                    break;
            }
            return !res.empty();
        }

        bool toggle_display_autooff_mode() {
            DisplayAutoOff mode;
            switch (dp_auto_off) {
                case DisplayAutoOff::DISPLAY_AUTOOFF_OFF:
                    mode = DisplayAutoOff::DISPLAY_AUTOOFF_ON;
                    break;
                case DisplayAutoOff::DISPLAY_AUTOOFF_ON:
                    mode = DisplayAutoOff::DISPLAY_AUTOOFF_OFF;
                    break;
            }
            return set_display_autooff_mode(mode);
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

    public:
        uint32_t timer_remaining = 0;
        uint32_t timer_total = 0;
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


#define D_PRFX_CORE200S "C2S_"
#define D_CMND_CORE200S_FAN_SPEED "fan_speed"
#define D_CMND_CORE200S_FAN_MODE "fan_mode"
#define D_CMND_CORE200S_NIGHT_LIGHT "night_light"
#define D_CMND_CORE200S_CHILD_LOCK "child_lock"
#define D_CMND_CORE200S_SLEEP_MODE "sleep_mode"
#define D_CMND_CORE200S_DISPLAY_AUTOOFF "display_autooff"
#define D_CMND_CORE200S_SET_TIMER "set_timer"
#define D_CMND_CORE200S_GET_TIMER "get_timer"

const char kTasmotaCore200SCommands[] PROGMEM = D_PRFX_CORE200S "|"
    D_CMND_CORE200S_FAN_SPEED "|" D_CMND_CORE200S_FAN_MODE "|" D_CMND_CORE200S_NIGHT_LIGHT "|" D_CMND_CORE200S_CHILD_LOCK "|" D_CMND_CORE200S_SLEEP_MODE "|" D_CMND_CORE200S_DISPLAY_AUTOOFF "|" D_CMND_CORE200S_SET_TIMER "|" D_CMND_CORE200S_GET_TIMER;

enum class C2SCmdArgument : uint_fast8_t {
    ARG_FULL,
    ARG_MED,
    ARG_LOW,
    ARG_OFF,
    ARG_ON,
    ARG_TOGGLE,
    PARSE_ERROR
};

auto parse_argument() {
    switch (XdrvMailbox.data_len) {
        case 6:
            if (strncmp(XdrvMailbox.data, "toggle", 6) == 0) return C2SCmdArgument::ARG_TOGGLE;
        case 4:
            if (strncmp(XdrvMailbox.data, "full", 4) == 0) return C2SCmdArgument::ARG_FULL;
            break;
        case 3:
            if (strncmp(XdrvMailbox.data, "med", 3) == 0) return C2SCmdArgument::ARG_MED;
            else if (strncmp(XdrvMailbox.data, "low", 3) == 0) return C2SCmdArgument::ARG_LOW;
            else if (strncmp(XdrvMailbox.data, "off", 3) == 0) return C2SCmdArgument::ARG_OFF;
            break;
        case 2:
            if (strncmp(XdrvMailbox.data, "on", 2) == 0) return C2SCmdArgument::ARG_ON;
            break;
    }
    return C2SCmdArgument::PARSE_ERROR;
}

void handle_fan_speed_cmd() {
    if (core200s) {
        bool success = false;
        switch(parse_argument()) {
            case C2SCmdArgument::ARG_FULL:
                success = core200s->set_fan_speed(FanSpeedMode::FAN_SPEED_FULL);
                break;
            case C2SCmdArgument::ARG_MED:
                success = core200s->set_fan_speed(FanSpeedMode::FAN_SPEED_MED);
                break;
            case C2SCmdArgument::ARG_LOW:
                success = core200s->set_fan_speed(FanSpeedMode::FAN_SPEED_LOW);
                break;
            case C2SCmdArgument::ARG_TOGGLE:
                success = core200s->toggle_fan_speed();
                break;
            default:
                return;
        }

        if (success) {
            Response_P(PSTR("{\"%s\": \"success\"}"), XdrvMailbox.command);
        }
    }
}

void handle_fan_mode_cmd() {
    if (core200s) {
        bool success = false;
        switch(parse_argument()) {
            case C2SCmdArgument::ARG_ON:
                success = core200s->set_fan_mode(FanMode::FAN_ON);
                break;
            case C2SCmdArgument::ARG_OFF:
                success = core200s->set_fan_mode(FanMode::FAN_OFF);
                break;
            case C2SCmdArgument::ARG_TOGGLE:
                success = core200s->toggle_fan_mode();
                break;
            default:
                return;
        }

        if (success) {
            Response_P(PSTR("{\"%s\": \"success\"}"), XdrvMailbox.command);
        }
    }
}

void handle_night_light_cmd() {
    if (core200s) {
        bool success = false;
        switch(parse_argument()) {
            case C2SCmdArgument::ARG_OFF:
                success = core200s->set_night_light_mode(NightLightMode::NL_OFF);
                break;
            case C2SCmdArgument::ARG_LOW:
                success = core200s->set_night_light_mode(NightLightMode::NL_LOW);
                break;
            case C2SCmdArgument::ARG_FULL:
                success = core200s->set_night_light_mode(NightLightMode::NL_FULL);
                break;
            case C2SCmdArgument::ARG_TOGGLE:
                success = core200s->toggle_night_light();
                break;
            default:
                return;
        }

        if (success) {
            Response_P(PSTR("{\"%s\": \"success\"}"), XdrvMailbox.command);
        }
    }
}

void handle_child_lock_cmd() {
    if (core200s) {
        bool success = false;
        switch(parse_argument()) {
            case C2SCmdArgument::ARG_ON:
                success = core200s->set_child_lock_mode(ChildLockMode::CL_ON);
                break;
            case C2SCmdArgument::ARG_OFF:
                success = core200s->set_child_lock_mode(ChildLockMode::CL_OFF);
                break;
            case C2SCmdArgument::ARG_TOGGLE:
                success = core200s->toggle_child_lock_mode();
                break;
            default:
                return;
        }

        if (success) {
            Response_P(PSTR("{\"%s\": \"success\"}"), XdrvMailbox.command);
        }
    }
}

void handle_sleep_mode_cmd() {
    if (core200s) {
        bool success = false;
        switch(parse_argument()) {
            case C2SCmdArgument::ARG_ON:
                success = core200s->set_sleep_mode(SleepModeMode::SL_ON);
                break;
            case C2SCmdArgument::ARG_OFF:
                success = core200s->set_sleep_mode(SleepModeMode::SL_OFF);
                break;
            case C2SCmdArgument::ARG_TOGGLE:
                success = core200s->toggle_sleep_mode();
                break;
            default:
                return;
        }

        if (success) {
            Response_P(PSTR("{\"%s\": \"success\"}"), XdrvMailbox.command);
        }
    }
}

void handle_display_autooff_cmd() {
    if (core200s) {
        bool success = false;
        switch(parse_argument()) {
            case C2SCmdArgument::ARG_ON:
                success = core200s->set_display_autooff_mode(DisplayAutoOff::DISPLAY_AUTOOFF_ON);
                break;
            case C2SCmdArgument::ARG_OFF:
                success = core200s->set_display_autooff_mode(DisplayAutoOff::DISPLAY_AUTOOFF_OFF);
                break;
            case C2SCmdArgument::ARG_TOGGLE:
                success = core200s->toggle_display_autooff_mode();
                break;
            default:
                return;
        }

        if (success) {
            Response_P(PSTR("{\"%s\": \"success\"}"), XdrvMailbox.command);
        }
    }
}

void handle_set_timer() {
    if (core200s) {
       auto seconds = std::strtoul(XdrvMailbox.data, nullptr, 0);
       if (seconds >= 24*60*60) {
            // timer only supports max value of 24h - 1s
            return;
       }
       if (core200s->set_timer(seconds)) {
            Response_P(PSTR("{\"%s\": \"success\"}"), XdrvMailbox.command);
       }
    }
}

void handle_get_timer() {
    if (core200s) {
        Response_P(PSTR("{\"%s\": \"success\", \"timer_total\":%d, \"timer_remaining\":%d }"), XdrvMailbox.command, core200s->timer_total, core200s->timer_remaining);
    }
}

void (* const TasmotaCore200SCommand[])(void) PROGMEM = {
    &handle_fan_speed_cmd, &handle_fan_mode_cmd, &handle_night_light_cmd, &handle_child_lock_cmd, &handle_sleep_mode_cmd, &handle_display_autooff_cmd, &handle_set_timer, &handle_get_timer
};

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

            if (count_update % 5 == 0) {
                core200s->queryTimer();
            }

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
    case FUNC_COMMAND:
        result = DecodeCommand(kTasmotaCore200SCommands, TasmotaCore200SCommand);
        break;
    }

    return result;
}

#endif // XDRV_91
