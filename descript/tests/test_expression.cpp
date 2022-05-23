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
        Function{"Add", dsValueType::Double,
            [](dsFunctionContext& ctx, void* userData) -> dsValue {
                double result = 0.0;
                for (uint32_t i = 0; i != ctx.argc(); ++i)
                    result += ctx.argAt(i).as<double>();
                return result;
            }},
    };

    LeakTestAllocator alloc;
    ExpressionTester tester(alloc, variables, functions);

    SECTION("Constants")
    {
        CHECK(tester.run("True", true));
        CHECK(tester.run("False", false));
        CHECK(tester.run("Nil", nullptr));
        CHECK(tester.run("0", 0.0));
        CHECK(tester.run("10", 10.0));
        CHECK(tester.run("1000", 1'000.0));
        CHECK(tester.run("1000000", 1'000'000.0));
    }

    SECTION("Negate")
    {
        CHECK(tester.run("-42", -42.0));
        CHECK(tester.run("--42", 42.0));
    }

    SECTION("Binary Arithmetic")
    {
        CHECK(tester.run("1 + 17", 18.0));
        CHECK(tester.run("-2 * 3", -6.0));
        CHECK(tester.run("0 - 3", -3.0));
        CHECK(tester.run("1 / 2", 0.5));
    }

    SECTION("Logical")
    {
        CHECK(tester.run("not true", false));
        CHECK(tester.run("true and false", false));
        CHECK(tester.run("true or false", true));
        CHECK(tester.run("true xor true", false));
        CHECK(tester.run("true and not false", true));
    }

    SECTION("Precedence")
    {
        CHECK(tester.run("2 + 3 * 4", 14.0));
        CHECK(tester.run("(2 + 3) * 4", 20.0));
        CHECK(tester.run("2 - 3 + 4", 3.0));
        CHECK(tester.run("2 + 3 - 4", 1.0));
        CHECK(tester.run("10 + 2 * -3 - (1 + 1)", 2.0));
    }

    SECTION("Variable")
    {
        CHECK(tester.run("Seven", 7.0));
        CHECK(tester.run("-Eleven", -11.0));
        CHECK(tester.run("Seven + Eleven", 18.0));
        CHECK(tester.run("Seven + 1", 8.0));
    }

    SECTION("Call")
    {
        CHECK(tester.run("Add()", 0.0));
        CHECK(tester.run("Add(1)", 1.0));
        CHECK(tester.run("-Add(1, 1)", -2.0));
        CHECK(tester.run("Add(1) + 1", 2.0));
        CHECK(tester.run("Add(1, 1) * Add(2, 3)", 10.0));
        CHECK(tester.run("Add(1, Add(2, 3), -2)", 4.0));
        CHECK(tester.run("Add(17, 99 - 50) + -42", 24.0));
        CHECK(tester.run("Add(Seven, 0, Eleven)", 18.0));
    }

    SECTION("Type errors") { CHECK_FALSE(tester.compile("1 + true", {})); }

    SECTION("Constant optimization")
    {
        CHECK(tester.constant("10", 10.0));
        CHECK(tester.constant("(10 + 10)", 20.0));
        CHECK(tester.constant("true or false", true));
        CHECK_FALSE(tester.constant("Seven", 7.0));
    }

    SECTION("Only variable") {
        CHECK(tester.variable("Seven", dsValueType::Double));
        CHECK_FALSE(tester.variable("7", dsValueType::Double));
    }
}
