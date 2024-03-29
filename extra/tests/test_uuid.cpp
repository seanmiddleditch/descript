// descript

#include <catch_amalgamated.hpp>

#include "descript/uuid.hh"

#include <cstring>

//static_assert(descript::dsParseUuid("{18bc4316-fb86-466e-80d3-07ac0ea5cc68}"));

namespace descript {
    std::ostream& operator<<(std::ostream& os, dsUuid const& uuid);
}

TEST_CASE("UUID Conversion", "[uuid]")
{
    using namespace descript;

    constexpr char uuidStr[] = "{a335f2af-7103-4761-99db-84a015812e9b}";

    constexpr dsUuid uuid = dsParseUuid("{a335f2af-7103-4761-99db-84a015812e9b}");
    CHECK(uuid);

    auto const str = dsUuidToString(uuid);

    CHECK(strcmp(str.string, uuidStr) == 0);
}

TEST_CASE("UUID Parse errors", "[uuid]")
{
    constexpr char uuidStr[] = "{a335f2af-7103-4761-99db-84a015812e9b}";
    constexpr char invalidCharsStr[] = "asdflkjaiuyeklajshdf";

    using namespace descript;

    constexpr dsUuid expectedUuid = dsParseUuid("{a335f2af-7103-4761-99db-84a015812e9b}");
    dsUuid uuid = dsParseUuid(uuidStr, nullptr);
    CHECK(uuid == expectedUuid);

    uuid = dsParseUuid(uuidStr, uuidStr + 1);
    CHECK(uuid != expectedUuid);

    uuid = dsParseUuid(invalidCharsStr, invalidCharsStr + sizeof(invalidCharsStr));
    CHECK_FALSE(uuid);
}

std::ostream& descript::operator<<(std::ostream& os, dsUuid const& uuid) {
    auto const str = dsUuidToString(uuid);
    return os << str.string;
}
