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
        astLinks_.clear();
        expression_.reset();
        nextToken_ = dsInvalidIndex;
        astRoot_ = dsInvalidIndex;
        isLowered_ = false;
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

        auto const [success, loweredRootIndex] = lower(astRoot_);
        if (loweredRootIndex != dsInvalidIndex)
            astRoot_ = loweredRootIndex;

        isLowered_ = success;
        return success;
    }

    bool dsExpressionCompiler::optimize()
    {
        DS_GUARD_OR(astRoot_ != dsInvalidIndex, false);
        DS_GUARD_OR(isLowered_, false);

        AstIndex const optimizedAstIndex = optimize(astRoot_);
        if (optimizedAstIndex == dsInvalidIndex)
            return false;

        astRoot_ = optimizedAstIndex;
        return true;
    }

    bool dsExpressionCompiler::build(dsExpressionBuilder& builder)
    {
        DS_GUARD_OR(astRoot_ != dsInvalidIndex, false);
        DS_GUARD_OR(isLowered_, false);

        if (!generate(astRoot_, builder))
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
            ast_.pushBack(Ast{.type = AstType::Literal, .primaryTokenIndex = nextToken_});
            ++nextToken_;
            break;
        case TokenType::KeyTrue:
        case TokenType::KeyFalse:
            leftIndex = AstIndex{ast_.size()};
            ast_.pushBack(Ast{.type = AstType::Literal, .primaryTokenIndex = nextToken_});
            ++nextToken_;
            break;
        case TokenType::KeyNil:
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

    auto dsExpressionCompiler::lower(AstIndex astIndex) -> LowerResult
    {
        DS_GUARD_OR(!isLowered_, LowerResult(false, astIndex))
        DS_GUARD_OR(astIndex != dsInvalidIndex, LowerResult(false, astIndex));

        switch (ast_[astIndex].type)
        {
        case AstType::Literal: {
            Ast& ast = ast_[astIndex];

            switch (tokens_[ast.primaryTokenIndex].type)
            {
            case TokenType::KeyTrue:
            case TokenType::KeyFalse:
                ast.type = AstType::Constant;
                ast.valueType = dsValueType::Bool;
                ast.data.constant = {.bool_ = tokens_[ast.primaryTokenIndex].type == TokenType::KeyTrue};
                return {true, astIndex};
            case TokenType::KeyNil:
                ast.type = AstType::Constant;
                ast.valueType = dsValueType::Nil;
                return {true, astIndex};
            case TokenType::LiteralInt:
                ast.type = AstType::Constant;
                ast.valueType = dsValueType::Double;
                ast.data.constant = {.int64_ = tokens_[ast.primaryTokenIndex].data.literalInt};
                return {true, astIndex};
            default: DS_GUARD_OR(false, LowerResult(false, astIndex), "Unknown literal token type");
            }
        }
        case AstType::UnaryOp: {
            auto const [success, operandIndex] = lower(ast_[astIndex].data.unary.childIndex);

            Ast& ast = ast_[astIndex];

            ast.data.unary.childIndex = operandIndex;

            switch (ast.data.unary.op)
            {
            case Operator::Negate:
                ast.valueType = dsValueType::Double;
                if (ast_[operandIndex].valueType != dsValueType::Double)
                    return {false, astIndex}; // FIXME: error on type
                return {success, astIndex};
            case Operator::Not:
                ast.valueType = dsValueType::Bool;
                if (ast_[operandIndex].valueType != dsValueType::Bool)
                    return {false, astIndex}; // FIXME: error on type
                return {success, astIndex};
            default: DS_GUARD_OR(false, LowerResult(false, astIndex), "Unknown unary operator");
            }
        }
        case AstType::BinaryOp: {
            auto const [leftSuccess, leftIndex] = lower(ast_[astIndex].data.binary.leftIndex);
            auto const [rightSuccess, rightIndex] = lower(ast_[astIndex].data.binary.rightIndex);
            bool const success = leftSuccess && rightSuccess;

            Ast& ast = ast_[astIndex];

            ast.data.binary.leftIndex = leftIndex;
            ast.data.binary.rightIndex = rightIndex;

            // FIXME: don't assume all binary operations are homogenously typed
            if (ast_[leftIndex].valueType != ast_[rightIndex].valueType)
                return {false, astIndex}; // FIXME: type error

            switch (ast.data.unary.op)
            {
            case Operator::Add:
            case Operator::Sub:
            case Operator::Mul:
            case Operator::Div:
                ast.valueType = dsValueType::Double;
                if (ast_[leftIndex].valueType != dsValueType::Double)
                    return {false, astIndex}; // FIXME: error on type
                return {success, astIndex};
            case Operator::And:
            case Operator::Or:
            case Operator::Xor:
                ast.valueType = dsValueType::Bool;
                if (ast_[leftIndex].valueType != dsValueType::Bool)
                    return {false, astIndex}; // FIXME: error on type
                return {success, astIndex};
            default: DS_GUARD_OR(false, LowerResult(false, astIndex), "Unknown unary operator");
            }
        }
        case AstType::Identifier: {
            Ast& ast = ast_[astIndex];

            Token const& identToken = tokens_[ast.primaryTokenIndex];
            char const* const identStart = expression_.data() + identToken.offset.value();
            char const* const identEnd = identStart + identToken.length;

            if (!host_.lookupVariable(dsName{identStart, identEnd}, ast.valueType))
            {
                // FIXME: error, invalid variable
                return {false, astIndex};
            }

            ast.type = AstType::Variable;
            ast.data = {.variable = {.nameHash = dsHashFnv1a64(identStart, identEnd)}};

            return {true, astIndex};
        }
        case AstType::Group: return lower(ast_[astIndex].data.group.childIndex);
        case AstType::Call: {
            Ast const& target = ast_[ast_[astIndex].data.call.targetIndex];
            if (target.type != AstType::Identifier)
            {
                // FIXME: error, can only call named functions
                return {false, astIndex};
            }

            Token const& identToken = tokens_[target.primaryTokenIndex];
            char const* const identStart = expression_.data() + identToken.offset.value();
            char const* const identEnd = identStart + identToken.length;

            dsFunctionId functionId = dsInvalidFunctionId;
            dsValueType resultType = dsValueType::Nil;
            if (!host_.lookupFunction(dsName{identStart, identEnd}, functionId, resultType))
            {
                // FIXME: error, invalid function
                return {false, astIndex};
            }

            bool success = true;

            AstLinkIndex firstArgIndex = dsInvalidIndex;
            AstLinkIndex lastLinkIndex = dsInvalidIndex;
            uint8_t arity = 0;
            for (AstLinkIndex linkIndex = ast_[astIndex].data.call.firstArgIndex; linkIndex != dsInvalidIndex;
                 linkIndex = astLinks_[linkIndex].nextIndex)
            {
                auto const [argSuccess, argIndex] = lower(astLinks_[linkIndex].childIndex);
                success &= argSuccess;

                AstLinkIndex newLinkIndex{astLinks_.size()};
                astLinks_.pushBack(AstLink{.childIndex = argIndex});

                if (firstArgIndex == dsInvalidIndex)
                    firstArgIndex = newLinkIndex;
                else
                    astLinks_[lastLinkIndex].nextIndex = newLinkIndex;

                lastLinkIndex = newLinkIndex;
                ++arity;
            }

            Ast& ast = ast_[astIndex];
            ast.type = AstType::Call;
            ast.valueType = resultType;
            ast.data = {.function = {.functionId = functionId, .firstArgIndex = firstArgIndex, .arity = arity}};
            return {success, astIndex};
        }
        default: DS_GUARD_OR(false, LowerResult(false, dsInvalidIndex), "Unknown ast node type");
        }
    }

    auto dsExpressionCompiler::optimize(AstIndex astIndex) -> AstIndex
    {
        switch (ast_[astIndex].type)
        {
        case AstType::UnaryOp: {
            AstIndex childIndex = optimize(ast_[astIndex].data.unary.childIndex);

            // we can only further optimize constants
            if (ast_[astIndex].type == AstType::Constant)
            {
                if (ast_[astIndex].data.unary.op == Operator::Negate && ast_[childIndex].valueType == dsValueType::Double)
                {
                    int64_t const value = ast_[childIndex].data.constant.int64_;

                    ast_[astIndex].type = AstType::Constant;
                    ast_[astIndex].valueType = dsValueType::Double;
                    ast_[astIndex].data = {.constant = {.int64_ = -value}};
                    return astIndex;
                }

                if (ast_[astIndex].data.unary.op == Operator::Not && ast_[childIndex].valueType == dsValueType::Bool)
                {
                    bool const value = ast_[childIndex].data.constant.bool_;

                    ast_[astIndex].type = AstType::Constant;
                    ast_[astIndex].valueType = dsValueType::Bool;
                    ast_[astIndex].data = {.constant = {.bool_ = !value}};
                    return astIndex;
                }
            }

            ast_[astIndex].data.unary.childIndex = childIndex;
            break;
        }
        case AstType::BinaryOp: {
            Operator const op = ast_[astIndex].data.binary.op;

            AstIndex leftChildIndex = optimize(ast_[astIndex].data.binary.leftIndex);
            AstIndex rightChildIndex = optimize(ast_[astIndex].data.binary.rightIndex);

            // we can fold literals for some operators (in all cases, only when operands are the same type -- FIXME: not a good assumption)
            if (ast_[leftChildIndex].type == AstType::Constant && ast_[rightChildIndex].type == AstType::Constant &&
                ast_[leftChildIndex].valueType == ast_[rightChildIndex].valueType)
            {
                dsValueType const valueType = ast_[leftChildIndex].valueType;

                Ast& ast = ast_[astIndex];

                // arithmetic operations on integers (we don't handle division yet because we're integer-only
                if (valueType == dsValueType::Double && (op == Operator::Add || op == Operator::Sub || op == Operator::Mul))
                {
                    int64_t const left = ast_[leftChildIndex].data.constant.int64_;
                    int64_t const right = ast_[rightChildIndex].data.constant.int64_;

                    // FIXME: handle overflow
                    switch (ast_[astIndex].data.binary.op)
                    {
                    case Operator::Add:
                        ast.type = AstType::Constant;
                        ast.data = {.constant = {.int64_ = left + right}};
                        break;
                    case Operator::Sub:
                        ast.type = AstType::Constant;
                        ast.data = {.constant = {.int64_ = left - right}};
                        break;
                    case Operator::Mul:
                        ast.type = AstType::Constant;
                        ast.data = {.constant = {.int64_ = left * right}};
                        break;
                    default: DS_GUARD_OR(false, astIndex, "Unexpected operator");
                    }

                    return astIndex;
                }

                // logical operations on booleans
                if (valueType == dsValueType::Bool && (op == Operator::Or || op == Operator::And || op == Operator::Xor))
                {
                    bool const left = ast_[leftChildIndex].data.constant.bool_;
                    bool const right = ast_[rightChildIndex].data.constant.bool_;

                    switch (ast_[astIndex].data.binary.op)
                    {
                    case Operator::And:
                        ast.type = AstType::Constant;
                        ast.data = {.constant = {.bool_ = left && right}};
                        break;
                    case Operator::Or:
                        ast.type = AstType::Constant;
                        ast.data = {.constant = {.bool_ = left || right}};
                        break;
                    case Operator::Xor:
                        ast.type = AstType::Constant;
                        ast.data = {.constant = {.bool_ = static_cast<bool>(left ^ right)}};
                        break;
                    default: DS_GUARD_OR(false, astIndex, "Unexpected operator");
                    }

                    return astIndex;
                }
            }

            ast_[astIndex].data.binary.leftIndex = leftChildIndex;
            ast_[astIndex].data.binary.rightIndex = rightChildIndex;
            break;
        }
        case AstType::Function: {
            AstIndex newCallIndex = astIndex;
            AstIndex lastArgIndex = dsInvalidIndex;

            for (AstLinkIndex linkIndex = ast_[astIndex].data.function.firstArgIndex; linkIndex != dsInvalidIndex;
                 linkIndex = astLinks_[linkIndex].nextIndex)
            {
                AstIndex newArgIndex = optimize(astLinks_[linkIndex].childIndex);
                astLinks_[linkIndex].childIndex = newArgIndex;
            }

            // this might not be "new" but just a copy of the original index
            return newCallIndex;
        }
        default: break;
        }

        // no optimization was performed, so we return our original node
        return astIndex;
    }

    bool dsExpressionCompiler::generate(AstIndex astIndex, dsExpressionBuilder& builder) const
    {
        Ast const& ast = ast_[astIndex];
        switch (ast.type)
        {
        case AstType::Constant:
            if (ast.valueType == dsValueType::Bool)
            {
                builder.pushOp((uint8_t)(ast.data.constant.bool_ ? dsOpCode::PushTrue : dsOpCode::PushFalse));
                return true;
            }

            if (ast.valueType == dsValueType::Nil)
            {
                builder.pushOp((uint8_t)dsOpCode::PushNil);
                return true;
            }

            if (ast.valueType == dsValueType::Double)
            {
                int64_t const value = ast.data.constant.int64_;

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
        case AstType::Variable: {
            dsExpressionVariableIndex const index = builder.pushVariable(ast.data.variable.nameHash);
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
        case AstType::BinaryOp:
            if (!generate(ast.data.binary.leftIndex, builder))
                return false;
            if (!generate(ast.data.binary.rightIndex, builder))
                return false;
            switch (ast.data.binary.op)
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
        case AstType::UnaryOp:
            if (!generate(ast.data.unary.childIndex, builder))
                return false;
            switch (ast.data.unary.op)
            {
            case Operator::Negate: builder.pushOp((uint8_t)dsOpCode::Neg); break;
            case Operator::Not: builder.pushOp((uint8_t)dsOpCode::Not); break;
            default: DS_GUARD_OR(false, false, "Unknown unary operator type");
            }
            return true;
        case AstType::Call: {
            for (AstLinkIndex linkIndex = ast.data.function.firstArgIndex; linkIndex != dsInvalidIndex;
                 linkIndex = astLinks_[linkIndex].nextIndex)
            {
                if (!generate(astLinks_[linkIndex].childIndex, builder))
                    return false;
            }

            dsExpressionFunctionIndex const index = builder.pushFunction(ast.data.function.functionId);
            if (index == dsInvalidIndex)
            {
                // FIXME: error, function overflow
                return false;
            }

            builder.pushOp((uint8_t)dsOpCode::Call);
            builder.pushOp((uint8_t)(index.value() >> 8));
            builder.pushOp((uint8_t)(index.value() & 0xff));
            builder.pushOp(ast.data.function.arity);
            return true;
        }
        default: DS_GUARD_OR(false, false, "Unknown AST node type");
        }
    }

    bool dsExpressionCompiler::isConstant() const noexcept
    {
        DS_GUARD_OR(astRoot_ != dsInvalidIndex, false);
        return ast_[astRoot_].type == AstType::Literal;
    }

    bool dsExpressionCompiler::isVariableOnly() const noexcept
    {
        DS_GUARD_OR(astRoot_ != dsInvalidIndex, false);
        return ast_[astRoot_].type == AstType::Variable;
    }

    dsValueType dsExpressionCompiler::resultType() const noexcept
    {
        DS_GUARD_OR(astRoot_ != dsInvalidIndex, dsValueType::Nil);
        return ast_[astRoot_].valueType;
    }

    bool dsExpressionCompiler::asConstant(dsValue& out_value) const
    {

        DS_GUARD_OR(astRoot_ != dsInvalidIndex, false);

        if (ast_[astRoot_].type != AstType::Constant)
            return false;

        switch (ast_[astRoot_].valueType)
        {
        case dsValueType::Nil: out_value = nullptr; return true;
        case dsValueType::Bool: out_value = ast_[astRoot_].data.constant.bool_; return true;
        case dsValueType::Double: out_value = static_cast<double>(ast_[astRoot_].data.constant.int64_); return true;
        default: DS_GUARD_OR(false, false, "Unrecognized literal data type");
        }
    }
} // namespace descript
