#pragma once

#include "Lexical.h"
#include <memory>

enum class SyntaxKind {
    Number,
    Variable,
    BinaryOperator,
    UnaryOperator,

    // 【新增：普通函数调用】
    FunctionCall,

    // 【新增：求和、连乘】
    Reduction
};

// 【新增：求和、连乘的种类】
enum class ReductionKind {
    Sum,
    Product
};

struct ASTNode {
    SyntaxKind kind = SyntaxKind::Number;

    // 数字节点：原始数字文本
    // 变量节点：变量名
    // 运算节点：运算符文本
    // 函数节点：函数名
    // 聚合节点："sum" 或 "prod"
    std::string text;

    // 对应源代码位置，便于报错
    std::size_t position = 0;

    // 仅 Number 使用
    double number = 0.0;

    // 仅 BinaryOperator / UnaryOperator 使用
    OperatorId op = OperatorId::None;

    // 二元运算：left、right 都使用
    // 一元运算：只使用 right
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;

    // 【新增：普通函数调用】
    // 参数表达式列表，例如 max(x, y) 有两个参数节点
    std::vector<std::unique_ptr<ASTNode>> arguments;

    // 【新增：求和、连乘】
    ReductionKind reduction = ReductionKind::Sum;

    // 下标变量名，例如 sum(i, 1, 3, i*x) 中的 i
    std::string indexName;

    // 下标声明的位置，便于后续语义分析报错
    std::size_t indexPosition = 0;

    // 上下界与主体都是表达式树
    std::unique_ptr<ASTNode> lower;
    std::unique_ptr<ASTNode> upper;
    std::unique_ptr<ASTNode> body;
};

// 【新增：比较约束】
// 普通算术表达式与约束分开保存
struct Constraint {
    std::unique_ptr<ASTNode> left;

    OperatorId relation = OperatorId::None;

    std::unique_ptr<ASTNode> right;

    // 比较符号及其位置
    std::string text;
    std::size_t position = 0;
};

class Syntax {
private:
    std::vector<Token> tokens;
    std::size_t current = 0;

    // 查看当前 Token，不前进
    const Token& peek() const;

    // 消耗当前 Token，遇到 End 时不再前进
    Token advance();

    // 匹配成功则消耗并返回 true，否则不前进
    bool match(TokenKind kind);

    // 要求指定类型，否则报告语法错误
    Token expect(TokenKind kind);

    // 数字、变量、括号，以及函数调用
    std::unique_ptr<ASTNode> parsePrimary();

    // 一元正负号
    std::unique_ptr<ASTNode> parseUnary();

    // 只处理二元算术运算
    std::unique_ptr<ASTNode> parseExpression(int minPrecedence);

    // 【新增：普通函数调用】
    // 调用前：函数名与左括号已经消耗
    // 返回时：右括号也已消耗
    std::unique_ptr<ASTNode> parseFunctionCall(const Token& name);

    // 【新增：求和、连乘】
    // 调用前：sum/prod 与左括号已经消耗
    // 返回时：右括号也已消耗
    std::unique_ptr<ASTNode> parseReduction(const Token& name);

public:
    explicit Syntax(std::vector<Token> tokens);

    // 解析完整的算术表达式
    std::unique_ptr<ASTNode> parse();

    // 【新增：比较约束入口】
    // 解析：算术表达式 比较运算符 算术表达式 End
    Constraint parseConstraint();
};