// descript

#include <catch_amalgamated.hpp>

#include "descript/value.hh"

#include "assembly_internal.hh"
#include "expression.hh"
#include "expression_compiler.hh"
#include "leak_alloc.hh"
#include "utility.hh"

using namespace descript;

static dsValue add(dsFunctionContext& ctx, void* userData)
{
    double result = 0.0;
    for (uint32_t i = 0; i != ctx.argc(); ++i)
        result += ctx.argAt(i).as<double>();
    return dsValue{result};
}

namespace {
    struct TestCompiled
    {
        explicit TestCompiled(dsAllocator& alloc) noexcept : byteCode(alloc), constants(alloc), variables(alloc), functions(alloc) {}

        dsArray<uint8_t> byteCode;
        dsArray<dsValue, dsExpressionConstantIndex> constants;
        dsArray<dsValue, dsExpressionVariableIndex> variables;
        dsArray<dsFunctionId, dsExpressionFunctionIndex> functions;
    };

    class TestExprCompilerHost final : public dsExpressionCompilerHost
    {
    public:
        bool lookupVariable(dsName name) const noexcept override;
        bool lookupFunction(dsName name, dsFunctionId& out_functionId) const noexcept override;
    };

    class TestExprBuilder final : public dsExpressionBuilder
    {
    public:
        explicit TestExprBuilder(TestCompiled& compiled) noexcept : compiled_(compiled) {}

        void pushOp(uint8_t byte) override { compiled_.byteCode.pushBack(byte); }
        dsExpressionConstantIndex pushConstant(dsValue const& value) override
        {
            uint32_t const index{compiled_.constants.size()};
            compiled_.constants.pushBack(value);
            return dsExpressionConstantIndex{index};
        }
        dsExpressionFunctionIndex pushFunction(dsFunctionId functionId) override
        {
            uint32_t const index{compiled_.functions.size()};
            compiled_.functions.pushBack(functionId);
            return dsExpressionFunctionIndex{index};
        }
        dsExpressionVariableIndex pushVariable(uint64_t nameHash) override { return dsExpressionVariableIndex{0}; }

    private:
        TestCompiled& compiled_;
    };

    class TestEvalHost final : public dsEvaluateHost
    {
    public:
        explicit TestEvalHost(TestCompiled const& compiled) noexcept : compiled_(compiled) {}

        void listen(dsEmitterId) override {}

        bool readVariable(dsExpressionVariableIndex variableIndex, dsValue& out_value) override
        {
            if (!compiled_.variables.contains(variableIndex))
                return false;
            out_value = compiled_.variables[variableIndex];
            return true;
        }

        bool readConstant(dsExpressionConstantIndex constantIndex, dsValue& out_value) override
        {
            if (!compiled_.constants.contains(constantIndex))
                return false;
            out_value = compiled_.constants[constantIndex];
            return true;
        }

        bool invokeFunction(dsExpressionFunctionIndex functionIndex, dsFunctionContext& ctx, dsValue& out_result) override
        {
            if (functionIndex.value() < dsCountOf(functionImpls))
            {
                dsAssemblyFunctionImpl const& func = functionImpls[functionIndex.value()];
                out_result = func.function(ctx, func.userData);
                return true;
            }
            return false;
        }

    private:
        static constexpr dsAssemblyFunctionImpl functionImpls[] = {{add, nullptr}};

        TestCompiled const& compiled_;
    };
} // namespace

TEST_CASE("Virtual Machine", "[vm]")
{
    using namespace descript;

    dsFunction const functions[] = {add};
    constexpr uint32_t functionCount = dsCountOf(functions);

    test::LeakTestAllocator alloc;
    TestCompiled compiled(alloc);
    TestEvalHost evalHost(compiled);
    TestExprCompilerHost compilerHost;
    TestExprBuilder builder(compiled);
    dsExpressionCompiler compiler(alloc, compilerHost, builder);

    SECTION("Constants")
    {
        REQUIRE(compiler.compile("1"));

        dsValue result;

        CHECK(dsEvaluate(evalHost, compiled.byteCode.data(), compiled.byteCode.size(), result));
        REQUIRE(result.type() == dsValueType::Double);
        CHECK(result.as<double>() == 1.0);
    }

    SECTION("Negate")
    {
        REQUIRE(compiler.compile("-42"));

        dsValue result;

        CHECK(dsEvaluate(evalHost, compiled.byteCode.data(), compiled.byteCode.size(), result));
        REQUIRE(result.type() == dsValueType::Double);
        CHECK(result.as<double>() == -42.0);
    }

    SECTION("Add")
    {
        REQUIRE(compiler.compile("1 + 17"));

        dsValue result;

        CHECK(dsEvaluate(evalHost, compiled.byteCode.data(), compiled.byteCode.size(), result));
        REQUIRE(result.type() == dsValueType::Double);
        CHECK(result.as<double>() == 18.0);
    }

    SECTION("Precedence")
    {
        REQUIRE(compiler.compile("10 + 2 * -3 - (1 + 1)"));

        dsValue result;

        CHECK(dsEvaluate(evalHost, compiled.byteCode.data(), compiled.byteCode.size(), result));
        REQUIRE(result.type() == dsValueType::Double);
        CHECK(result.as<double>() == 2.0);
    }

    SECTION("Variable")
    {
        REQUIRE(compiler.compile("-var * 3"));

        compiled.variables.pushBack(dsValue{7.0});

        dsValue result;

        CHECK(dsEvaluate(evalHost, compiled.byteCode.data(), compiled.byteCode.size(), result));
        REQUIRE(result.type() == dsValueType::Double);
        CHECK(result.as<double>() == -21.0);
    }

    SECTION("Call")
    {
        REQUIRE(compiler.compile("Add(17, 99 - 50) + -42"));

        dsValue result;

        CHECK(dsEvaluate(evalHost, compiled.byteCode.data(), compiled.byteCode.size(), result));
        REQUIRE(result.type() == dsValueType::Double);
        CHECK(result.as<double>() == 24.0);
    }
}

bool TestExprCompilerHost::lookupVariable(dsName name) const noexcept
{
    uint32_t const variableLen = dsNameLen(name);
    if (variableLen == 3 && std::strncmp("var", name.name, variableLen) == 0)
        return true;
    return false;
}

bool TestExprCompilerHost::lookupFunction(dsName name, dsFunctionId& out_functionId) const noexcept
{
    uint32_t const functionLen = dsNameLen(name);
    if (functionLen == 3 && std::strncmp("Add", name.name, functionLen) == 0)
    {
        out_functionId = dsFunctionId{0};
        return true;
    }
    return false;
}
