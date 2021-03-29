// descript

#include "descript/uuid.hh"

#include <random>

namespace descript {

    static constexpr uint8_t intToHexLower(int val) noexcept
    {
        if (val < 0 || val >= 36)
            return '?';
        if (val < 10)
            return '0' + val;
        return 'a' + (val - 10);
    };

    dsUuid dsCreateUuid() noexcept
    {
        std::random_device device;
        std::mt19937 engine(device());
        std::uniform_int_distribution<std::mt19937::result_type> dist8(0, 255);

        dsUuid uuid;
        for (uint32_t index = 0; index != dsUuid::length; ++index)
            uuid.bytes[index] = dist8(engine);

        return uuid;
    }

    dsStringUuid dsUuidToString(dsUuid const& uuid) noexcept
    {
        dsStringUuid result;

        // {8-
        result.string[0] = '{';
        for (uint32_t i = 0; i != 4; ++i)
        {
            result.string[i * 2 + 1] = intToHexLower(uuid.bytes[i] >> 4);
            result.string[i * 2 + 2] = intToHexLower(uuid.bytes[i] & 0xf);
        }
        result.string[9] = '-';

        // 4-
        for (uint32_t i = 4; i != 6; ++i)
        {
            result.string[i * 2 + 2] = intToHexLower(uuid.bytes[i] >> 4);
            result.string[i * 2 + 3] = intToHexLower(uuid.bytes[i] & 0xf);
        }
        result.string[14] = '-';

        // 4-
        for (uint32_t i = 6; i != 8; ++i)
        {
            result.string[i * 2 + 3] = intToHexLower(uuid.bytes[i] >> 4);
            result.string[i * 2 + 4] = intToHexLower(uuid.bytes[i] & 0xf);
        }
        result.string[19] = '-';

        // 4-
        for (uint32_t i = 8; i != 10; ++i)
        {
            result.string[i * 2 + 4] = intToHexLower(uuid.bytes[i] >> 4);
            result.string[i * 2 + 5] = intToHexLower(uuid.bytes[i] & 0xf);
        }
        result.string[24] = '-';

        // 12}
        for (uint32_t i = 10; i != 16; ++i)
        {
            result.string[i * 2 + 5] = intToHexLower(uuid.bytes[i] >> 4);
            result.string[i * 2 + 6] = intToHexLower(uuid.bytes[i] & 0xf);
        }
        result.string[37] = '}';

        // nul
        result.string[38] = '\0';

        return result;
    }

} // namespace descript
