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
            : host_(host), tokens_(alloc), ast_(alloc), mid_(alloc), astLinks_(alloc), midLinks_(alloc), expression_(alloc)
        {
        }

        DS_API void reset();
        DS_API bool compile(char const* expression, char const* expressionEnd = nullptr);
        DS_API bool optimize();
        DS_API bool build(dsExpressionBuilder& builder);

        DS_API bool isConstant() const noexcept;
        DS_API bool isVariableOnly() const noexcept;
        DS_API dsValueType resultType() const noexcept;

        DS_API bool asConstant(dsValue& out_value) const;

    private:
        DS_DEFINE_INDEX(TokenIndex);
        DS_DEFINE_INDEX(AstIndex);
        DS_DEFINE_INDEX(AstLinkIndex);
        DS_DEFINE_INDEX(MidIndex);
        DS_DEFINE_INDEX(MidLinkIndex);
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

            Literal,
            Identifier,
            BinaryOp,
            UnaryOp,
            Group,
            Call,
        };

        enum class MidType : uint8_t
        {
            Invalid,

            Literal,
            Variable,
            BinaryOp,
            UnaryOp,
            Call,
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
            union Data {
                int unused_ = 0;
                struct Literal
                {
                    LiteralType type = LiteralType::Nil;
                    union Value {
                        bool b = false;
                        int64_t s64;
                    } value;
                } literal;
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
            } data;
        };

        struct Mid
        {
            MidType type = MidType::Invalid;
            AstIndex astIndex = dsInvalidIndex;
            dsValueType valueType = dsValueType::Nil;
            union Data {
                int unused_ = 0;
                struct Literal
                {
                    LiteralType type = LiteralType::Nil;
                    union Value {
                        bool b = false;
                        int64_t s64;
                    } value;
                } literal;
                struct Variable
                {
                    uint64_t nameHash = 0;
                } variable;
                struct Binary
                {
                    Operator op = Operator::Invalid;
                    MidIndex leftIndex = dsInvalidIndex;
                    MidIndex rightIndex = dsInvalidIndex;
                } binary;
                struct Unary
                {
                    Operator op = Operator::Invalid;
                    MidIndex childIndex = dsInvalidIndex;
                } unary;
                struct Group
                {
                    MidIndex childIndex = dsInvalidIndex;
                } group;
                struct Call
                {
                    dsFunctionId functionId = dsInvalidFunctionId;
                    MidLinkIndex firstArgIndex = dsInvalidIndex;
                    uint8_t arity = 0;
                } call;
                struct Arg
                {
                    MidIndex callIndex;
                    MidIndex argIndex;
                    MidIndex nextIndex;
                } arg;
            } data;
        };

        struct AstLink
        {
            AstIndex childIndex = dsInvalidIndex;
            AstLinkIndex nextIndex = dsInvalidIndex;
        };

        struct MidLink
        {
            MidIndex childIndex = dsInvalidIndex;
            MidLinkIndex nextIndex = dsInvalidIndex;
        };

        struct MidResult
        {
            MidResult() noexcept = default;
            explicit MidResult(MidIndex index) noexcept : success(index != dsInvalidIndex), index(index) {}
            MidResult(bool success, MidIndex index) noexcept : success(success), index(index) {}

            bool success = false;
            MidIndex index = dsInvalidIndex;
        };

        struct Precedence
        {
            Operator op = Operator::Invalid;
            int power = -1;
        };

        bool tokenize();
        AstIndex parse();
        MidResult lower(AstIndex astIndex);
        MidIndex optimize(MidIndex midIndex);
        bool generate(MidIndex midIndex, dsExpressionBuilder& builder) const;

        static Precedence unaryPrecedence(TokenType token) noexcept;
        static Precedence binaryPrecedence(TokenType token) noexcept;
        AstIndex parseExpr(int bindingPower);
        AstIndex parseFunc(AstIndex targetIndex);

        dsExpressionCompilerHost& host_;
        dsArray<Token, TokenIndex> tokens_;
        dsArray<Ast, AstIndex> ast_;
        dsArray<Mid, MidIndex> mid_;
        dsArray<AstLink, AstLinkIndex> astLinks_;
        dsArray<MidLink, MidLinkIndex> midLinks_;
        dsString expression_;
        TokenIndex nextToken_ = dsInvalidIndex;
        AstIndex astRoot_ = dsInvalidIndex;
        MidIndex midRoot_ = dsInvalidIndex;
        AstIndex optimizedAstRoot_ = dsInvalidIndex;
    };
} // namespace descript
