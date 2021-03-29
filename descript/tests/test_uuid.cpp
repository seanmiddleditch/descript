// descript

#include <catch_amalgamated.hpp>

#include "descript/uuid.hh"

#include <cstring>

static_assert(descript::dsParseUuid("{18bc4316-fb86-466e-80d3-07ac0ea5cc68}"));

TEST_CASE("UUID Conversion", "[uuid]")
{
    using namespace descript;

    constexpr char uuidStr[] = "{a335f2af-7103-4761-99db-84a015812e9b}";

    constexpr dsUuid uuid = dsParseUuid(uuidStr);

    CHECK(uuid);

    auto const str = dsUuidToString(uuid);

    CHECK(strcmp(str.string, uuidStr) == 0);
}
