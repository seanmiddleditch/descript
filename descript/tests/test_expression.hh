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
        dsValueType returnType = dsValueType::Nil;
        dsFunction function = nullptr;
        void* userData = nullptr;
    };

    struct Variable
    {
        char const* name = nullptr;
        dsValue value;
    };

    std::ostream& operator<<(std::ostream& os, dsValue const& value);
    std::ostream& operator<<(std::ostream& os, dsValueType type);

    struct CompileResult
    {
        enum class Code
        {
            Success,
            CompileFailed,
            OptimizeFailed,
            NotVariableOnly,
            TypeFailed,
        } code = Code::Success;
        dsValueType expected = dsValueType::Nil;
        dsValueType actual = dsValueType::Nil;

        CompileResult(Code code, dsValueType expected) noexcept : code(code), expected(expected) {}
        CompileResult(Code code, dsValueType expected, dsValueType actual) noexcept : code(code), expected(expected), actual(actual) {}

        explicit operator bool() const noexcept { return code == Code::Success; }

        friend std::ostream& operator<<(std::ostream& os, CompileResult const& result)
        {
            switch (result.code)
            {
            case Code::Success: return os << "Success\nExpected: " << result.expected << "\nResult: " << result.actual;
            case Code::CompileFailed: return os << "Compile Failed\nExpected: " << result.expected;
            case Code::OptimizeFailed: return os << "Optimize Failed\nExpected: " << result.expected;
            case Code::NotVariableOnly: return os << "Variable Only Failed\nExpected: " << result.expected;
            case Code::TypeFailed: return os << "Type Check Failed\nExpected: " << result.expected;
            default: return os;
            }
        }
    };

    struct RunResult
    {
        enum class Code
        {
            Success,
            CompileFailed,
            OptimizeFailed,
            BuildFailed,
            OptimizedBuildFailed,
            EvalFailed,
            OptimizedEvalFailed,
            OptimizedResultFailed,
            NotConstant,
            ResultFailed,
        } code = Code::Success;
        dsValue expected;
        dsValue actual;

        RunResult(Code code, dsValue const& expected) noexcept : code(code), expected(expected) {}
        RunResult(Code code, dsValue const& expected, dsValue const& actual) noexcept : code(code), expected(expected), actual(actual) {}

        explicit operator bool() const noexcept { return code == Code::Success; }

        friend std::ostream& operator<<(std::ostream& os, RunResult const& result)
        {
            switch (result.code)
            {
            case Code::Success: return os << "Success\nExpected: " << result.expected << "\nResult: " << result.actual;
            case Code::CompileFailed: return os << "Compile Failed\nExpected: " << result.expected;
            case Code::OptimizeFailed: return os << "Optimize Failed\nExpected: " << result.expected;
            case Code::BuildFailed: return os << "Build Failed\nExpected: " << result.expected;
            case Code::OptimizedBuildFailed: return os << "Build (Optimized) Failed\nExpected: " << result.expected;
            case Code::EvalFailed: return os << "Eval Failed\nExpected: " << result.expected;
            case Code::OptimizedEvalFailed: return os << "Eval (Optimized) Failed\nExpected: " << result.expected;
            case Code::ResultFailed: return os << "Result Failed\nExpected: " << result.expected << "\nResult: " << result.actual;
            case Code::NotConstant: return os << "Not Constant";
            case Code::OptimizedResultFailed:
                return os << "Result (Optimized) Failed\nExpected: " << result.expected << "\nResult: " << result.actual;
            default: return os;
            }
        }
    };

    class ExpressionTester final : public dsExpressionCompilerHost, dsExpressionBuilder, dsEvaluateHost
    {
    public:
        explicit ExpressionTester(dsAllocator& alloc, dsSpan<Variable const> variables, dsSpan<Function const> functions) noexcept
            : byteCode_(alloc), constants_(alloc), variables_(variables), functions_(functions), compiler_(alloc, *this)
        {
        }

        CompileResult compile(char const* expression, dsValueType expectedType);
        CompileResult variable(char const* expression, dsValueType expectedType);
        RunResult run(char const* expression, dsValue const& expected);
        RunResult constant(char const* expression, dsValue const& expected);

    protected:
        // dsExpressionCompilerHost
        bool lookupVariable(dsName name, dsValueType& out_type) const noexcept override;
        bool lookupFunction(dsName name, dsFunctionId& out_functionId, dsValueType& out_type) const noexcept override;

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
        case dsValueType::Nil: return os << "nil";
        case dsValueType::Int32: return os << "int32(" << value.as<int32_t>() << ')';
        case dsValueType::Float32: return os << "float32(" << value.as<float>() << ')';
        case dsValueType::Bool: return os << (value.as<bool>() ? "true" : "false");
        default: return os << "unknown(???)";
        }
    }

    std::ostream& operator<<(std::ostream& os, dsValueType type)
    {
        switch (type)
        {
        case dsValueType::Nil: return os << "nil";
        case dsValueType::Int32: return os << "int32";
        case dsValueType::Float32: return os << "float32";
        case dsValueType::Bool: return os << "bool";
        default: return os << "unknown";
        }
    }

    CompileResult ExpressionTester::compile(char const* expression, dsValueType expectedType)
    {
        if (!compiler_.compile(expression))
            return CompileResult{CompileResult::Code::CompileFailed, expectedType};
        if (!compiler_.optimize())
            return CompileResult{CompileResult::Code::OptimizeFailed, expectedType};
        return CompileResult{CompileResult::Code::Success, expectedType, compiler_.resultType()};
    }

    CompileResult ExpressionTester::variable(char const* expression, dsValueType expectedType)
    {
        if (!compiler_.compile(expression))
            return CompileResult{CompileResult::Code::CompileFailed, expectedType};
        if (!compiler_.optimize())
            return CompileResult{CompileResult::Code::OptimizeFailed, expectedType};
        if (!compiler_.isVariableOnly())
            return CompileResult{CompileResult::Code::NotVariableOnly, expectedType, compiler_.resultType()};
        return CompileResult{CompileResult::Code::Success, expectedType, compiler_.resultType()};
    }

    RunResult ExpressionTester::run(char const* expression, dsValue const& expected)
    {
        if (!compiler_.compile(expression))
            return RunResult{RunResult::Code::CompileFailed, expected};

        uint32_t const byteCodeOffset = byteCode_.size();
        if (!compiler_.build(*this))
            return RunResult{RunResult::Code::BuildFailed, expected};
        dsValue result;
        if (!dsEvaluate(*this, byteCode_.data() + byteCodeOffset, byteCode_.size() - byteCodeOffset, result))
            return RunResult{RunResult::Code::EvalFailed, expected};
        if (result != expected)
            return RunResult{RunResult::Code::ResultFailed, expected, result};

        uint32_t const optimizedByteCodeOffset = byteCode_.size();

        if (!compiler_.optimize())
            return RunResult{RunResult::Code::OptimizeFailed, expected};
        if (!compiler_.build(*this))
            return RunResult{RunResult::Code::OptimizedBuildFailed, expected};
        dsValue optimizedResult;
        if (!dsEvaluate(*this, byteCode_.data() + optimizedByteCodeOffset, byteCode_.size() - optimizedByteCodeOffset, optimizedResult))
            return RunResult{RunResult::Code::OptimizedEvalFailed, expected};
        if (optimizedResult != expected)
            return RunResult{RunResult::Code::OptimizedResultFailed, expected, optimizedResult};

        return RunResult{RunResult::Code::Success, expected, result};
    };

    RunResult ExpressionTester::constant(char const* expression, dsValue const& expected)
    {
        if (!compiler_.compile(expression))
            return RunResult{RunResult::Code::CompileFailed, expected};
        if (!compiler_.optimize())
            return RunResult{RunResult::Code::OptimizeFailed, expected};

        dsValue actual;
        if (!compiler_.asConstant(actual))
            return RunResult{RunResult::Code::NotConstant, expected};

        if (actual != expected)
            return RunResult{RunResult::Code::ResultFailed, expected, actual};

        return RunResult{RunResult::Code::Success, expected, actual};
    };

    bool ExpressionTester::lookupVariable(dsName name, dsValueType& out_type) const noexcept
    {
        uint32_t const variableLen = dsNameLen(name);
        for (Variable const& variable : variables_)
        {
            if (variableLen == std::strlen(variable.name) && std::strncmp(name.name, variable.name, variableLen) == 0)
            {
                out_type = variable.value.type();
                return true;
            }
        }
        return false;
    }

    bool ExpressionTester::lookupFunction(dsName name, dsFunctionId& out_functionId, dsValueType& out_type) const noexcept
    {
        uint32_t const functionLen = dsNameLen(name);
        uint32_t nextFunctionId = 0;
        for (Function const& function : functions_)
        {
            if (functionLen == std::strlen(function.name) && std::strncmp(name.name, function.name, functionLen) == 0)
            {
                out_functionId = dsFunctionId{nextFunctionId};
                out_type = function.returnType;
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
        DS_GUARD_OR(false, dsInvalidIndex, "Invalid variable hash, miscompile");
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
