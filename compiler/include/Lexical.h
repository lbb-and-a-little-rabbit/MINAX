#pragma once

#include <cmath>
#include <cstring>
#include <cstddef>
#include <iostream>
#include <locale>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

enum class TokenKind {
    Number,
    Identifier,
    Operator,
    LParen,
    RParen,
    Comma,
    End
};

enum class OperatorId {
    None,

    Add,
    Subtract,
    Multiply,
    Divide,

    Equal,
    NotEqual,
    Greater,
    Less,
    GreaterEqual,
    LessEqual
};

enum class OperatorCategory {
    Arithmetic,
    Comparison
};

enum class Associativity {
    Left,
    Right,
    NonAssociative
};

// 这里记录的是二元运算符的信息。
// 一元正负号由语法分析器根据上下文另行处理。
struct OperatorInfo {
    OperatorId id;
    OperatorCategory category;
    int precedence;
    Associativity associativity;
};

const std::unordered_map<std::string, OperatorInfo>& operatorTable();

std::string tokenKindToString(TokenKind kind);

struct Token {
    TokenKind kind;
    std::string text;
    std::size_t position;

    // 仅 Number 使用
    double number = 0.0;

    // 仅 Operator 使用
    OperatorId op = OperatorId::None;
};

class Lexical {
private:
    std::string words;

public:
    explicit Lexical(std::string words);
    std::vector<Token> tokenize() const;

};
