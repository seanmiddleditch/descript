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
        astRoot_ = dsInvalidIndex;
        optimizedAstRoot_ = dsInvalidIndex;
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

        return true;
    }

    bool dsExpressionCompiler::build()
    {
        DS_GUARD_OR(astRoot_ != dsInvalidIndex, false);

        optimizedAstRoot_ = optimize(astRoot_);
        if (optimizedAstRoot_ == dsInvalidIndex)
            return false;

        if (!generate(optimizedAstRoot_, builder_))
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
            ast_.pushBack(
                Ast{.type = AstType::Literal, .primaryTokenIndex = nextToken_, .data = {.literal{.value = leftToken.data.literalInt}}});
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

    auto dsExpressionCompiler::optimize(AstIndex astIndex) -> AstIndex
    {
        switch (ast_[astIndex].type)
        {
        case AstType::UnaryOp: {
            AstIndex childIndex = optimize(ast_[astIndex].data.unary.childIndex);

            // we can fold literals operands
            if (ast_[childIndex].type == AstType::Literal)
            {
                AstIndex const newIndex{ast_.size()};
                ast_.pushBack(Ast{.type = AstType::Literal,
                    .primaryTokenIndex = ast_[astIndex].primaryTokenIndex,
                    .data = {.literal = {.value = 0}}});

                switch (ast_[astIndex].data.unary.op)
                {
                case Operator::Negate: ast_[newIndex].data.literal.value = -ast_[childIndex].data.literal.value; break;
                case Operator::Not: ast_[newIndex].data.literal.value = ast_[childIndex].data.literal.value ? 0 : 1; break;
                default: DS_GUARD_OR(false, astIndex, "Unexpected operator");
                }

                return newIndex;
            }

            // if our child was optimized, we need to create a new node with
            // the new child index
            if (childIndex != ast_[astIndex].data.unary.childIndex)
            {
                AstIndex const newIndex{ast_.size()};
                ast_.pushBack(Ast{.type = AstType::UnaryOp,
                    .primaryTokenIndex = ast_[astIndex].primaryTokenIndex,
                    .data = {.unary = {.op = ast_[astIndex].data.unary.op, .childIndex = childIndex}}});
                return newIndex;
            }

            break;
        }
        case AstType::BinaryOp: {
            AstIndex leftChildIndex = optimize(ast_[astIndex].data.binary.leftIndex);
            AstIndex rightChildIndex = optimize(ast_[astIndex].data.binary.rightIndex);

            // we can fold literals for some operators (not division, since we don't do floating math in the compiler)
            if (ast_[astIndex].data.binary.op != Operator::Div && ast_[leftChildIndex].type == AstType::Literal &&
                ast_[rightChildIndex].type == AstType::Literal)
            {
                AstIndex const newIndex{ast_.size()};
                ast_.pushBack(Ast{.type = AstType::Literal,
                    .primaryTokenIndex = ast_[astIndex].primaryTokenIndex,
                    .data = {.literal = {.value = 0}}});

                // FIXME: handle overflow
                switch (ast_[astIndex].data.binary.op)
                {
                case Operator::Add:
                    ast_[newIndex].data.literal.value = ast_[leftChildIndex].data.literal.value + ast_[rightChildIndex].data.literal.value;
                    break;
                case Operator::Sub:
                    ast_[newIndex].data.literal.value = ast_[leftChildIndex].data.literal.value - ast_[rightChildIndex].data.literal.value;
                    break;
                case Operator::Mul:
                    ast_[newIndex].data.literal.value = ast_[leftChildIndex].data.literal.value * ast_[rightChildIndex].data.literal.value;
                    break;
                default: DS_GUARD_OR(false, astIndex, "Unexpected operator");
                }

                return newIndex;
            }

            // if either child was optimized, we need to create a new node with
            // the new child indices
            if (leftChildIndex != ast_[astIndex].data.binary.leftIndex || rightChildIndex != ast_[astIndex].data.binary.rightIndex)
            {
                AstIndex const newIndex{ast_.size()};
                ast_.pushBack(Ast{.type = AstType::BinaryOp,
                    .primaryTokenIndex = ast_[astIndex].primaryTokenIndex,
                    .data = {.binary = {.op = ast_[astIndex].data.unary.op, .leftIndex = leftChildIndex, .rightIndex = rightChildIndex}}});
                return newIndex;
            }

            break;
        }
        case AstType::Group:
            // not really an "optimization" but it does simplify the AST
            // for parent optimization analysis
            return ast_[astIndex].data.group.childIndex;
        case AstType::Call: {
            AstIndex newCallIndex = astIndex;
            AstIndex lastArgIndex = dsInvalidIndex;

            for (AstIndex argIndex = ast_[astIndex].data.call.firstArgIndex; argIndex != dsInvalidIndex;
                 argIndex = ast_[argIndex].nextArgIndex)
            {
                AstIndex newArgIndex = optimize(argIndex);

                // if the child is unmodified, we have nothing further to do
                if (newArgIndex == argIndex)
                {
                    lastArgIndex = argIndex;
                    continue;
                }

                // if we're already part of a rewritten call chain, just link in the new arg
                if (newCallIndex != astIndex)
                {
                    if (lastArgIndex != dsInvalidIndex)
                        ast_[lastArgIndex].nextArgIndex = newArgIndex;
                    else
                        ast_[newCallIndex].data.call.firstArgIndex = newArgIndex;

                    lastArgIndex = newArgIndex;
                    continue;
                }

                // we'll need to rewrite the call with the new arguments since we have an optimized child
                newCallIndex = AstIndex{ast_.size()};
                ast_.pushBack(Ast{.type = AstType::Call,
                    .primaryTokenIndex = ast_[astIndex].primaryTokenIndex,
                    .data = {.call = ast_[astIndex].data.call}});

                // if the new child is the _first_ arg, we have it easy, just write the new
                // first child and we're done
                if (argIndex == ast_[astIndex].data.call.firstArgIndex)
                {
                    ast_[newCallIndex].data.call.firstArgIndex = newArgIndex;

                    lastArgIndex = newArgIndex;
                    continue;
                }

                // the new child has some prior siblings which will need to all be rewritten
                // to form a new linked list
                {
                    // first, create the new head node
                    AstIndex const oldHeadIndex = ast_[astIndex].data.call.firstArgIndex;
                    AstIndex const newHeadIndex = AstIndex{ast_.size()};
                    ast_[newCallIndex].data.call.firstArgIndex = newHeadIndex;
                    ast_.pushBack(ast_[oldHeadIndex]);

                    AstIndex lastRewrittenArgIndex = newHeadIndex;

                    // create new arg nodes for each preceding arg sibling (after the head)
                    for (AstIndex oldArgIndex = ast_[oldHeadIndex].nextArgIndex; oldArgIndex != argIndex;
                         oldArgIndex = ast_[oldArgIndex].nextArgIndex)
                    {
                        AstIndex const newRewrittenArgIndex{ast_.size()};
                        ast_.pushBack(ast_[oldArgIndex]);
                        ast_[lastRewrittenArgIndex].nextArgIndex = newRewrittenArgIndex;

                        lastRewrittenArgIndex = newRewrittenArgIndex;
                    }

                    // link ourselves in on the new chain
                    ast_[lastRewrittenArgIndex].nextArgIndex = newArgIndex;
                }

                lastArgIndex = newArgIndex;
            }

            // this might not be "new" but just a copy of the original index
            return newCallIndex;
        }
        default: break;
        }

        // no optimization was performed, so we return our original node
        return astIndex;
    }

    bool dsExpressionCompiler::generate(AstIndex astIndex, dsExpressionBuilder& builder)
    {
        Ast const& ast = ast_[astIndex];
        switch (ast.type)
        {
        case AstType::Literal:
            switch (ast.data.literal.value)
            {
            case 0: builder.pushOp((uint8_t)dsOpCode::Push0); break;
            case 1: builder.pushOp((uint8_t)dsOpCode::Push1); break;
            case 2: builder.pushOp((uint8_t)dsOpCode::Push2); break;
            case -1: builder.pushOp((uint8_t)dsOpCode::PushNeg1); break;
            default:
                if (ast.data.literal.value >= 0)
                {
                    int64_t const value = ast.data.literal.value;

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
                    int64_t const value = ast.data.literal.value;

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
                    dsExpressionConstantIndex const index = builder.pushConstant(dsValue{static_cast<double>(ast.data.literal.value)});
                    if (index == dsInvalidIndex)

                    {
                        // FIXME: error, constant overflow
                        return false;
                    }

                    builder.pushOp((uint8_t)dsOpCode::PushConstant);
                    builder.pushOp((uint8_t)(index.value() >> 8));
                    builder.pushOp((uint8_t)(index.value() & 0xff));
                }
                break;
            }
            return true;
        case AstType::Identifier: {
            Token const& identToken = tokens_[ast.primaryTokenIndex];
            char const* const identStart = expression_.data() + identToken.offset.value();
            char const* const identEnd = identStart + identToken.length;

            if (!host_.lookupVariable(dsName{identStart, identEnd}))
            {
                // FIXME: error, invalid variable
                return false;
            }

            uint64_t const nameHash = dsHashFnv1a64(identStart, identEnd);
            dsExpressionVariableIndex const index = builder.pushVariable(nameHash);
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
            default: DS_GUARD_OR(false, false, "Unknown binary operator type");
            }
            return true;
        case AstType::UnaryOp:
            if (!generate(ast.data.unary.childIndex, builder))
                return false;
            switch (ast.data.unary.op)
            {
            case Operator::Negate: builder.pushOp((uint8_t)dsOpCode::Neg); break;
            default: DS_GUARD_OR(false, false, "Unknown unary operator type");
            }
            return true;
        case AstType::Group: return generate(ast.data.group.childIndex, builder);
        case AstType::Call: {
            uint8_t argc = 0;
            for (AstIndex argIndex = ast.data.call.firstArgIndex; argIndex != dsInvalidIndex; argIndex = ast_[argIndex].nextArgIndex)
            {
                ++argc;
                if (!generate(argIndex, builder))
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

            dsExpressionFunctionIndex const index = builder.pushFunction(functionId);
            if (index == dsInvalidIndex)
            {
                // FIXME: error, function overflow
                return false;
            }

            builder.pushOp((uint8_t)dsOpCode::Call);
            builder.pushOp((uint8_t)(index.value() >> 8));
            builder.pushOp((uint8_t)(index.value() & 0xff));
            builder.pushOp(argc);
            return true;
        }
        default: DS_GUARD_OR(false, false, "Unknown AST node type");
        }
    }
} // namespace descript
