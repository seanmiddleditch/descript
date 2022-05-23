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

    constexpr char toAsciiLower(char c) noexcept { return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c; }

    constexpr bool strCaseEqual(char const* first, char const* second, uint32_t maxLen)
    {
        while (*first != '\0' && *second != '\0' && maxLen != 0)
        {
            if (toAsciiLower(*first) != toAsciiLower(*second))
                return false;
            ++first;
            ++second;
            --maxLen;
        }
        return maxLen == 0;
    }
} // namespace

namespace descript {
    void dsExpressionCompiler::reset()
    {
        tokens_.clear();
        ast_.clear();
        mid_.clear();
        astLinks_.clear();
        midLinks_.clear();
        expression_.reset();
        nextToken_ = dsInvalidIndex;
        astRoot_ = dsInvalidIndex;
        optimizedAstRoot_ = dsInvalidIndex;
        midRoot_ = dsInvalidIndex;
    }

    bool dsExpressionCompiler::compile(char const* expression, char const* expressionEnd)
    {
        DS_GUARD_OR(expression != nullptr, false);

        reset();

        expression_.reset(expression, expressionEnd);

        if (!tokenize())
            return false;

        astRoot_ = parse();
        if (astRoot_ == dsInvalidIndex)
            return false;

        auto const [success, midRoot] = lower(astRoot_);
        midRoot_ = midRoot;

        return success;
    }

    bool dsExpressionCompiler::optimize()
    {
        DS_GUARD_OR(midRoot_ != dsInvalidIndex, false);

        MidIndex const midIndex = optimize(midRoot_);
        if (midIndex == dsInvalidIndex)
            return false;

        midRoot_ = midIndex;
        return true;
    }

    bool dsExpressionCompiler::build(dsExpressionBuilder& builder)
    {
        DS_GUARD_OR(astRoot_ != dsInvalidIndex, false);
        DS_GUARD_OR(midRoot_ != dsInvalidIndex, false);

        AstIndex const rootIndex = optimizedAstRoot_ != dsInvalidIndex ? optimizedAstRoot_ : astRoot_;

        if (!generate(midRoot_, builder))
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

        constexpr struct KeywordMap
        {
            char const* keyword;
            TokenType type;
        } keywordMap[] = {
            {.keyword = "true", .type = TokenType::KeyTrue},
            {.keyword = "false", .type = TokenType::KeyFalse},
            {.keyword = "and", .type = TokenType::KeyAnd},
            {.keyword = "or", .type = TokenType::KeyOr},
            {.keyword = "xor", .type = TokenType::KeyXor},
            {.keyword = "not", .type = TokenType::KeyNot},
            {.keyword = "is", .type = TokenType::KeyIs},
            {.keyword = "nil", .type = TokenType::KeyNil},

            // reserved words we'd like to consider using in the future
            {.keyword = "null", .type = TokenType::Reserved},
            {.keyword = "eq", .type = TokenType::Reserved},
            {.keyword = "ne", .type = TokenType::Reserved},
            {.keyword = "lt", .type = TokenType::Reserved},
            {.keyword = "lte", .type = TokenType::Reserved},
            {.keyword = "gt", .type = TokenType::Reserved},
            {.keyword = "gte", .type = TokenType::Reserved},
            {.keyword = "if", .type = TokenType::Reserved},
            {.keyword = "then", .type = TokenType::Reserved},
            {.keyword = "end", .type = TokenType::Reserved},
            {.keyword = "for", .type = TokenType::Reserved},
            {.keyword = "while", .type = TokenType::Reserved},
            {.keyword = "do", .type = TokenType::Reserved},
            {.keyword = "done", .type = TokenType::Reserved},
            {.keyword = "in", .type = TokenType::Reserved},
            {.keyword = "case", .type = TokenType::Reserved},
            {.keyword = "when", .type = TokenType::Reserved},
            {.keyword = "where", .type = TokenType::Reserved},
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

            // handle identifiers / keywords
            if (isIdentifierFirst(*input))
            {
                ++input;
                while (input != inputEnd && isIdentifierRest(*input))
                    ++input;

                uint32_t const end = static_cast<uint32_t>(input - inputStart);

                TokenType type = TokenType::Identifier;

                for (KeywordMap const& keyword : keywordMap)
                {
                    if (strCaseEqual(keyword.keyword, inputStart + offset.value(), end - offset.value()))
                    {
                        type = keyword.type;
                        break;
                    }
                }

                tokens_.pushBack(Token{.offset = offset, .length = static_cast<uint16_t>(end - offset.value()), .type = type});
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
        case TokenType::Minus: return {.op = Operator::Negate, .power = 5};
        case TokenType::KeyNot: return {.op = Operator::Not, .power = 5};
        case TokenType::LParen: return {.op = Operator::Group, .power = 0};
        default: return {};
        }
    }

    auto dsExpressionCompiler::binaryPrecedence(TokenType token) noexcept -> Precedence
    {
        switch (token)
        {
        case TokenType::KeyOr: return {.op = Operator::Or, .power = 1};
        case TokenType::KeyXor: return {.op = Operator::Xor, .power = 1};
        case TokenType::KeyAnd: return {.op = Operator::And, .power = 2};
        case TokenType::Plus: return {.op = Operator::Add, .power = 3};
        case TokenType::Minus: return {.op = Operator::Sub, .power = 3};
        case TokenType::Star: return {.op = Operator::Mul, .power = 4};
        case TokenType::Slash: return {.op = Operator::Div, .power = 4};
        case TokenType::LParen: return {.op = Operator::Call, .power = 6};
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
            ast_.pushBack(Ast{.type = AstType::Literal,
                .primaryTokenIndex = nextToken_,
                .data = {.literal{.type = LiteralType::Integer, .value = {.s64 = leftToken.data.literalInt}}}});
            ++nextToken_;
            break;
        case TokenType::KeyTrue:
        case TokenType::KeyFalse:
            leftIndex = AstIndex{ast_.size()};
            ast_.pushBack(Ast{.type = AstType::Literal,
                .primaryTokenIndex = nextToken_,
                .data = {.literal{.type = LiteralType::Boolean, .value = {.b = leftToken.type == TokenType::KeyTrue}}}});
            ++nextToken_;
            break;
        case TokenType::KeyNil:
            leftIndex = AstIndex{ast_.size()};
            ast_.pushBack(Ast{.type = AstType::Literal, .primaryTokenIndex = nextToken_, .data = {.literal{.type = LiteralType::Nil}}});
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

        AstLinkIndex prevLinkIndex = dsInvalidIndex;
        while (tokens_.contains(nextToken_))
        {
            AstIndex const argIndex = parseExpr(0);

            AstLinkIndex const linkIndex{astLinks_.size()};
            astLinks_.pushBack(AstLink{.childIndex = argIndex});

            if (prevLinkIndex == dsInvalidIndex)
                ast_[callIndex].data.call.firstArgIndex = linkIndex;
            else
                astLinks_[prevLinkIndex].nextIndex = linkIndex;
            prevLinkIndex = linkIndex;

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

    auto dsExpressionCompiler::lower(AstIndex astIndex) -> MidResult
    {
        DS_GUARD_OR(astIndex != dsInvalidIndex, MidResult(false, dsInvalidIndex));

        Ast const& ast = ast_[astIndex];
        switch (ast.type)
        {
        case AstType::Literal: {
            MidIndex const midIndex{mid_.size()};
            Mid& mid = mid_.pushBack(Mid{.type = MidType::Literal, .astIndex = astIndex, .data = {.literal = {}}});

            switch (tokens_[ast.primaryTokenIndex].type)
            {
            case TokenType::KeyTrue:
            case TokenType::KeyFalse:
                mid.valueType = dsValueType::Bool;
                mid.data.literal.type = LiteralType::Boolean;
                mid.data.literal.value = {.b = tokens_[ast.primaryTokenIndex].type == TokenType::KeyTrue};
                return {true, midIndex};
            case TokenType::KeyNil:
                mid.valueType = dsValueType::Nil;
                mid.data.literal.type = LiteralType::Nil;
                return {true, midIndex};
            case TokenType::LiteralInt:
                mid.valueType = dsValueType::Double;
                mid.data.literal.type = LiteralType::Integer;
                mid.data.literal.value = {.s64 = tokens_[ast.primaryTokenIndex].data.literalInt};
                return {true, midIndex};
            default: DS_GUARD_OR(false, MidResult(false, midIndex), "Unknown ast node type");
            }
        }
        case AstType::UnaryOp: {
            auto const [success, operandIndex] = lower(ast.data.unary.childIndex);

            MidIndex const midIndex{mid_.size()};
            Mid& mid = mid_.pushBack(Mid{.type = MidType::UnaryOp,
                .astIndex = astIndex,
                .data = {.unary = {.op = ast.data.unary.op, .childIndex = operandIndex}}});

            switch (ast.data.unary.op)
            {
            case Operator::Negate:
                mid.valueType = dsValueType::Double;
                if (mid_[operandIndex].valueType != dsValueType::Double)
                    return {false, midIndex}; // FIXME: error on type
                return {success, midIndex};
            case Operator::Not:
                mid.valueType = dsValueType::Bool;
                if (mid_[operandIndex].valueType != dsValueType::Bool)
                    return {false, midIndex}; // FIXME: error on type
                return {success, midIndex};
            default: DS_GUARD_OR(false, MidResult(false, midIndex), "Unknown unary operator");
            }
        }
        case AstType::BinaryOp: {
            auto const [leftSuccess, leftIndex] = lower(ast.data.binary.leftIndex);
            auto const [rightSuccess, rightIndex] = lower(ast.data.binary.rightIndex);
            bool const success = leftSuccess && rightSuccess;

            MidIndex const midIndex{mid_.size()};
            Mid& mid = mid_.pushBack(Mid{.type = MidType::BinaryOp,
                .astIndex = astIndex,
                .data = {.binary = {.op = ast.data.binary.op, .leftIndex = leftIndex, .rightIndex = rightIndex}}});

            // FIXME: don't assume all binary operations are homogenously typed
            if (mid_[leftIndex].valueType != mid_[rightIndex].valueType)
                return {false, midIndex}; // FIXME: type error

            switch (ast.data.unary.op)
            {
            case Operator::Add:
            case Operator::Sub:
            case Operator::Mul:
            case Operator::Div:
                mid.valueType = dsValueType::Double;
                if (mid_[leftIndex].valueType != dsValueType::Double)
                    return {false, midIndex}; // FIXME: error on type
                return {success, midIndex};
            case Operator::And:
            case Operator::Or:
            case Operator::Xor:
                mid.valueType = dsValueType::Bool;
                if (mid_[leftIndex].valueType != dsValueType::Bool)
                    return {false, midIndex}; // FIXME: error on type
                return {success, midIndex};
            default: DS_GUARD_OR(false, MidResult(false, midIndex), "Unknown unary operator");
            }
        }
        case AstType::Identifier: {
            MidIndex const midIndex{mid_.size()};
            Mid& mid = mid_.pushBack(Mid{.type = MidType::Variable, .astIndex = astIndex, .data = {.variable = {}}});

            Token const& identToken = tokens_[ast.primaryTokenIndex];
            char const* const identStart = expression_.data() + identToken.offset.value();
            char const* const identEnd = identStart + identToken.length;

            if (!host_.lookupVariable(dsName{identStart, identEnd}, mid.valueType))
            {
                // FIXME: error, invalid variable
                return {false, midIndex};
            }

            mid.data.variable.nameHash = dsHashFnv1a64(identStart, identEnd);

            return {true, midIndex};
        }
        case AstType::Group: return lower(ast.data.group.childIndex);
        case AstType::Call: {
            MidIndex const midIndex{mid_.size()};
            mid_.pushBack(Mid{.type = MidType::Call, .astIndex = astIndex, .data = {.call = {}}});

            Ast const& target = ast_[ast.data.call.targetIndex];
            if (target.type != AstType::Identifier)
            {
                // FIXME: error, can only call named functions
                return {false, midIndex};
            }

            Token const& identToken = tokens_[target.primaryTokenIndex];
            char const* const identStart = expression_.data() + identToken.offset.value();
            char const* const identEnd = identStart + identToken.length;

            if (!host_.lookupFunction(dsName{identStart, identEnd}, mid_[midIndex].data.call.functionId, mid_[midIndex].valueType))
            {
                // FIXME: error, invalid function
                return {false, midIndex};
            }

            bool success = true;

            MidLinkIndex lastLinkIndex = dsInvalidIndex;
            for (AstLinkIndex linkIndex = ast.data.call.firstArgIndex; linkIndex != dsInvalidIndex;
                 linkIndex = astLinks_[linkIndex].nextIndex)
            {
                auto const [argSuccess, midArgIndex] = lower(astLinks_[linkIndex].childIndex);
                success &= argSuccess;

                MidLinkIndex midLinkIndex{midLinks_.size()};
                midLinks_.pushBack(MidLink{.childIndex = midArgIndex});

                if (lastLinkIndex == dsInvalidIndex)
                    mid_[midIndex].data.call.firstArgIndex = midLinkIndex;
                else
                    midLinks_[lastLinkIndex].nextIndex = midLinkIndex;

                lastLinkIndex = midLinkIndex;
                ++mid_[midIndex].data.call.arity;
            }

            return {success, midIndex};
        }
        default: DS_GUARD_OR(false, MidResult(false, dsInvalidIndex), "Unknown ast node type");
        }
    }

    auto dsExpressionCompiler::optimize(MidIndex midIndex) -> MidIndex
    {
        switch (mid_[midIndex].type)
        {
        case MidType::UnaryOp: {
            MidIndex childIndex = optimize(mid_[midIndex].data.unary.childIndex);

            // we can only further optimize literals
            if (mid_[midIndex].type == MidType::Literal)
            {
                if (mid_[midIndex].data.unary.op == Operator::Negate && mid_[childIndex].data.literal.type == LiteralType::Integer)
                {
                    int64_t const value = mid_[childIndex].data.literal.value.s64;

                    mid_[midIndex].type = MidType::Literal;
                    mid_[midIndex].data = {.literal = {.type = LiteralType::Integer, .value = {.s64 = -value}}};
                    return midIndex;
                }

                if (mid_[midIndex].data.unary.op == Operator::Not && mid_[childIndex].data.literal.type == LiteralType::Boolean)
                {
                    bool const value = mid_[childIndex].data.literal.value.b;

                    mid_[midIndex].type = MidType::Literal;
                    mid_[midIndex].data = {.literal = {.type = LiteralType::Boolean, .value = {.b = !value}}};
                    return midIndex;
                }
            }

            mid_[midIndex].data.unary.childIndex = childIndex;
            break;
        }
        case MidType::BinaryOp: {
            Operator const op = mid_[midIndex].data.binary.op;

            MidIndex leftChildIndex = optimize(mid_[midIndex].data.binary.leftIndex);
            MidIndex rightChildIndex = optimize(mid_[midIndex].data.binary.rightIndex);

            // we can fold literals for some operators (in all cases, only when operands are the same type -- FIXME: not a good assumption)
            if (mid_[leftChildIndex].type == MidType::Literal && mid_[rightChildIndex].type == MidType::Literal &&
                mid_[leftChildIndex].data.literal.type == mid_[rightChildIndex].data.literal.type)
            {
                LiteralType const litType = mid_[leftChildIndex].data.literal.type;

                // arithmetic operations on integers (we don't handle division yet because we're integer-only
                if (litType == LiteralType::Integer && (op == Operator::Add || op == Operator::Sub || op == Operator::Mul))
                {
                    MidIndex const newIndex{mid_.size()};
                    mid_.pushBack(Mid{.type = MidType::Literal,
                        .astIndex = mid_[midIndex].astIndex,
                        .data = {.literal = {.type = litType, .value = {.s64 = 0}}}});

                    // FIXME: handle overflow
                    switch (mid_[midIndex].data.binary.op)
                    {
                    case Operator::Add:
                        mid_[newIndex].data.literal.value.s64 =
                            mid_[leftChildIndex].data.literal.value.s64 + mid_[rightChildIndex].data.literal.value.s64;
                        break;
                    case Operator::Sub:
                        mid_[newIndex].data.literal.value.s64 =
                            mid_[leftChildIndex].data.literal.value.s64 - mid_[rightChildIndex].data.literal.value.s64;
                        break;
                    case Operator::Mul:
                        mid_[newIndex].data.literal.value.s64 =
                            mid_[leftChildIndex].data.literal.value.s64 * mid_[rightChildIndex].data.literal.value.s64;
                        break;
                    default: DS_GUARD_OR(false, midIndex, "Unexpected operator");
                    }

                    return newIndex;
                }

                // logical operations on booleans
                if (litType == LiteralType::Boolean && (op == Operator::Or || op == Operator::And || op == Operator::Xor))
                {
                    MidIndex const newIndex{mid_.size()};
                    mid_.pushBack(Mid{.type = MidType::Literal,
                        .astIndex = mid_[midIndex].astIndex,
                        .data = {.literal = {.type = litType, .value = {.s64 = 0}}}});

                    // FIXME: handle overflow
                    switch (mid_[midIndex].data.binary.op)
                    {
                    case Operator::And:
                        mid_[newIndex].data.literal.value.b =
                            mid_[leftChildIndex].data.literal.value.b && mid_[rightChildIndex].data.literal.value.b;
                        break;
                    case Operator::Or:
                        mid_[newIndex].data.literal.value.b =
                            mid_[leftChildIndex].data.literal.value.b || mid_[rightChildIndex].data.literal.value.b;
                        break;
                    case Operator::Xor:
                        mid_[newIndex].data.literal.value.b =
                            mid_[leftChildIndex].data.literal.value.b ^ mid_[rightChildIndex].data.literal.value.b;
                        break;
                    default: DS_GUARD_OR(false, midIndex, "Unexpected operator");
                    }

                    return newIndex;
                }
            }

            mid_[midIndex].data.binary.leftIndex = leftChildIndex;
            mid_[midIndex].data.binary.rightIndex = rightChildIndex;
            break;
        }
        case MidType::Call: {
            MidIndex newCallIndex = midIndex;
            MidIndex lastArgIndex = dsInvalidIndex;

            for (MidLinkIndex linkIndex = mid_[midIndex].data.call.firstArgIndex; linkIndex != dsInvalidIndex;
                 linkIndex = midLinks_[linkIndex].nextIndex)
            {
                MidIndex newArgIndex = optimize(midLinks_[linkIndex].childIndex);
                midLinks_[linkIndex].childIndex = newArgIndex;
            }

            // this might not be "new" but just a copy of the original index
            return newCallIndex;
        }
        default: break;
        }

        // no optimization was performed, so we return our original node
        return midIndex;
    }

    bool dsExpressionCompiler::generate(MidIndex midIndex, dsExpressionBuilder& builder) const
    {
        Mid const& mid = mid_[midIndex];
        switch (mid.type)
        {
        case MidType::Literal:
            if (mid.data.literal.type == LiteralType::Boolean)
            {
                builder.pushOp((uint8_t)(mid.data.literal.value.b ? dsOpCode::PushTrue : dsOpCode::PushFalse));
                return true;
            }

            if (mid.data.literal.type == LiteralType::Nil)
            {
                builder.pushOp((uint8_t)dsOpCode::PushNil);
                return true;
            }

            if (mid.data.literal.type == LiteralType::Integer)
            {
                int64_t const value = mid.data.literal.value.s64;

                switch (value)
                {
                case 0: builder.pushOp((uint8_t)dsOpCode::Push0); break;
                case 1: builder.pushOp((uint8_t)dsOpCode::Push1); break;
                case 2: builder.pushOp((uint8_t)dsOpCode::Push2); break;
                case -1: builder.pushOp((uint8_t)dsOpCode::PushNeg1); break;
                default:
                    if (value >= 0)
                    {
                        if (value <= UINT8_MAX)
                        {
                            uint8_t const value8 = static_cast<uint8_t>(value);

                            builder.pushOp((uint8_t)dsOpCode::PushU8);
                            builder.pushOp((uint8_t)value8);
                            break;
                        }

                        if (value <= UINT16_MAX)
                        {
                            uint16_t const value16 = static_cast<uint16_t>(value);

                            builder.pushOp((uint8_t)dsOpCode::PushU16);
                            builder.pushOp((uint8_t)(value16 >> 8));
                            builder.pushOp((uint8_t)(value16 & 0xff));
                            break;
                        }
                    }
                    else
                    {
                        if (value >= INT8_MIN)
                        {
                            uint8_t const value8 = static_cast<int8_t>(value);

                            builder.pushOp((uint8_t)dsOpCode::PushS8);
                            builder.pushOp((uint8_t)value8);
                            break;
                        }

                        if (value >= INT16_MAX)
                        {
                            uint16_t const value16 = static_cast<int16_t>(value);

                            builder.pushOp((uint8_t)dsOpCode::PushS16);
                            builder.pushOp((uint8_t)(value16 >> 8));
                            builder.pushOp((uint8_t)(value16 & 0xff));
                            break;
                        }
                    }

                    {
                        dsExpressionConstantIndex const index = builder.pushConstant(dsValue{static_cast<double>(value)});
                        if (index == dsInvalidIndex)

                        {
                            // FIXME: error, constant overflow
                            return false;
                        }

                        builder.pushOp((uint8_t)dsOpCode::PushConstant);
                        builder.pushOp((uint8_t)(index.value() >> 8));
                        builder.pushOp((uint8_t)(index.value() & 0xff));
                    }
                }
                return true;
            }

            DS_GUARD_OR(false, false, "Unexpected literal type");
        case MidType::Variable: {
            dsExpressionVariableIndex const index = builder.pushVariable(mid.data.variable.nameHash);
            if (index == dsInvalidIndex)
            {
                // FIXME: error, variable overflow
                return false;
            }

            builder.pushOp((uint8_t)dsOpCode::Read);
            builder.pushOp((uint8_t)(index.value() >> 8));
            builder.pushOp((uint8_t)(index.value() & 0xff));
            return true;
        }
        case MidType::BinaryOp:
            if (!generate(mid.data.binary.leftIndex, builder))
                return false;
            if (!generate(mid.data.binary.rightIndex, builder))
                return false;
            switch (mid.data.binary.op)
            {
            case Operator::Add: builder.pushOp((uint8_t)dsOpCode::Add); break;
            case Operator::Sub: builder.pushOp((uint8_t)dsOpCode::Sub); break;
            case Operator::Mul: builder.pushOp((uint8_t)dsOpCode::Mul); break;
            case Operator::Div: builder.pushOp((uint8_t)dsOpCode::Div); break;
            case Operator::And: builder.pushOp((uint8_t)dsOpCode::And); break;
            case Operator::Or: builder.pushOp((uint8_t)dsOpCode::Or); break;
            case Operator::Xor: builder.pushOp((uint8_t)dsOpCode::Xor); break;
            default: DS_GUARD_OR(false, false, "Unknown binary operator type");
            }
            return true;
        case MidType::UnaryOp:
            if (!generate(mid.data.unary.childIndex, builder))
                return false;
            switch (mid.data.unary.op)
            {
            case Operator::Negate: builder.pushOp((uint8_t)dsOpCode::Neg); break;
            case Operator::Not: builder.pushOp((uint8_t)dsOpCode::Not); break;
            default: DS_GUARD_OR(false, false, "Unknown unary operator type");
            }
            return true;
        case MidType::Call: {
            for (MidLinkIndex linkIndex = mid.data.call.firstArgIndex; linkIndex != dsInvalidIndex;
                 linkIndex = midLinks_[linkIndex].nextIndex)
            {
                if (!generate(midLinks_[linkIndex].childIndex, builder))
                    return false;
            }

            dsExpressionFunctionIndex const index = builder.pushFunction(mid.data.call.functionId);
            if (index == dsInvalidIndex)
            {
                // FIXME: error, function overflow
                return false;
            }

            builder.pushOp((uint8_t)dsOpCode::Call);
            builder.pushOp((uint8_t)(index.value() >> 8));
            builder.pushOp((uint8_t)(index.value() & 0xff));
            builder.pushOp(mid.data.call.arity);
            return true;
        }
        default: DS_GUARD_OR(false, false, "Unknown AST node type");
        }
    }

    bool dsExpressionCompiler::isConstant() const noexcept
    {
        DS_GUARD_OR(midRoot_ != dsInvalidIndex, false);
        return mid_[midRoot_].type == MidType::Literal;
    }

    bool dsExpressionCompiler::isVariableOnly() const noexcept
    {
        DS_GUARD_OR(midRoot_ != dsInvalidIndex, false);
        return mid_[midRoot_].type == MidType::Variable;
    }

    dsValueType dsExpressionCompiler::resultType() const noexcept
    {
        DS_GUARD_OR(midRoot_ != dsInvalidIndex, dsValueType::Nil);
        return mid_[midRoot_].valueType;
    }

    bool dsExpressionCompiler::asConstant(dsValue& out_value) const
    {

        DS_GUARD_OR(midRoot_ != dsInvalidIndex, false);

        if (mid_[midRoot_].type != MidType::Literal)
            return false;

        switch (mid_[midRoot_].data.literal.type)
        {
        case LiteralType::Nil: out_value = nullptr; return true;
        case LiteralType::Boolean: out_value = mid_[midRoot_].data.literal.value.b; return true;
        case LiteralType::Integer: out_value = static_cast<double>(mid_[midRoot_].data.literal.value.s64); return true;
        default: DS_GUARD_OR(false, false, "Unrecognized literal data type");
        }
    }
} // namespace descript
