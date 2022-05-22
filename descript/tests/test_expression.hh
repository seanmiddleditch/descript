// descript

#pragma once

#include "descript/alloc.hh"
#include "descript/types.hh"
#include "descript/value.hh"

#include "array.hh"
#include "expression_compiler.hh"
#include "fnv.hh"
#include "utility.hh"

#include <ostream>

namespace descript::test::expression {
    struct Function
    {
        char const* name = nullptr;
        dsFunction function = nullptr;
        void* userData = nullptr;
    };

    struct Variable
    {
        char const* name = nullptr;
        dsValue value;
    };

    std::ostream& operator<<(std::ostream& os, dsValue const& value);

    struct Result
    {
        enum class Code
        {
            Success,
            CompileFailed,
            EvalFailed,
            TypeFailed,
            ResultFailed,
        } code = Code::Success;
        dsValue expected;
        dsValue actual;

        /*implicit*/ Result(Code code) noexcept : code(code) {}
        Result(Code code, dsValue const& expected, dsValue const& actual) noexcept : code(code), expected(expected), actual(actual) {}

        explicit operator bool() const noexcept { return code == Code::Success; }

        friend std::ostream& operator<<(std::ostream& os, Result const& result)
        {
            switch (result.code)
            {
            case Result::Code::Success: return os << "Success\nExpected: " << result.expected << "\nResult: " << result.actual;
            case Result::Code::CompileFailed: return os << "Compile Failed\nExpected: " << result.expected;
            case Result::Code::EvalFailed: return os << "Eval Failed\nExpected: " << result.expected;
            case Result::Code::TypeFailed:
                return os << "Type Check Failed\nExpected: " << result.expected << "\nResult: " << result.actual;
            case Result::Code::ResultFailed:
                return os << "Result Failed\nExpected: " << result.expected << "\nResult: " << result.actual;
            default: return os;
            }
        }
    };

    class ExpressionTester final : public dsExpressionCompilerHost, dsExpressionBuilder, dsEvaluateHost
    {
    public:
        explicit ExpressionTester(dsAllocator& alloc, dsSpan<Variable const> variables, dsSpan<Function const> functions) noexcept
            : byteCode_(alloc), constants_(alloc), variables_(variables), functions_(functions), compiler_(alloc, *this, *this)
        {
        }

        Result compile(char const* expression, dsValue const& expected);
        Result compile(char const* expression, double expected) { return compile(expression, dsValue{expected}); }

    protected:
        // dsExpressionCompilerHost
        bool lookupVariable(dsName name) const noexcept override;
        bool lookupFunction(dsName name, dsFunctionId& out_functionId) const noexcept override;

        // dsExpressionBuilder
        void pushOp(uint8_t byte) override { byteCode_.pushBack(byte); }
        dsExpressionConstantIndex pushConstant(dsValue const& value) override;
        dsExpressionFunctionIndex pushFunction(dsFunctionId functionId) override;
        dsExpressionVariableIndex pushVariable(uint64_t nameHash) override;

        // dsEvaluateHost
        void listen(dsEmitterId) override {}
        bool readVariable(dsExpressionVariableIndex variableIndex, dsValue& out_value) override;
        bool readConstant(dsExpressionConstantIndex constantIndex, dsValue& out_value) override;
        bool invokeFunction(dsExpressionFunctionIndex functionIndex, dsFunctionContext& ctx, dsValue& out_result) override;

    private:
        dsArray<uint8_t> byteCode_;
        dsArray<dsValue, dsExpressionConstantIndex> constants_;
        dsSpan<Variable const> variables_;
        dsSpan<Function const> functions_;
        dsExpressionCompiler compiler_;
    };

    std::ostream& operator<<(std::ostream& os, dsValue const& value)
    {
        switch (value.type())
        {
        case dsValueType::Double: return os << "double(" << value.as<double>() << ')';
        default: return os << "unknown(???)";
        }
    }

    Result ExpressionTester::compile(char const* expression, dsValue const& expected)
    {
        uint32_t byteCodeOffset = byteCode_.size();

        if (!compiler_.compile(expression))
            return Result::Code::CompileFailed;
        dsValue result;
        if (!dsEvaluate(*this, byteCode_.data() + byteCodeOffset, byteCode_.size() - byteCodeOffset, result))
            return Result::Code::EvalFailed;
        if (result.type() != expected.type())
            return {Result::Code::TypeFailed, dsValue{expected}, result};
        if (result != expected)
            return {Result::Code::ResultFailed, dsValue{expected}, result};
        return {Result::Code::Success, dsValue{expected}, result};
    };

    bool ExpressionTester::lookupVariable(dsName name) const noexcept
    {
        uint32_t const variableLen = dsNameLen(name);
        for (Variable const& variable : variables_)
            if (variableLen == std::strlen(variable.name) && std::strncmp(name.name, variable.name, variableLen) == 0)
                return true;
        return false;
    }

    bool ExpressionTester::lookupFunction(dsName name, dsFunctionId& out_functionId) const noexcept
    {
        uint32_t const functionLen = dsNameLen(name);
        uint32_t nextFunctionId = 0;
        for (Function const& function : functions_)
        {
            if (functionLen == std::strlen(function.name) && std::strncmp(name.name, function.name, functionLen) == 0)
            {
                out_functionId = dsFunctionId{nextFunctionId};
                return true;
            }
            ++nextFunctionId;
        }
        return false;
    }

    dsExpressionConstantIndex ExpressionTester::pushConstant(dsValue const& value)
    {
        uint32_t const index{constants_.size()};
        constants_.pushBack(value);
        return dsExpressionConstantIndex{index};
    }

    dsExpressionVariableIndex ExpressionTester::pushVariable(uint64_t nameHash)
    {
        uint32_t nextIndex = 0;
        for (Variable const& variable : variables_)
        {
            if (nameHash == dsHashFnv1a64(variable.name))
            {
                return dsExpressionVariableIndex{nextIndex};
            }
            ++nextIndex;
        }
        DS_GUARD_OR(false, dsInvalidIndex, "Invalid variable has, miscompile");
    }

    dsExpressionFunctionIndex ExpressionTester::pushFunction(dsFunctionId functionId)
    {
        return dsExpressionFunctionIndex{static_cast<uint32_t>(functionId.value())};
    }

    bool ExpressionTester::readVariable(dsExpressionVariableIndex variableIndex, dsValue& out_value)
    {
        DS_GUARD_OR(variableIndex.value() < variables_.count, false);
        out_value = variables_.items[variableIndex.value()].value;
        return true;
    }

    bool ExpressionTester::readConstant(dsExpressionConstantIndex constantIndex, dsValue& out_value)
    {
        if (!constants_.contains(constantIndex))
            return false;
        out_value = constants_[constantIndex];
        return true;
    }

    bool ExpressionTester::invokeFunction(dsExpressionFunctionIndex functionIndex, dsFunctionContext& ctx, dsValue& out_result)
    {
        DS_GUARD_OR(functionIndex.value() < functions_.count, false, "Invalid function index, miscompile");

        Function const& func = functions_.items[functionIndex.value()];
        out_result = func.function(ctx, func.userData);
        return true;
    }
} // namespace descript::test::expression
