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
        virtual bool lookupVariable(dsName name) const noexcept = 0;
        virtual bool lookupFunction(dsName name, dsFunctionId& out_functionId) const noexcept = 0;

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
            : host_(host), tokens_(alloc), ast_(alloc), expression_(alloc)
        {
        }

        DS_API void reset();
        DS_API bool compile(char const* expression, char const* expressionEnd = nullptr);
        DS_API bool optimize();
        DS_API bool build(dsExpressionBuilder& builder);

    private:
        DS_DEFINE_INDEX(TokenIndex);
        DS_DEFINE_INDEX(AstIndex);
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
            AstIndex nextArgIndex = dsInvalidIndex;
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
                    AstIndex firstArgIndex = dsInvalidIndex;
                } call;
            } data;
        };

        struct Precedence
        {
            Operator op = Operator::Invalid;
            int power = -1;
        };

        bool tokenize();
        AstIndex parse();
        AstIndex optimize(AstIndex astIndex);
        bool generate(AstIndex astIndex, dsExpressionBuilder& builder);

        static Precedence unaryPrecedence(TokenType token) noexcept;
        static Precedence binaryPrecedence(TokenType token) noexcept;
        AstIndex parseExpr(int bindingPower);
        AstIndex parseFunc(AstIndex targetIndex);

        dsExpressionCompilerHost& host_;
        dsArray<Token, TokenIndex> tokens_;
        dsArray<Ast, AstIndex> ast_;
        dsString expression_;
        TokenIndex nextToken_ = dsInvalidIndex;
        AstIndex astRoot_ = dsInvalidIndex;
        AstIndex optimizedAstRoot_ = dsInvalidIndex;
    };
} // namespace descript
