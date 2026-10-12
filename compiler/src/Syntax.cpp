#include "Syntax.h"

namespace {

[[noreturn]]
void syntaxError(std::size_t position, const std::string& message) {
    std::stringstream ss;
    ss << "Syntax error at position " << position << " : " << message << std::endl;
    throw std::runtime_error(ss.str());
}

} // namespace

Syntax::Syntax(std::vector<Token> inputTokens) : tokens(std::move(inputTokens)) {
    // 【补充：检查输入，保证 peek/advance 的边界安全】
    if (tokens.empty()) {
        throw std::invalid_argument(
            "Syntax requires a non-empty token sequence ending with End"
        );
    }

    if (tokens.back().kind != TokenKind::End) {
        throw std::invalid_argument(
            "Token sequence must end with End"
        );
    }

    // End 只能出现在最后，防止提前结束并忽略后续内容
    for (std::size_t i = 0; i + 1 < tokens.size(); i++) {
        if (tokens[i].kind == TokenKind::End) {
            throw std::invalid_argument(
                "End token must appear only at the end"
            );
        }
    }
}

const Token& Syntax::peek() const {
    return tokens[current];
}

Token Syntax::advance() {
    auto token = peek();

    if (token.kind != TokenKind::End) {
        current++;
    }

    return token;
}

bool Syntax::match(TokenKind kind) {
    if (peek().kind == kind) {
        advance();
        return true;
    }

    return false;
}

Token Syntax::expect(TokenKind kind) {
    auto token = peek();

    if (match(kind)) {
        return token;
    }

    syntaxError(token.position, "Expected " + tokenKindToString(kind) + " but got " + tokenKindToString(token.kind) + " ('" + token.text + "')");
}

// ==================== 普通表达式入口 ====================

std::unique_ptr<ASTNode> Syntax::parse() {
    auto node = parseExpression(0);

    expect(TokenKind::End);

    return node;
}

// ==================== 【新增：比较约束入口】 ====================

Constraint Syntax::parseConstraint() {
    Constraint constraint;

    // 1. 解析左侧。
    // parseExpression 遇到比较运算符会停止，不消耗它。
    constraint.left = parseExpression(0);

    // 2. 检查比较运算符。
    const auto token = peek();

    if (token.kind != TokenKind::Operator) {
        syntaxError(
            token.position,
            "Expected comparison operator (=, !=, >, <, >=, <=)"
        );
    }

    const auto& operators = operatorTable();
    const auto it = operators.find(token.text);

    if (it == operators.end() ||
        it->second.category != OperatorCategory::Comparison) {
        syntaxError(
            token.position,
            "Expected comparison operator but got '" +
            token.text + "'"
        );
    }

    constraint.relation = it->second.id;
    constraint.text = token.text;
    constraint.position = token.position;

    advance();

    // 3. 解析右侧。
    constraint.right = parseExpression(0);

    // 4. 不允许连续比较，例如 0 < x < 10。
    const auto& next = peek();

    if (next.kind == TokenKind::Operator) {
        const auto nextIt = operators.find(next.text);

        if (nextIt != operators.end() &&
            nextIt->second.category == OperatorCategory::Comparison) {
            syntaxError(
                next.position,
                "Chained comparisons are not supported; "
                "split them into separate constraints"
            );
        }
    }

    expect(TokenKind::End);

    return constraint;
}

// ==================== 基本表达式 ====================

std::unique_ptr<ASTNode> Syntax::parsePrimary() {
    const auto token = peek();

    switch (token.kind) {
        case TokenKind::Number: {
            auto node = std::make_unique<ASTNode>();

            node->kind = SyntaxKind::Number;
            node->text = token.text;
            node->position = token.position;
            node->number = token.number;

            advance();

            return node;
        }

        case TokenKind::Identifier: {
            advance(); // 消耗标识符

            // 【新增：区分变量与函数调用】
            if (match(TokenKind::LParen)) {
                // 到这里，名字和左括号都已经消耗。

                if (token.text == "sum" || token.text == "prod") {
                    return parseReduction(token);
                }

                return parseFunctionCall(token);
            }

            // 后面没有左括号，作为普通变量。
            auto node = std::make_unique<ASTNode>();

            node->kind = SyntaxKind::Variable;
            node->text = token.text;
            node->position = token.position;

            return node;
        }

        case TokenKind::LParen: {
            advance();

            auto node = parseExpression(0);

            expect(TokenKind::RParen);

            return node;
        }

        default: {
            syntaxError(
                token.position,
                "Expected primary expression but got " +
                tokenKindToString(token.kind) +
                " ('" + token.text + "')"
            );
        }
    }
}

// ==================== 【新增：普通函数调用】 ====================

std::unique_ptr<ASTNode> Syntax::parseFunctionCall(const Token& name) {
    auto node = std::make_unique<ASTNode>();

    node->kind = SyntaxKind::FunctionCall;
    node->text = name.text;
    node->position = name.position;

    // 允许语法形式 f()。
    // 函数是否允许零参数，由后续语义分析检查。
    if (match(TokenKind::RParen)) {
        return node;
    }

    while (true) {
        // 参数可以是任意算术表达式，包括嵌套调用。
        auto argument = parseExpression(0);
        node->arguments.push_back(std::move(argument));

        // 没有逗号，说明参数列表应结束。
        if (!match(TokenKind::Comma)) {
            break;
        }

        // 有逗号则继续解析下一个参数。
        // 对于 f(x,)，下一轮会在右括号处报缺少表达式。
    }

    expect(TokenKind::RParen);

    return node;
}

// ==================== 【新增：求和、连乘】 ====================

std::unique_ptr<ASTNode> Syntax::parseReduction(const Token& name) {
    auto node = std::make_unique<ASTNode>();

    node->kind = SyntaxKind::Reduction;
    node->text = name.text;
    node->position = name.position;

    node->reduction =
        name.text == "sum"
            ? ReductionKind::Sum
            : ReductionKind::Product;

    // 格式：
    // sum/prod ( 下标名 , 下界 , 上界 , 主体表达式 )
    //
    // 名字和左括号已由 parsePrimary 消耗。

    // 1. 下标必须是一个标识符。
    const auto indexToken = expect(TokenKind::Identifier);

    node->indexName = indexToken.text;
    node->indexPosition = indexToken.position;

    expect(TokenKind::Comma);

    // 2. 下界。
    node->lower = parseExpression(0);

    expect(TokenKind::Comma);

    // 3. 上界。
    node->upper = parseExpression(0);

    expect(TokenKind::Comma);

    // 4. 主体。
    node->body = parseExpression(0);

    expect(TokenKind::RParen);

    // 此处只构建树。
    // 下标作用域、上下界是否为整数及循环求值，
    // 都留给后续语义分析和求值器。
    return node;
}

// ==================== 一元正负号 ====================

std::unique_ptr<ASTNode> Syntax::parseUnary() {
    const auto token = peek();

    if (token.kind == TokenKind::Operator &&
        (token.op == OperatorId::Add ||
         token.op == OperatorId::Subtract)) {

        advance();

        auto node = std::make_unique<ASTNode>();

        node->kind = SyntaxKind::UnaryOperator;
        node->text = token.text;
        node->position = token.position;
        node->op = token.op;

        node->right = parseUnary();

        return node;
    }

    return parsePrimary();
}

// ==================== 二元算术表达式 ====================

std::unique_ptr<ASTNode> Syntax::parseExpression(int minPrecedence) {
    auto left = parseUnary();

    while (true) {
        const auto token = peek();

        // 逗号、右括号、End 等，留给调用者处理。
        if (token.kind != TokenKind::Operator) {
            break;
        }

        const auto& operators = operatorTable();
        const auto it = operators.find(token.text);

        if (it == operators.end()) {
            syntaxError(
                token.position,
                "Unknown operator '" + token.text + "'"
            );
        }

        const auto& info = it->second;

        // 【比较约束支持】
        // 比较运算符交给 parseConstraint，不在这里消耗。
        if (info.category != OperatorCategory::Arithmetic) {
            break;
        }

        if (info.precedence < minPrecedence) {
            break;
        }

        // 当前四则运算均为左结合。
        // 也支持未来添加右结合的算术运算符。
        // 非结合算术运算需要额外设计，避免错误地按右结合处理。
        if (info.associativity == Associativity::NonAssociative) {
            syntaxError(
                token.position,
                "Non-associative arithmetic operators "
                "are not supported by this parser"
            );
        }

        advance();

        const int nextMinPrecedence = info.associativity == Associativity::Left ? info.precedence + 1 : info.precedence;

        auto right = parseExpression(nextMinPrecedence);

        auto node = std::make_unique<ASTNode>();

        node->kind = SyntaxKind::BinaryOperator;
        node->text = token.text;
        node->position = token.position;
        node->op = info.id;

        node->left = std::move(left);
        node->right = std::move(right);

        left = std::move(node);
    }

    return left;
}