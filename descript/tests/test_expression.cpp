// descript

#include "test_expression.hh"

#include "descript/context.hh"
#include "descript/evaluate.hh"
#include "descript/value.hh"

#include "assembly_internal.hh"
#include "leak_alloc.hh"

#include <catch_amalgamated.hpp>

TEST_CASE("Virtual Machine", "[vm]")
{
    using namespace descript;
    using namespace descript::test;
    using namespace descript::test::expression;

    static const Variable variables[] = {
        Variable{.name = "Seven", .value = dsValueStorage{7}},
        Variable{.name = "Eleven", .value = dsValueStorage{11}},
    };

    static constexpr Function functions[] = {
        Function{.name = "Add",
            .returnType = dsType<int32_t>.typeId,
            .function =
                [](dsFunctionContext& ctx, void* userData) {
                    int32_t result = 0;
                    for (uint32_t i = 0; i != ctx.getArgCount(); ++i)
                        result += ctx.getArgAt<int32_t>(i);
                    ctx.result(result);
                }},
    };

    LeakTestAllocator alloc;
    ExpressionTester tester(alloc, variables, functions);

    SECTION("Constants")
    {
        CHECK(tester.run("True", true));
        CHECK(tester.run("False", false));
        CHECK(tester.run("Nil", nullptr));
        CHECK(tester.run("0", 0));
        CHECK(tester.run("10", 10));
        CHECK(tester.run("1000", 1'000));
        CHECK(tester.run("1000000", 1'000'000));
    }

    SECTION("Negate")
    {
        CHECK(tester.run("-42", -42));
        CHECK(tester.run("--42", 42));
    }

    SECTION("Binary Arithmetic")
    {
        CHECK(tester.run("1 + 17", 18));
        CHECK(tester.run("-2 * 3", -6));
        CHECK(tester.run("0 - 3", -3));
        CHECK(tester.run("1 / 2", 0));
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
        CHECK(tester.run("2 + 3 * 4", 14));
        CHECK(tester.run("(2 + 3) * 4", 20));
        CHECK(tester.run("2 - 3 + 4", 3));
        CHECK(tester.run("2 + 3 - 4", 1));
        CHECK(tester.run("10 + 2 * -3 - (1 + 1)", 2));
    }

    SECTION("Variable")
    {
        CHECK(tester.run("Seven", 7));
        CHECK(tester.run("-Eleven", -11));
        CHECK(tester.run("Seven + Eleven", 18));
        CHECK(tester.run("Seven + 1", 8));
    }

    SECTION("Call")
    {
        CHECK(tester.run("Add()", 0));
        CHECK(tester.run("Add(1)", 1));
        CHECK(tester.run("-Add(1, 1)", -2));
        CHECK(tester.run("Add(1) + 1", 2));
        CHECK(tester.run("Add(1, 1) * Add(2, 3)", 10));
        CHECK(tester.run("Add(1, Add(2, 3), -2)", 4));
        CHECK(tester.run("Add(17, 99 - 50) + -42", 24));
        CHECK(tester.run("Add(Seven, 0, Eleven)", 18));
    }

    SECTION("Type errors") { CHECK_FALSE(tester.compile("1 + true", dsType<void>.typeId)); }

    SECTION("Constant optimization")
    {
        CHECK(tester.constant("10", 10));
        CHECK(tester.constant("(10 + 10)", 20));
        CHECK(tester.constant("true or false", true));
        CHECK_FALSE(tester.constant("Seven", 7));
    }

    SECTION("Only variable")
    {
        CHECK(tester.variable("Seven", dsType<int32_t>.typeId));
        CHECK_FALSE(tester.variable("7", dsType<int32_t>.typeId));
    }
}
