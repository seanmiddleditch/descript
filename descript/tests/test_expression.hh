// descript

#pragma once

#include "descript/alloc.hh"
#include "descript/evaluate.hh"
#include "descript/expression_compiler.hh"
#include "descript/meta.hh"
#include "descript/types.hh"
#include "descript/value.hh"

#include "array.hh"
#include "fnv.hh"
#include "storage.hh"
#include "utility.hh"

#include <ostream>

namespace descript::test::expression {
    struct Function
    {
        char const* name = nullptr;
        dsTypeId returnType;
        dsFunction function = nullptr;
        void* userData = nullptr;
    };

    struct Variable
    {
        char const* name = nullptr;
        dsValueStorage value;
    };

    inline std::ostream& operator<<(std::ostream& os, dsValueStorage const& value);
    inline std::ostream& operator<<(std::ostream& os, dsTypeId type);

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
        dsTypeId expected;
        dsTypeId actual;

        CompileResult(Code code, dsTypeId expected) noexcept : code(code), expected(expected) {}
        CompileResult(Code code, dsTypeId expected, dsTypeId actual) noexcept : code(code), expected(expected), actual(actual) {}

        explicit operator bool() const noexcept { return code == Code::Success; }

        friend std::ostream& operator<<(std::ostream& os, CompileResult const& result)
        {
            switch (result.code)
            {
            case Code::Success: return os << "Success\nExpected: " << result.expected << "\nResult: " << result.actual;
            case Code::CompileFailed: return os << "Compile Failed\nExpected: " << result.expected;
            case Code::OptimizeFailed: return os << "Optimize Failed\nExpected: " << result.expected;
            case Code::NotVariableOnly: return os << "Variable Only Failed\nExpected: " << result.expected;
            case Code::TypeFailed: return os << "Type Check Failed\nExpected: " << result.expected << "\nResult: " << result.actual;
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
        dsValueStorage expected;
        dsValueStorage actual;

        RunResult(Code code, dsValueStorage const& expected) noexcept : code(code), expected(expected) {}
        RunResult(Code code, dsValueStorage const& expected, dsValueStorage const& actual) noexcept
            : code(code), expected(expected), actual(actual)
        {
        }

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
            : byteCode_(alloc), constants_(alloc), variables_(variables), functions_(functions),
              compiler_(dsCreateExpressionCompiler(alloc, *this))
        {
        }
        ~ExpressionTester() { dsDestroyExpressionCompiler(compiler_); }

        CompileResult compile(char const* expression, dsTypeId expectedType);
        CompileResult variable(char const* expression, dsTypeId expectedType);
        RunResult run(char const* expression, dsValueStorage const& expected);
        RunResult constant(char const* expression, dsValueStorage const& expected);

    protected:
        // dsExpressionCompilerHost
        bool lookupVariable(dsName name, dsVariableCompileMeta& out_meta) const noexcept override;
        bool lookupFunction(dsName name, dsFunctionCompileMeta& out_meta) const noexcept override;

        // dsExpressionBuilder
        void pushOp(uint8_t byte) override { byteCode_.pushBack(byte); }
        uint32_t pushConstant(dsValueRef const& value) override;
        uint32_t pushFunction(dsFunctionId functionId) override;
        uint32_t pushVariable(uint64_t nameHash) override;

        // dsEvaluateHost
        void listen(dsEmitterId) override {}
        bool readVariable(uint32_t variableIndex, dsValueOut out_value) override;
        bool readConstant(uint32_t constantIndex, dsValueOut out_value) override;
        bool invokeFunction(uint32_t functionIndex, dsFunctionContext& ctx) override;

    private:
        dsArray<uint8_t> byteCode_;
        dsArray<dsValueStorage> constants_;
        dsSpan<Variable const> variables_;
        dsSpan<Function const> functions_;
        dsExpressionCompiler* compiler_ = nullptr;
    };

    std::ostream& operator<<(std::ostream& os, dsValueStorage const& value)
    {
        if (value.type() == dsType<int32_t>)
            return os << value.type() << '(' << value.as<int32_t>() << ')';
        if (value.type() == dsType<float>)
            return os << value.type() << '(' << value.as<float>() << ')';
        if (value.type() == dsType<bool>)
            return os << (value.as<bool>() ? "true" : "false");
        return os << value.type();
    }

    std::ostream& operator<<(std::ostream& os, dsTypeId type) { return os << type.meta().name; }

    CompileResult ExpressionTester::compile(char const* expression, dsTypeId expectedType)
    {
        if (!compiler_->compile(expression))
            return CompileResult{CompileResult::Code::CompileFailed, expectedType};
        if (!compiler_->optimize())
            return CompileResult{CompileResult::Code::OptimizeFailed, expectedType};
        return CompileResult{CompileResult::Code::Success, expectedType, compiler_->resultType()};
    }

    CompileResult ExpressionTester::variable(char const* expression, dsTypeId expectedType)
    {
        if (!compiler_->compile(expression))
            return CompileResult{CompileResult::Code::CompileFailed, expectedType};
        if (!compiler_->optimize())
            return CompileResult{CompileResult::Code::OptimizeFailed, expectedType};
        if (!compiler_->isVariableOnly())
            return CompileResult{CompileResult::Code::NotVariableOnly, expectedType, compiler_->resultType()};
        if (compiler_->resultType() != expectedType)
            return CompileResult{CompileResult::Code::TypeFailed, expectedType, compiler_->resultType()};
        return CompileResult{CompileResult::Code::Success, expectedType, compiler_->resultType()};
    }

    RunResult ExpressionTester::run(char const* expression, dsValueStorage const& expected)
    {
        if (!compiler_->compile(expression))
            return RunResult{RunResult::Code::CompileFailed, expected};

        uint32_t const byteCodeOffset = byteCode_.size();
        if (!compiler_->build(*this))
            return RunResult{RunResult::Code::BuildFailed, expected};
        dsValueStorage result;
        if (!dsEvaluate(*this, byteCode_.data() + byteCodeOffset, byteCode_.size() - byteCodeOffset, result.out()))
            return RunResult{RunResult::Code::EvalFailed, expected};
        if (result != expected)
            return RunResult{RunResult::Code::ResultFailed, expected, result};

        uint32_t const optimizedByteCodeOffset = byteCode_.size();

        if (!compiler_->optimize())
            return RunResult{RunResult::Code::OptimizeFailed, expected};
        if (!compiler_->build(*this))
            return RunResult{RunResult::Code::OptimizedBuildFailed, expected};
        dsValueStorage optimizedResult;
        if (!dsEvaluate(*this, byteCode_.data() + optimizedByteCodeOffset, byteCode_.size() - optimizedByteCodeOffset,
                optimizedResult.out()))
            return RunResult{RunResult::Code::OptimizedEvalFailed, expected};
        if (optimizedResult != expected)
            return RunResult{RunResult::Code::OptimizedResultFailed, expected, optimizedResult};

        return RunResult{RunResult::Code::Success, expected, result};
    };

    RunResult ExpressionTester::constant(char const* expression, dsValueStorage const& expected)
    {
        if (!compiler_->compile(expression))
            return RunResult{RunResult::Code::CompileFailed, expected};
        if (!compiler_->optimize())
            return RunResult{RunResult::Code::OptimizeFailed, expected};

        dsValueStorage actual;
        if (!compiler_->asConstant(actual.out()))
            return RunResult{RunResult::Code::NotConstant, expected};

        if (actual != expected)
            return RunResult{RunResult::Code::ResultFailed, expected, actual};

        return RunResult{RunResult::Code::Success, expected, actual};
    };

    bool ExpressionTester::lookupVariable(dsName name, dsVariableCompileMeta& out_meta) const noexcept
    {
        uint32_t const variableLen = dsNameLen(name);
        for (Variable const& variable : variables_)
        {
            if (variableLen == std::strlen(variable.name) && std::strncmp(name.name, variable.name, variableLen) == 0)
            {
                out_meta.type = variable.value.type();
                return true;
            }
        }
        return false;
    }

    bool ExpressionTester::lookupFunction(dsName name, dsFunctionCompileMeta& out_meta) const noexcept
    {
        uint32_t const functionLen = dsNameLen(name);
        uint32_t nextFunctionId = 0;
        for (Function const& function : functions_)
        {
            if (functionLen == std::strlen(function.name) && std::strncmp(name.name, function.name, functionLen) == 0)
            {
                out_meta.functionId = dsFunctionId{nextFunctionId};
                out_meta.returnType = function.returnType;
                return true;
            }
            ++nextFunctionId;
        }
        return false;
    }

    uint32_t ExpressionTester::pushConstant(dsValueRef const& value)
    {
        uint32_t const index{constants_.size()};
        constants_.emplaceBack(value);
        return index;
    }

    uint32_t ExpressionTester::pushVariable(uint64_t nameHash)
    {
        uint32_t nextIndex = 0;
        for (Variable const& variable : variables_)
        {
            if (nameHash == dsHashFnv1a64(variable.name))
                return nextIndex;
            ++nextIndex;
        }
        DS_GUARD_OR(false, 0, "Invalid variable hash, miscompile");
    }

    uint32_t ExpressionTester::pushFunction(dsFunctionId functionId) { return static_cast<uint32_t>(functionId.value()); }

    bool ExpressionTester::readVariable(uint32_t variableIndex, dsValueOut out_value)
    {
        DS_GUARD_OR(variableIndex < variables_.count, false);
        return out_value.accept(variables_.items[variableIndex].value.ref());
    }

    bool ExpressionTester::readConstant(uint32_t constantIndex, dsValueOut out_value)
    {
        if (!constants_.contains(constantIndex))
            return false;
        return out_value.accept(constants_[constantIndex].ref());
    }

    bool ExpressionTester::invokeFunction(uint32_t functionIndex, dsFunctionContext& ctx)
    {
        DS_GUARD_OR(functionIndex < functions_.count, false, "Invalid function index, miscompile");

        Function const& func = functions_.items[functionIndex];
        func.function(ctx, func.userData);
        return true;
    }
} // namespace descript::test::expression
