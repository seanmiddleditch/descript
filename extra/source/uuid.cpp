// descript

#include "descript/uuid.hh"

#include <cstring>
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

    dsUuid dsParseUuid(char const* string, char const* stringEnd) noexcept
    {
        dsUuid result;

        if (string == nullptr)
            return result;

        if (stringEnd == nullptr)
            stringEnd = string + std::strlen(string);

        bool hasBraces = false;
        if (string != stringEnd && *string == '{')
        {
            hasBraces = true;
            ++string;
        }

        uint32_t nibbleIndex = 0;
        while (string != stringEnd && nibbleIndex != (dsUuid::length << 1))
        {
            char const ch = *string++;

            // FIXME: check that the - is in a valid location?
            if (ch == '-')
                continue;

            int const value = (ch >= '0' && ch <= '9')   ? ch - '0'
                              : (ch >= 'a' && ch <= 'f') ? 10 + (ch - 'a')
                              : (ch >= 'A' && ch <= 'F') ? 10 + (ch - 'A')
                                                         : -1;
            if (value == -1)
                return dsUuid{};

            if ((nibbleIndex & 1) == 0)
                result.bytes[nibbleIndex++ >> 1] = (value << 4);
            else
                result.bytes[nibbleIndex++ >> 1] |= value;
        }

        if (hasBraces)
        {
            if (string == stringEnd || *string != '}')
                return dsUuid{};
            ++string;
        }

        if (string != stringEnd)
            return dsUuid{};

        return result;
    }

} // namespace descript
