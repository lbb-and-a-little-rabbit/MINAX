// g++ -std=c++17 -Iinclude test/SemanticTest.cpp src/Lexical.cpp src/Syntax.cpp src/Semantic.cpp -o semantictest
//
// 测试语义绑定，不计算数学结果。
// N=数字，G=优化变量，L=局部下标，F=函数，R=求和/连乘。
// 所有表达式测试复用同一个 Semantic，验证每次分析的状态重置。

#include "Semantic.h"

#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::logic_error("Test assertion failed: " + message);
    }
}

// 输出绑定后的紧凑 AST，同时用于自动对比结果。
std::string boundAST(const ASTNode* node) {
    require(node != nullptr, "null AST node");

    switch (node->kind) {
        case SyntaxKind::Number:
            return "N(" + node->text + ")";

        case SyntaxKind::Variable: {
            require(node->binding.slot != INVALID_BINDING_ID,
                    "variable has invalid slot");
            const auto source = node->binding.source;
            require(source == VariableSource::Global ||
                    source == VariableSource::Local,
                    "variable is unbound");
            return std::string(source == VariableSource::Global ? "G(" : "L(")
                 + node->text + ":" + std::to_string(node->binding.slot) + ")";
        }

        case SyntaxKind::UnaryOperator:
            return "U(" + node->text + "," +
                   boundAST(node->right.get()) + ")";

        case SyntaxKind::BinaryOperator:
            return "B(" + node->text + "," +
                   boundAST(node->left.get()) + "," +
                   boundAST(node->right.get()) + ")";

        case SyntaxKind::FunctionCall: {
            require(node->functionId != INVALID_BINDING_ID,
                    "function is unbound");
            std::string result = "F(" + node->text + ":" +
                                 std::to_string(node->functionId);
            for (const auto& argument : node->arguments) {
                result += "," + boundAST(argument.get());
            }
            return result + ")";
        }

        case SyntaxKind::Reduction:
            require(node->indexSlot != INVALID_BINDING_ID,
                    "reduction index is unbound");
            return "R(" + node->text + "," + node->indexName + ":" +
                   std::to_string(node->indexSlot) + "," +
                   boundAST(node->lower.get()) + "," +
                   boundAST(node->upper.get()) + "," +
                   boundAST(node->body.get()) + ")";
    }
    throw std::logic_error("Unknown AST kind");
}

struct ExpressionTest {
    const char* name;
    const char* source;
    bool constraint;
    bool success;
    // 成功：预期 AST；失败：预期语义错误中的文字。
    const char* expected;
    std::size_t localSlots;
};

bool runExpression(Semantic& semantic, const ExpressionTest& test) {
    std::cout << "Input: " << test.source << '\n';

    // 前端错误不能被误算为预期语义错误。
    std::unique_ptr<ASTNode> root;
    Constraint constraint;

    try {
        Lexical lexical(test.source);
        Syntax parser(lexical.tokenize());
        if (test.constraint) {
            constraint = parser.parseConstraint();
        } else {
            root = parser.parse();
        }
    } catch (const std::exception& e) {
        std::cout << "FAIL: unexpected lexical/syntax error: "
                  << e.what() << '\n';
        return false;
    }

    SemanticResult result;
    try {
        if (test.constraint) {
            result = semantic.analyzeConstraint(constraint);
        } else {
            result = semantic.analyze(*root);
        }
    } catch (const std::exception& e) {
        const std::string message = e.what();
        const bool expectedError =
            !test.success &&
            message.rfind("Semantic error at position ", 0) == 0 &&
            message.find(test.expected) != std::string::npos;

        std::cout << (expectedError ? "PASS: expected error\n" :
                                      "FAIL: unexpected error\n")
                  << message << '\n';
        return expectedError;
    }

    if (!test.success) {
        std::cout << "FAIL: expected semantic error, but analysis succeeded\n";
        return false;
    }

    try {
        const std::string actual = test.constraint
            ? "C(" + constraint.text + "," +
              boundAST(constraint.left.get()) + "," +
              boundAST(constraint.right.get()) + ")"
            : boundAST(root.get());

        std::cout << "Bound AST: " << actual
                  << "\nLocal slots: " << result.localSlotCount << '\n';

        require(actual == test.expected,
                "binding mismatch; expected " + std::string(test.expected));
        require(result.localSlotCount == test.localSlots,
                "localSlotCount mismatch");
        std::cout << "PASS\n";
        return true;
    } catch (const std::exception& e) {
        std::cout << "FAIL: " << e.what() << '\n';
        return false;
    }
}

template<class ExpectedException>
bool expectException(const std::function<void()>& action,
                     const std::string& fragment) {
    try {
        action();
    } catch (const ExpectedException& e) {
        const std::string message = e.what();
        if (message.find(fragment) != std::string::npos) {
            std::cout << "PASS: expected error: " << message << '\n';
            return true;
        }
        std::cout << "FAIL: unexpected diagnostic: " << message << '\n';
        return false;
    } catch (const std::exception& e) {
        std::cout << "FAIL: unexpected exception type: " << e.what() << '\n';
        return false;
    }
    std::cout << "FAIL: expected exception was not thrown\n";
    return false;
}

} // namespace

int main() {
    // sin 同时也是优化变量，用于检查变量/函数的独立名字空间。
    const std::vector<std::string> variables = {"x", "y", "sin"};
    const FunctionTable functions = {
        {"sin", 1, 1},  // id=0
        {"max", 2, 2},  // id=1
        {"f",   0, 3}   // id=2，测试参数数量范围
    };

    std::size_t total = 0;
    std::size_t passed = 0;
    auto run = [&](const std::string& name,
                   const std::function<bool()>& action) {
        std::cout << "\n[" << ++total << "] " << name << '\n';
        try {
            if (action()) ++passed;
        } catch (const std::exception& e) {
            std::cout << "FAIL: unexpected exception: " << e.what() << '\n';
        }
    };

    try {
        Semantic semantic(variables, functions);
        const std::vector<ExpressionTest> tests = {
            {"global bindings", "x + y", false, true,
             "B(+,G(x:0),G(y:1))", 0},

            {"unary and binary", "- x * 2", false, true,
             "B(*,U(-,G(x:0)),N(2))", 0},

            {"nested function bindings", "max ( sin ( x ) , y )", false, true,
             "F(max:1,F(sin:0,G(x:0)),G(y:1))", 0},

            {"same variable and function name", "sin + sin ( x )", false, true,
             "B(+,G(sin:2),F(sin:0,G(x:0)))", 0},

            {"zero arguments allowed", "f ( )", false, true, "F(f:2)", 0},
            {"maximum argument count", "f ( x , y , 1 )", false, true,
             "F(f:2,G(x:0),G(y:1),N(1))", 0},

            {"sum binding", "sum ( i , 1 , 3 , i * x )", false, true,
             "R(sum,i:0,N(1),N(3),B(*,L(i:0),G(x:0)))", 1},

            {"nested scope and outer bound",
             "sum ( i , 1 , 3 , prod ( j , 1 , i , j + x ) )", false, true,
             "R(sum,i:0,N(1),N(3),R(prod,j:1,N(1),L(i:0),B(+,L(j:1),G(x:0))))", 2},

            {"same-name shadowing and restoration",
             "sum ( i , 1 , 3 , sum ( i , 1 , i , i ) + i )", false, true,
             "R(sum,i:0,N(1),N(3),B(+,R(sum,i:1,N(1),L(i:0),L(i:1)),L(i:0)))", 2},

            {"local shadows global only in body",
             "sum ( x , 1 , x , x ) + x", false, true,
             "B(+,R(sum,x:0,N(1),G(x:0),L(x:0)),G(x:0))", 1},

            {"sibling reductions get separate slots",
             "sum ( i , 1 , 2 , i ) + prod ( j , 1 , 2 , j )", false, true,
             "B(+,R(sum,i:0,N(1),N(2),L(i:0)),R(prod,j:1,N(1),N(2),L(j:1)))", 2},

            {"constraint sides share slot allocation",
             "sum ( i , 1 , 3 , i * x ) <= prod ( j , 1 , 2 , j + y )",
             true, true,
             "C(<=,R(sum,i:0,N(1),N(3),B(*,L(i:0),G(x:0))),R(prod,j:1,N(1),N(2),B(+,L(j:1),G(y:1))))", 2},

            {"undefined global", "x + z", false, false, "Undefined variable", 0},
            {"undefined function", "unknown ( x )", false, false, "Undefined function", 0},
            {"too few arguments", "sin ( )", false, false, "expects", 0},
            {"too many arguments", "sin ( x , y )", false, false, "expects", 0},
            {"argument traversal", "sin ( z )", false, false, "Undefined variable", 0},
            {"index not visible in own bound",
             "sum ( i , 1 , i , i * x )", false, false, "Undefined variable", 0},
            {"index not visible after reduction",
             "sum ( i , 1 , 3 , i ) + i", false, false, "Undefined variable", 0},

            // 同一个分析器在嵌套作用域内失败后，再分析合法表达式。
            {"nested failure", "sum ( i , 1 , 3 , prod ( j , 1 , 2 , z ) )",
             false, false, "Undefined variable", 0},
            {"no leaked local after failure", "i", false, false, "Undefined variable", 0},
            {"recovery and slot reset", "prod ( k , 1 , 2 , k + y )",
             false, true, "R(prod,k:0,N(1),N(2),B(+,L(k:0),G(y:1)))", 1},

            {"constraint right side checked", "x <= z", true, false, "Undefined variable", 0},
            {"constraint index does not leak to other side",
             "sum ( i , 1 , 3 , i ) = i", true, false, "Undefined variable", 0},
            {"constraint recovery", "x + y = 10", true, true,
             "C(=,B(+,G(x:0),G(y:1)),N(10))", 0},

            // 这些属于运行时求值检查，本阶段不应误报。
            {"division by zero not evaluated", "1 / 0", false, true,
             "B(/,N(1),N(0))", 0},
            {"bound integrality deferred", "sum ( i , 1.5 , 3 , i )",
             false, true, "R(sum,i:0,N(1.5),N(3),L(i:0))", 1}
        };

        for (const auto& test : tests) {
            run(test.name, [&] { return runExpression(semantic, test); });
        }

        // ================= 注册信息错误 =================
        run("duplicate variable", [&] {
            return expectException<std::invalid_argument>([&] {
                Semantic invalid({"x", "x"}, functions);
            }, "Duplicate variable");
        });
        run("invalid variable name", [&] {
            return expectException<std::invalid_argument>([&] {
                Semantic invalid({"1x"}, functions);
            }, "Invalid variable");
        });
        run("duplicate function", [&] {
            return expectException<std::invalid_argument>([&] {
                Semantic invalid(variables, {{"f", 1, 1}, {"f", 2, 2}});
            }, "Duplicate function");
        });
        run("invalid function name", [&] {
            return expectException<std::invalid_argument>([&] {
                Semantic invalid(variables, {{"bad@", 1, 1}});
            }, "Invalid function");
        });
        run("invalid argument range", [&] {
            return expectException<std::invalid_argument>([&] {
                Semantic invalid(variables, {{"f", 3, 1}});
            }, "Invalid argument count range");
        });
        for (const std::string reserved : {"sum", "prod"}) {
            run("reserved function: " + reserved, [&] {
                return expectException<std::invalid_argument>([&] {
                    Semantic invalid(variables, {{reserved, 1, 1}});
                }, "Cannot register");
            });
        }

        // ================= 手工破坏 AST，测试防御检查 =================
        run("null function argument", [&] {
            return expectException<std::logic_error>([&] {
                ASTNode node;
                node.kind = SyntaxKind::FunctionCall;
                node.text = "sin";
                node.arguments.emplace_back(); // 一个参数，但指针为空
                semantic.analyze(node);
            }, "null");
        });

        run("non-finite number", [&] {
            return expectException<std::runtime_error>([&] {
                ASTNode node;
                node.kind = SyntaxKind::Number;
                node.number = std::numeric_limits<double>::infinity();
                node.text = "inf";
                semantic.analyze(node);
            }, "Invalid number");
        });

        run("missing constraint sides", [&] {
            return expectException<std::logic_error>([&] {
                Constraint constraint;
                constraint.relation = OperatorId::Equal;
                semantic.analyzeConstraint(constraint);
            }, "null");
        });

        // 同一棵树更换优化变量顺序后必须重新绑定，而不是跳过旧绑定。
        run("reanalyze overwrites old bindings", [&] {
            Lexical lexical("x + y");
            Syntax parser(lexical.tokenize());
            auto root = parser.parse();
            semantic.analyze(*root);

            Semantic reversed({"y", "x"}, functions);
            const auto result = reversed.analyze(*root);
            const auto actual = boundAST(root.get());
            require(actual == "B(+,G(x:1),G(y:0))", "old binding was reused");
            require(result.localSlotCount == 0, "unexpected local slots");
            std::cout << "Bound AST: " << actual << "\nPASS\n";
            return true;
        });
    } catch (const std::exception& e) {
        std::cout << "FATAL: test setup failed: " << e.what() << '\n';
        return 1;
    }

    std::cout << "\n========== SUMMARY ==========\n"
              << "Total:  " << total << '\n'
              << "Passed: " << passed << '\n'
              << "Failed: " << total - passed << '\n';

    return passed == total ? 0 : 1;
}

