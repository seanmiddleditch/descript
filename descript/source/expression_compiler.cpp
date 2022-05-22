// descript

#include "expression_compiler.hh"
#include "fnv.hh"
#include "ops.hh"
#include "utility.hh"

#include <cstring>

namespace {
    constexpr bool isAsciiDigit(char c) noexcept { return c >= '0' && c <= '9'; }
    constexpr bool isAsciiLetter(char c) noexcept { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
    constexpr bool isAsciiSpace(char c) noexcept { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

    constexpr bool isIdentifierFirst(char c) noexcept { return c == '_' || isAsciiLetter(c); }
    constexpr bool isIdentifierRest(char c) noexcept { return c == '_' || isAsciiLetter(c) || isAsciiDigit(c); }
} // namespace

namespace descript {
    void dsExpressionCompiler::reset()
    {
        tokens_.clear();
        ast_.clear();
        expression_.reset();
        nextToken_ = dsInvalidIndex;
    }

    bool dsExpressionCompiler::compile(char const* expression, char const* expressionEnd)
    {
        DS_GUARD_OR(expression != nullptr, false);

        reset();

        expression_.reset(expression, expressionEnd);

        if (!tokenize())
            return false;

        AstIndex const rootIndex = parse();
        if (rootIndex == dsInvalidIndex)
            return false;

        if (!generate(rootIndex))
            return false;

        return true;
    }

    bool dsExpressionCompiler::tokenize()
    {
        constexpr struct TokenMap
        {
            char match;
            TokenType type;
        } tokenMap[] = {
            {.match = '+', .type = TokenType::Plus},
            {.match = '-', .type = TokenType::Minus},
            {.match = '*', .type = TokenType::Star},
            {.match = '/', .type = TokenType::Slash},
            {.match = '(', .type = TokenType::LParen},
            {.match = ')', .type = TokenType::RParen},
            {.match = ',', .type = TokenType::Comma},
        };

        char const* const inputStart = expression_.data();
        char const* const inputEnd = inputStart + expression_.size();
        char const* input = inputStart;
        while (input != inputEnd)
        {
            // skip spaces
            if (isAsciiSpace(*input))
            {
                ++input;
                continue;
            }

            SourceLocation const offset{static_cast<uint32_t>(input - inputStart)};

            // handle operations
            {
                bool matched = false;
                for (TokenMap const& item : tokenMap)
                {
                    if (item.match == *input)
                    {
                        tokens_.pushBack(Token{.offset = offset, .length = 1, .type = item.type});
                        ++input;
                        matched = true;
                        break;
                    }
                }
                if (matched)
                    continue;
            }

            // handle identifiers
            if (isIdentifierFirst(*input))
            {
                ++input;
                while (input != inputEnd && isIdentifierRest(*input))
                    ++input;

                uint32_t const end = static_cast<uint32_t>(input - inputStart);
                tokens_.pushBack(
                    Token{.offset = offset, .length = static_cast<uint16_t>(end - offset.value()), .type = TokenType::Identifier});
                continue;
            }

            // handle integers
            if (isAsciiDigit(*input))
            {
                int64_t value = *input - '0';

                ++input;
                while (input != inputEnd && isAsciiDigit(*input))
                {
                    value *= 10;
                    value += *input - '0';
                    ++input;
                }

                uint32_t const end = static_cast<uint32_t>(input - inputStart);
                tokens_.pushBack(Token{.offset = offset,
                    .length = static_cast<uint16_t>(end - offset.value()),
                    .type = TokenType::LiteralInt,
                    .data = {.literalInt = value}});
                continue;
            }

            return false; // FIXME: raise error
        }

        return true;
    }

    // returns dsInvalidIndex on failure
    auto dsExpressionCompiler::parse() -> AstIndex
    {
        if (tokens_.empty())
            return dsInvalidIndex;

        nextToken_ = TokenIndex{0};

        AstIndex const root = parseExpr(0);
        if (root == dsInvalidIndex)
            return dsInvalidIndex;

        // FIXME: error, unexpected and uncomsumed token
        if (tokens_.contains(nextToken_))
            return dsInvalidIndex;

        return root;
    }

    auto dsExpressionCompiler::unaryPrecedence(TokenType token) noexcept -> Precedence
    {
        switch (token)
        {
        case TokenType::Minus: return {.op = Operator::Negate, .power = 3};
        case TokenType::LParen: return {.op = Operator::Group, .power = 0};
        default: return {};
        }
    }

    auto dsExpressionCompiler::binaryPrecedence(TokenType token) noexcept -> Precedence
    {
        switch (token)
        {
        case TokenType::Plus: return {.op = Operator::Add, .power = 1};
        case TokenType::Minus: return {.op = Operator::Sub, .power = 1};
        case TokenType::Star: return {.op = Operator::Mul, .power = 2};
        case TokenType::Slash: return {.op = Operator::Div, .power = 2};
        case TokenType::LParen: return {.op = Operator::Call, .power = 4};
        default: return {};
        }
    }

    auto dsExpressionCompiler::parseExpr(int power) -> AstIndex
    {
        if (!tokens_.contains(nextToken_))
            return dsInvalidIndex;

        // parse unary or atom
        Token const& leftToken = tokens_[nextToken_];
        AstIndex leftIndex = dsInvalidIndex;
        switch (leftToken.type)
        {
        case TokenType::LiteralInt:
            leftIndex = AstIndex{ast_.size()};
            ast_.pushBack(Ast{.type = AstType::Literal, .primaryTokenIndex = nextToken_});
            ++nextToken_;
            break;
        case TokenType::Identifier:
            leftIndex = AstIndex{ast_.size()};
            ast_.pushBack(Ast{.type = AstType::Identifier, .primaryTokenIndex = nextToken_});
            ++nextToken_;
            break;
        default: {
            Precedence const prec = unaryPrecedence(leftToken.type);
            if (prec.power == -1)
                return dsInvalidIndex;

            TokenIndex const leftTokenIndex = nextToken_;
            ++nextToken_;

            AstIndex rightIndex = parseExpr(prec.power);
            if (rightIndex == dsInvalidIndex)
                return dsInvalidIndex;

            if (prec.op == Operator::Group)
            {
                if (!tokens_.contains(nextToken_) || tokens_[nextToken_].type != TokenType::RParen)
                {
                    // FIXME: error, expected rparen
                    return dsInvalidIndex;
                }
                ++nextToken_;

                leftIndex = AstIndex{ast_.size()};
                ast_.pushBack(
                    Ast{.type = AstType::Group, .primaryTokenIndex = leftTokenIndex, .data = {.group = {.childIndex = rightIndex}}});
            }
            else
            {
                leftIndex = AstIndex{ast_.size()};
                ast_.pushBack(Ast{.type = AstType::UnaryOp,
                    .primaryTokenIndex = leftTokenIndex,
                    .data = {.unary = {.op = prec.op, .childIndex = rightIndex}}});
            }
            break;
        }
        }

        while (tokens_.contains(nextToken_))
        {
            // expect an infix operator
            TokenIndex const infixTokenIndex = nextToken_;
            Token const& infixToken = tokens_[infixTokenIndex];

            Precedence const prec = binaryPrecedence(infixToken.type);
            if (prec.power == -1)
                break;

            if (prec.power <= power)
                break;

            ++nextToken_;

            if (prec.op == Operator::Call)
            {
                leftIndex = parseFunc(leftIndex);
                if (leftIndex == dsInvalidIndex)
                    return dsInvalidIndex;
            }
            else
            {
                AstIndex const rightIndex = parseExpr(prec.power);
                if (rightIndex == dsInvalidIndex)
                    return dsInvalidIndex;

                AstIndex const newIndex{ast_.size()};
                ast_.pushBack(Ast{.type = AstType::BinaryOp,
                    .primaryTokenIndex = infixTokenIndex,
                    .data = {.binary = {.op = prec.op, .leftIndex = leftIndex, .rightIndex = rightIndex}}});
                leftIndex = newIndex;
            }
        }

        return leftIndex;
    }

    auto dsExpressionCompiler::parseFunc(AstIndex targetIndex) -> AstIndex
    {
        AstIndex const callIndex{ast_.size()};
        ast_.pushBack(Ast{.type = AstType::Call, .primaryTokenIndex = nextToken_, .data = {.call = {.targetIndex = targetIndex}}});

        if (tokens_.contains(nextToken_) && tokens_[nextToken_].type == TokenType::RParen)
        {
            ++nextToken_;
            return callIndex;
        }

        AstIndex prevArgIndex = dsInvalidIndex;
        while (tokens_.contains(nextToken_))
        {
            AstIndex const argIndex = parseExpr(0);

            if (prevArgIndex == dsInvalidIndex)
                ast_[callIndex].data.call.firstArgIndex = argIndex;
            else
                ast_[prevArgIndex].nextArgIndex = argIndex;
            prevArgIndex = argIndex;

            // we only continue looping if we get a comma
            if (tokens_.contains(nextToken_) && tokens_[nextToken_].type == TokenType::Comma)
            {
                ++nextToken_;
                continue;
            }

            break;
        }

        // expect a closing rparen
        if (!tokens_.contains(nextToken_) || tokens_[nextToken_].type != TokenType::RParen)
        {
            // FIXME: error, expected rparen
            return dsInvalidIndex;
        }
        ++nextToken_;

        return callIndex;
    }

    bool dsExpressionCompiler::generate(AstIndex astIndex)
    {
        Ast const& ast = ast_[astIndex];
        Token const& primaryToken = tokens_[ast.primaryTokenIndex];
        switch (ast.type)
        {
        case AstType::Literal:
            switch (primaryToken.type)
            {
            case TokenType::LiteralInt:
                switch (primaryToken.data.literalInt)
                {
                case 0: builder_.pushOp((uint8_t)dsOpCode::Push0); break;
                case 1: builder_.pushOp((uint8_t)dsOpCode::Push1); break;
                case -1: builder_.pushOp((uint8_t)dsOpCode::PushNeg1); break;
                default: {
                    dsExpressionConstantIndex const index =
                        builder_.pushConstant(dsValue{static_cast<double>(primaryToken.data.literalInt)});
                    if (index == dsInvalidIndex)
                    {
                        // FIXME: error, constant overflow
                        return false;
                    }

                    builder_.pushOp((uint8_t)dsOpCode::PushConstant);
                    builder_.pushOp((uint8_t)(index.value() >> 8));
                    builder_.pushOp((uint8_t)(index.value() & 0xff));
                    break;
                }
                }
                break;
            default: DS_GUARD_OR(false, false, "Unknown literal token type");
            }
            return true;
        case AstType::Identifier: {
            char const* const identStart = expression_.data() + primaryToken.offset.value();
            char const* const identEnd = identStart + primaryToken.length;

            if (!host_.lookupVariable(dsName{identStart, identEnd}))
            {
                // FIXME: error, invalid variable
                return false;
            }

            uint64_t const nameHash = dsHashFnv1a64(identStart, identEnd);
            dsExpressionVariableIndex const index = builder_.pushVariable(nameHash);
            if (index == dsInvalidIndex)
            {
                // FIXME: error, variable overflow
                return false;
            }

            builder_.pushOp((uint8_t)dsOpCode::Read);
            builder_.pushOp((uint8_t)(index.value() >> 8));
            builder_.pushOp((uint8_t)(index.value() & 0xff));
            return true;
        }
        case AstType::BinaryOp:
            if (!generate(ast.data.binary.leftIndex))
                return false;
            if (!generate(ast.data.binary.rightIndex))
                return false;
            switch (ast.data.binary.op)
            {
            case Operator::Add: builder_.pushOp((uint8_t)dsOpCode::Add); break;
            case Operator::Sub: builder_.pushOp((uint8_t)dsOpCode::Sub); break;
            case Operator::Mul: builder_.pushOp((uint8_t)dsOpCode::Mul); break;
            case Operator::Div: builder_.pushOp((uint8_t)dsOpCode::Div); break;
            default: DS_GUARD_OR(false, false, "Unknown binary operator type");
            }
            return true;
        case AstType::UnaryOp:
            if (!generate(ast.data.unary.childIndex))
                return false;
            switch (ast.data.unary.op)
            {
            case Operator::Negate: builder_.pushOp((uint8_t)dsOpCode::Neg); break;
            default: DS_GUARD_OR(false, false, "Unknown unary operator type");
            }
            return true;
        case AstType::Group: return generate(ast.data.group.childIndex);
        case AstType::Call: {
            uint8_t argc = 0;
            for (AstIndex argIndex = ast.data.call.firstArgIndex; argIndex != dsInvalidIndex; argIndex = ast_[argIndex].nextArgIndex)
            {
                ++argc;
                if (!generate(argIndex))
                    return false;
            }

            Ast const& target = ast_[ast.data.call.targetIndex];
            if (target.type != AstType::Identifier)
            {
                // FIXME: error, invalid grammar
                return false;
            }

            Token const& targetToken = tokens_[target.primaryTokenIndex];
            char const* const identStart = expression_.data() + targetToken.offset.value();
            char const* const identEnd = identStart + targetToken.length;

            dsFunctionId functionId = dsInvalidFunctionId;
            if (!host_.lookupFunction(dsName{identStart, identEnd}, functionId))
            {
                // FIXME: error, invalid function
                return false;
            }

            dsExpressionFunctionIndex const index = builder_.pushFunction(functionId);
            if (index == dsInvalidIndex)
            {
                // FIXME: error, function overflow
                return false;
            }

            builder_.pushOp((uint8_t)dsOpCode::Call);
            builder_.pushOp((uint8_t)(index.value() >> 8));
            builder_.pushOp((uint8_t)(index.value() & 0xff));
            builder_.pushOp(argc);
            return true;
        }
        default: DS_GUARD_OR(false, false, "Unknown AST node type");
        }
    }
} // namespace descript
