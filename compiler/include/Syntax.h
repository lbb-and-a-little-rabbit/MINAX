#pragma once

#include "Lexical.h"

enum class SyntaxKind { 
    Number,
    Variable,
    BinaryOperator,
    UnaryOperator
};

struct ASTNode {
    SyntaxKind kind;
    // 数字节点：原始数字文本
    // 变量节点：变量名
    // 运算节点：运算符文本
    std::string text;

    // 对应源代码位置，便于报错
    std::size_t position = 0;

    // 仅 Number 使用
    double number = 0.0;

    // 仅 BinaryOperator / UnaryOperator 使用
    OperatorId op = OperatorId::None;

    // 二元运算：left、right 都使用
    // 一元运算：约定只使用 right
    // 数字、变量：两者都是 nullptr
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
};

class Syntax { 
private:
    std::vector<Token> tokens;
    std::size_t current = 0;

    // 查看当前 Token，不前进
    const Token& peek() const;

    // 消耗当前 Token，并返回被消耗的 Token
    Token advance();

    // 类型符合则消耗并返回 true，否则不前进
    bool match(TokenKind kind);

    // 要求指定类型，否则报告语法错误
    Token expect(TokenKind kind);

    // 基本表达式：数字、变量、括号
    std::unique_ptr<ASTNode> parsePrimary();

    // 一元正负号
    std::unique_ptr<ASTNode> parseUnary();

    // 二元算术表达式，按优先级构造树
    std::unique_ptr<ASTNode> parseExpression(int minPrecedence);

public:
    explicit Syntax(std::vector<Token> tokens);

    std::unique_ptr<ASTNode> parse();
};