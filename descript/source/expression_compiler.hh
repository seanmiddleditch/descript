// descript

#pragma once

#include "descript/export.hh"
#include "descript/types.hh"
#include "descript/value.hh"

#include "array.hh"
#include "expression.hh"
#include "index.hh"
#include "string.hh"

#include <cstdint>

namespace descript {
    class dsExpressionCompilerHost
    {
    public:
        virtual bool lookupVariable(dsName name, dsValueType& out_type) const noexcept = 0;
        virtual bool lookupFunction(dsName name, dsFunctionId& out_functionId, dsValueType& out_type) const noexcept = 0;

    protected:
        ~dsExpressionCompilerHost() = default;
    };

    class dsExpressionBuilder
    {
    public:
        virtual void pushOp(uint8_t byte) = 0;

        virtual dsExpressionConstantIndex pushConstant(dsValue const& value) = 0;
        virtual dsExpressionFunctionIndex pushFunction(dsFunctionId functionId) = 0;
        virtual dsExpressionVariableIndex pushVariable(uint64_t nameHash) = 0;

    protected:
        ~dsExpressionBuilder() = default;
    };

    class dsExpressionCompiler final
    {
    public:
        explicit dsExpressionCompiler(dsAllocator& alloc, dsExpressionCompilerHost& host) noexcept
            : host_(host), tokens_(alloc), ast_(alloc), astLinks_(alloc), expression_(alloc)
        {
        }

        DS_API void reset();
        DS_API bool compile(char const* expression, char const* expressionEnd = nullptr);
        DS_API bool optimize();
        DS_API bool build(dsExpressionBuilder& builder);

        DS_API bool isEmpty() const noexcept;
        DS_API bool isConstant() const noexcept;
        DS_API bool isVariableOnly() const noexcept;
        DS_API dsValueType resultType() const noexcept;

        DS_API bool asConstant(dsValue& out_value) const;

    private:
        DS_DEFINE_INDEX(TokenIndex);
        DS_DEFINE_INDEX(AstIndex);
        DS_DEFINE_INDEX(AstLinkIndex);
        DS_DEFINE_INDEX(SourceLocation);

        enum class TokenType : uint8_t
        {
            Invalid,

            Plus,
            Minus,
            Star,
            Slash,
            LParen,
            RParen,
            Comma,
            LiteralInt,
            Identifier,
            KeyTrue,
            KeyFalse,
            KeyOr,
            KeyAnd,
            KeyNot,
            KeyXor,
            KeyIs,
            KeyNil,
            Reserved,
        };

        enum class AstType : uint8_t
        {
            Invalid,

            // common ast types
            BinaryOp,
            UnaryOp,

            // only exist before lowering
            Literal,
            Identifier,
            Call,
            Group,

            // only exist after lowering
            Constant,
            Variable,
            Function,
        };

        enum class Operator
        {
            Invalid,

            // binary arithmetic
            Add,
            Sub,
            Mul,
            Div,

            // binary logical
            And,
            Or,
            Xor,

            // unary arithmetic
            Negate,

            // unary logical
            Not,

            // special
            Group,
            Call,
        };

        enum class LiteralType
        {
            Integer,
            Boolean,
            Nil
        };

        struct Token
        {
            SourceLocation offset = dsInvalidIndex;
            uint16_t length = 0;
            TokenType type = TokenType::Invalid;
            union Data {
                int64_t literalInt = 0;
            } data;
        };

        struct Ast
        {
            AstType type = AstType::Invalid;
            TokenIndex primaryTokenIndex = dsInvalidIndex;
            dsValueType valueType = dsValueType::Nil; // only filled in after lowering
            union Data {
                int unused_ = 0;
                union Constant
                {
                    bool bool_ = false;
                    int64_t int64_;
                    double float64_;
                } constant;
                struct Variable
                {
                    uint64_t nameHash = 0;
                } variable;
                struct Binary
                {
                    Operator op = Operator::Invalid;
                    AstIndex leftIndex = dsInvalidIndex;
                    AstIndex rightIndex = dsInvalidIndex;
                } binary;
                struct Unary
                {
                    Operator op = Operator::Invalid;
                    AstIndex childIndex = dsInvalidIndex;
                } unary;
                struct Group
                {
                    AstIndex childIndex = dsInvalidIndex;
                } group;
                struct Call
                {
                    AstIndex targetIndex = dsInvalidIndex;
                    AstLinkIndex firstArgIndex = dsInvalidIndex;
                } call;
                struct Function
                {
                    dsFunctionId functionId = dsInvalidFunctionId;
                    AstLinkIndex firstArgIndex = dsInvalidIndex;
                    uint8_t arity = 0;
                } function;
            } data;
        };

        struct AstLink
        {
            AstIndex childIndex = dsInvalidIndex;
            AstLinkIndex nextIndex = dsInvalidIndex;
        };

        struct LowerResult
        {
            LowerResult() noexcept = default;
            explicit LowerResult(AstIndex index) noexcept : success(index != dsInvalidIndex), index(index) {}
            LowerResult(bool success, AstIndex index) noexcept : success(success), index(index) {}

            bool success = false;
            AstIndex index = dsInvalidIndex;
        };

        struct Precedence
        {
            Operator op = Operator::Invalid;
            int power = -1;
        };

        enum class Status
        {
            Reset,
            Lexed,
            Parsed,
            Lowered,
            Optimized,
        };

        bool tokenize();
        AstIndex parse();
        LowerResult lower(AstIndex astIndex);
        AstIndex optimize(AstIndex astIndex);
        bool generate(AstIndex astIndex, dsExpressionBuilder& builder) const;

        static Precedence unaryPrecedence(TokenType token) noexcept;
        static Precedence binaryPrecedence(TokenType token) noexcept;
        AstIndex parseExpr(int bindingPower);
        AstIndex parseFunc(AstIndex targetIndex);

        dsExpressionCompilerHost& host_;
        dsArray<Token, TokenIndex> tokens_;
        dsArray<Ast, AstIndex> ast_;
        dsArray<AstLink, AstLinkIndex> astLinks_;
        dsString expression_;
        TokenIndex nextToken_ = dsInvalidIndex;
        AstIndex astRoot_ = dsInvalidIndex;
        Status status_ = Status::Reset;
    };
} // namespace descript
