#include "Syntax.h"

[[noreturn]]
void syntaxError(std::size_t position, std::string message) {
    std::stringstream ss;
    ss << "Syntax error at position " << position << " : " << message << std::endl;
    throw std::runtime_error(ss.str());
}

Syntax::Syntax(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

const Token& Syntax::peek() const {
    return tokens[current]; 
}

Token Syntax::advance() {
    auto token = tokens[current];
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
    syntaxError(token.position, "Expected " + tokenKindToString(kind) + " but got " + tokenKindToString(token.kind));
}

std::unique_ptr<ASTNode> Syntax::parse() { 
    auto node = parseExpression(0);
    expect(TokenKind::End);
    return node;
}

std::unique_ptr<ASTNode> Syntax::parsePrimary() { 
    auto token = peek();
    switch(token.kind) {
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
            auto node = std::make_unique<ASTNode>();
            node->kind = SyntaxKind::Variable;
            node->text = token.text;
            node->position = token.position;
            advance();
            return node;
        }
        case TokenKind::LParen: { 
            advance();
            auto node = parseExpression(0);
            expect(TokenKind::RParen);
            return node;
        }
        default: {
            syntaxError(token.position, "Expected primary expression but got " + tokenKindToString(token.kind));
        }
    }
}

std::unique_ptr<ASTNode> Syntax::parseUnary() { 
    auto token = peek();
    if (token.kind == TokenKind::Operator && (token.op == OperatorId::Add || token.op == OperatorId::Subtract)) {
        advance();
        auto node = std::make_unique<ASTNode>();
        node->kind = SyntaxKind::UnaryOperator;
        node->text = token.text;
        node->position = token.position;
        node->op = token.op;
        node->right = parseUnary();
        return node;
    }
    else {
        return parsePrimary();
    }
}

std::unique_ptr<ASTNode> Syntax::parseExpression(int minPrecedence) { 
    auto left = parseUnary();
    
    while (true) {
        auto token = peek();
        if (token.kind != TokenKind::Operator) { 
            break;
        }

        const auto& info = operatorTable().at(token.text);

        if (info.category != OperatorCategory::Arithmetic) {
            break;
        }

        if (info.precedence < minPrecedence) {
            break;
        }

        advance();

        int nextMinPrecedence = info.associativity == Associativity::Left ? info.precedence + 1 : info.precedence;

        auto right = parseExpression(nextMinPrecedence);

        auto node = std::make_unique<ASTNode>();
        node->kind = SyntaxKind::BinaryOperator;
        node->text = token.text;
        node->position = token.position;
        node->op = token.op;
        node->left = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }

    return left;
}