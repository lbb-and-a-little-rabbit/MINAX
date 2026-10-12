// g++ -std=c++17 -Iinclude test/SyntaxComplicatedTest.cpp src/Syntax.cpp src/Lexical.cpp -o syntaxcomplicatedtest
//
// 本文件测试语法结构，不执行数学运算、不检查函数注册或变量作用域。
// 合法用例对比完整 AST；错误用例必须在语法阶段抛出预期诊断。
// 因此 f()、未知函数、非整数求和边界在本阶段可以解析成功。

#include "Syntax.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

enum class Entry {
    Expression,
    Constraint
};

struct TestCase {
    const char* name;
    Entry entry;
    const char* source;
    bool shouldPass;

    // 成功用例：预期 AST 的紧凑表示。
    // 错误用例：预期语法错误消息中包含的片段。
    const char* expected;
};

std::string opText(OperatorId op) {
    switch (op) {
        case OperatorId::Add:          return "+";
        case OperatorId::Subtract:     return "-";
        case OperatorId::Multiply:     return "*";
        case OperatorId::Divide:       return "/";
        case OperatorId::Equal:        return "=";
        case OperatorId::NotEqual:     return "!=";
        case OperatorId::Greater:      return ">";
        case OperatorId::Less:         return "<";
        case OperatorId::GreaterEqual: return ">=";
        case OperatorId::LessEqual:    return "<=";
        default:
            throw std::logic_error("AST contains an invalid operator");
    }
}

// 将 AST 转为可比较、可打印的表示。
// 使用运算符枚举而不只读取 text，以检查 op 字段是否正确填写。
std::string astText(const ASTNode* node) {
    if (!node) {
        throw std::logic_error("AST contains a missing node");
    }

    switch (node->kind) {
        case SyntaxKind::Number:
            return "N(" + node->text + ")";

        case SyntaxKind::Variable:
            return "V(" + node->text + ")";

        case SyntaxKind::UnaryOperator:
            return "U(" + opText(node->op) + "," +
                   astText(node->right.get()) + ")";

        case SyntaxKind::BinaryOperator:
            return "B(" + opText(node->op) + "," +
                   astText(node->left.get()) + "," +
                   astText(node->right.get()) + ")";

        case SyntaxKind::FunctionCall: {
            std::string result = "F(" + node->text;

            for (const auto& argument : node->arguments) {
                result += "," + astText(argument.get());
            }

            return result + ")";
        }

        case SyntaxKind::Reduction: {
            std::string name;

            switch (node->reduction) {
                case ReductionKind::Sum:     name = "sum";  break;
                case ReductionKind::Product: name = "prod"; break;
                default:
                    throw std::logic_error("Invalid reduction kind");
            }

            if (node->text != name) {
                throw std::logic_error("Reduction text/kind mismatch");
            }

            return "R(" + name + "," + node->indexName + "," +
                   astText(node->lower.get()) + "," +
                   astText(node->upper.get()) + "," +
                   astText(node->body.get()) + ")";
        }
    }

    throw std::logic_error("Unknown AST kind");
}

std::string constraintText(const Constraint& constraint) {
    const std::string relation = opText(constraint.relation);

    if (relation != "=" && relation != "!=" &&
        relation != ">" && relation != "<" &&
        relation != ">=" && relation != "<=") {
        throw std::logic_error("Constraint has a non-comparison operator");
    }

    if (constraint.text != relation) {
        throw std::logic_error("Constraint text/relation mismatch");
    }

    return "C(" + relation + "," +
           astText(constraint.left.get()) + "," +
           astText(constraint.right.get()) + ")";
}

bool runTest(const TestCase& test, std::size_t index) {
    std::cout << "\n[" << index << "] " << test.name
              << "\nInput: " << test.source << '\n';

    // 所有用例都应通过词法分析。
    // 词法阶段异常不能被误认为“预期语法错误”。
    std::vector<Token> tokens;
    try {
        Lexical lexical(test.source);
        tokens = lexical.tokenize();
    } catch (const std::exception& error) {
        std::cout << "FAIL: unexpected lexical error: "
                  << error.what() << '\n';
        return false;
    }

    try {
        Syntax syntax(std::move(tokens));
        std::string actual;

        if (test.entry == Entry::Expression) {
            auto root = syntax.parse();
            actual = astText(root.get());
        } else {
            auto constraint = syntax.parseConstraint();
            actual = constraintText(constraint);
        }

        std::cout << "AST: " << actual << '\n';

        if (!test.shouldPass) {
            std::cout << "FAIL: expected syntax error, but parsing succeeded\n";
            return false;
        }

        if (actual != test.expected) {
            std::cout << "FAIL: AST mismatch\nExpected: "
                      << test.expected << '\n';
            return false;
        }

        std::cout << "PASS\n";
        return true;
    } catch (const std::exception& error) {
        const std::string message = error.what();

        // 不把 out_of_range、节点缺失等异常误记为通过。
        const bool isSyntaxError =
            message.rfind("Syntax error at position ", 0) == 0;

        if (!test.shouldPass && isSyntaxError &&
            message.find(test.expected) != std::string::npos) {
            std::cout << "PASS: expected rejection\n"
                      << message << '\n';
            return true;
        }

        std::cout << "FAIL: unexpected exception\n"
                  << message << '\n';
        return false;
    }
}

} // namespace

int main() {
    const std::vector<TestCase> tests = {
        // ================= 普通函数调用 =================
        {"one argument", Entry::Expression, "sin ( x )", true,
         "F(sin,V(x))"},

        {"multiple arguments and precedence", Entry::Expression,
         "max ( x + 1 , y * 2 )", true,
         "F(max,B(+,V(x),N(1)),B(*,V(y),N(2)))"},

        {"nested calls", Entry::Expression,
         "sin ( max ( x , y ) )", true,
         "F(sin,F(max,V(x),V(y)))"},

        {"three arguments", Entry::Expression, "f ( x , y , z )", true,
         "F(f,V(x),V(y),V(z))"},

        {"zero arguments are syntactically allowed", Entry::Expression,
         "f ( )", true, "F(f)"},

        {"unary operator on a call", Entry::Expression,
         "- sin ( x ) * 2", true, "B(*,U(-,F(sin,V(x))),N(2))"},

        {"function name without parentheses is a variable", Entry::Expression,
         "sum + prod", true, "B(+,V(sum),V(prod))"},

        // ================= 求和与连乘 =================
        {"sum", Entry::Expression, "sum ( i , 1 , 3 , i * x )", true,
         "R(sum,i,N(1),N(3),B(*,V(i),V(x)))"},

        {"product", Entry::Expression, "prod ( j , 1 , 4 , x + j )", true,
         "R(prod,j,N(1),N(4),B(+,V(x),V(j)))"},

        {"expression bounds", Entry::Expression,
         "sum ( i , - 2 , n + 1 , i )", true,
         "R(sum,i,U(-,N(2)),B(+,V(n),N(1)),V(i))"},

        {"nested reductions", Entry::Expression,
         "sum ( i , 1 , 3 , prod ( j , 1 , 2 , i + j ) )", true,
         "R(sum,i,N(1),N(3),R(prod,j,N(1),N(2),B(+,V(i),V(j))))"},

        {"call inside reduction", Entry::Expression,
         "sum ( i , 1 , 3 , sin ( i * x ) )", true,
         "R(sum,i,N(1),N(3),F(sin,B(*,V(i),V(x))))"},

        {"reduction inside call", Entry::Expression,
         "max ( sum ( i , 1 , 3 , i ) , x )", true,
         "F(max,R(sum,i,N(1),N(3),V(i)),V(x))"},

        {"parenthesized reduction followed by multiplication", Entry::Expression,
         "( sum ( i , 1 , 3 , i ) + 2 ) * x", true,
         "B(*,B(+,R(sum,i,N(1),N(3),V(i)),N(2)),V(x))"},

        // 下界非整数或空范围属于后续语义/求值策略，不是语法错误。
        {"noninteger bound is syntactically allowed", Entry::Expression,
         "sum ( i , 1.5 , 3 , i )", true,
         "R(sum,i,N(1.5),N(3),V(i))"},

        {"empty range is syntactically allowed", Entry::Expression,
         "prod ( i , 3 , 1 , i )", true,
         "R(prod,i,N(3),N(1),V(i))"},

        // ================= 六种比较约束 =================
        {"equal", Entry::Constraint, "x = y", true, "C(=,V(x),V(y))"},
        {"not equal", Entry::Constraint, "x != y", true, "C(!=,V(x),V(y))"},
        {"greater", Entry::Constraint, "x > y", true, "C(>,V(x),V(y))"},
        {"less", Entry::Constraint, "x < y", true, "C(<,V(x),V(y))"},
        {"greater equal", Entry::Constraint, "x >= y", true, "C(>=,V(x),V(y))"},
        {"less equal", Entry::Constraint, "x <= y", true, "C(<=,V(x),V(y))"},

        {"complex constraint", Entry::Constraint,
         "x + prod ( j , 1 , 3 , y + j ) >= 10", true,
         "C(>=,B(+,V(x),R(prod,j,N(1),N(3),B(+,V(y),V(j)))),N(10))"},

        {"parenthesized sides", Entry::Constraint,
         "( x + y ) <= ( sin ( z ) + 3 )", true,
         "C(<=,B(+,V(x),V(y)),B(+,F(sin,V(z)),N(3)))"},

        // ================= 错误：函数调用 =================
        {"missing first argument", Entry::Expression,
         "max ( , x )", false, "Expected primary expression"},

        {"trailing comma", Entry::Expression,
         "max ( x , )", false, "Expected primary expression"},

        {"missing comma", Entry::Expression,
         "max ( x y )", false, "Expected RParen"},

        {"missing call closing parenthesis", Entry::Expression,
         "sin ( x", false, "Expected RParen"},

        {"double comma", Entry::Expression,
         "max ( x , , y )", false, "Expected primary expression"},

        {"trailing token after call", Entry::Expression,
         "sin ( x ) y", false, "Expected End"},

        // ================= 错误：求和与连乘 =================
        {"index must be identifier", Entry::Expression,
         "sum ( 1 , 1 , 3 , x )", false, "Expected Identifier"},

        {"index cannot be expression", Entry::Expression,
         "sum ( i + 1 , 1 , 3 , i )", false, "Expected Comma"},

        {"missing lower bound", Entry::Expression,
         "sum ( i , , 3 , i )", false, "Expected primary expression"},

        {"missing upper bound", Entry::Expression,
         "prod ( i , 1 , , i )", false, "Expected primary expression"},

        {"missing body", Entry::Expression,
         "sum ( i , 1 , 3 , )", false, "Expected primary expression"},

        {"missing body separator", Entry::Expression,
         "sum ( i , 1 , 3 )", false, "Expected Comma"},

        {"extra reduction argument", Entry::Expression,
         "prod ( i , 1 , 3 , i , x )", false, "Expected RParen"},

        {"missing reduction closing parenthesis", Entry::Expression,
         "sum ( i , 1 , 3 , i", false, "Expected RParen"},

        // ================= 错误：比较约束及入口 =================
        {"constraint missing relation", Entry::Constraint,
         "x + y", false, "Expected comparison operator"},

        {"constraint missing left side", Entry::Constraint,
         ">= 3", false, "Expected primary expression"},

        {"constraint missing right side", Entry::Constraint,
         "x >=", false, "Expected primary expression"},

        {"chained comparison", Entry::Constraint,
         "0 < x < 10", false, "Chained comparisons"},

        {"whole constraint parentheses unsupported", Entry::Constraint,
         "( x >= 0 )", false, "Expected RParen"},

        {"comparison in arithmetic entry", Entry::Expression,
         "x >= 0", false, "Expected End"},

        {"comparison in function argument unsupported", Entry::Expression,
         "f ( x > 0 )", false, "Expected RParen"},

        {"extra constraint token", Entry::Constraint,
         "x >= 0 y", false, "Expected End"}
    };

    std::size_t passed = 0;

    for (std::size_t i = 0; i < tests.size(); ++i) {
        if (runTest(tests[i], i + 1)) {
            ++passed;
        }
    }

    std::cout << "\n========== SUMMARY ==========\n"
              << "Total:  " << tests.size() << '\n'
              << "Passed: " << passed << '\n'
              << "Failed: " << tests.size() - passed << '\n';

    // 所有预期成功和预期失败用例均符合预期，才返回 0。
    return passed == tests.size() ? 0 : 1;
}

