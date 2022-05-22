// descript

#include "test_expression.hh"

#include <catch_amalgamated.hpp>

#include "descript/value.hh"

#include "assembly_internal.hh"
#include "expression.hh"
#include "leak_alloc.hh"

TEST_CASE("Virtual Machine", "[vm]")
{
    using namespace descript;
    using namespace descript::test;
    using namespace descript::test::expression;

    static constexpr Variable variables[] = {
        Variable{"Seven", dsValue{7.0}},
        Variable{"Eleven", dsValue{11.0}},
    };

    static constexpr Function functions[] = {
        Function{"Add",
            [](dsFunctionContext& ctx, void* userData) {
                double result = 0.0;
                for (uint32_t i = 0; i != ctx.argc(); ++i)
                    result += ctx.argAt(i).as<double>();
                return dsValue{result};
            }},
    };

    LeakTestAllocator alloc;
    ExpressionTester tester(alloc, variables, functions);

    SECTION("Constants")
    {
        CHECK(tester.compile("True", dsValue{true}));
        CHECK(tester.compile("False", dsValue{false}));
        CHECK(tester.compile("Nil", dsValue{nullptr}));
        CHECK(tester.compile("0", 0.0));
        CHECK(tester.compile("10", 10.0));
        CHECK(tester.compile("1000", 1'000.0));
        CHECK(tester.compile("1000000", 1'000'000.0));
    }

    SECTION("Negate")
    {
        CHECK(tester.compile("-42", -42.0));
        CHECK(tester.compile("--42", 42.0));
    }

    SECTION("Binary Arithmetic")
    {
        CHECK(tester.compile("1 + 17", 18.0));
        CHECK(tester.compile("-2 * 3", -6.0));
        CHECK(tester.compile("0 - 3", -3.0));
        CHECK(tester.compile("1 / 2", 0.5));
    }

    SECTION("Logical") {
        CHECK(tester.compile("not true", dsValue{false}));
        CHECK(tester.compile("true and false", dsValue{false}));
        CHECK(tester.compile("true or false", dsValue{true}));
        CHECK(tester.compile("true xor true", dsValue{false}));
        CHECK(tester.compile("true and not false", dsValue{true}));
    }

    SECTION("Precedence")
    {
        CHECK(tester.compile("2 + 3 * 4", 14.0));
        CHECK(tester.compile("(2 + 3) * 4", 20.0));
        CHECK(tester.compile("2 - 3 + 4", 3.0));
        CHECK(tester.compile("2 + 3 - 4", 1.0));
        CHECK(tester.compile("10 + 2 * -3 - (1 + 1)", 2.0));
    }

    SECTION("Variable")
    {
        CHECK(tester.compile("Seven", 7.0));
        CHECK(tester.compile("-Eleven", -11.0));
        CHECK(tester.compile("Seven + Eleven", 18.0));
        CHECK(tester.compile("Seven + 1", 8.0));
    }

    SECTION("Call")
    {
        CHECK(tester.compile("Add()", 0.0));
        CHECK(tester.compile("Add(1)", 1.0));
        CHECK(tester.compile("-Add(1, 1)", -2.0));
        CHECK(tester.compile("Add(1) + 1", 2.0));
        CHECK(tester.compile("Add(1, 1) * Add(2, 3)", 10.0));
        CHECK(tester.compile("Add(1, Add(2, 3), -2)", 4.0));
        CHECK(tester.compile("Add(17, 99 - 50) + -42", 24.0));
        CHECK(tester.compile("Add(Seven, 0, Eleven)", 18.0));
    }
}
