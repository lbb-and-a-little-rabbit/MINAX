#include "Syntax.h"

void printAST(const ASTNode* node, int depth = 0, const std::string& label = "") {
    const std::string indent(depth * 4, ' ');

    std::cout << indent << label;

    if (node == nullptr) {
        std::cout << "<null>\n";
        return;
    }

    switch (node->kind) {
        case SyntaxKind::Number:
            std::cout << "Number: " << node->text << '\n';
            break;

        case SyntaxKind::Variable:
            std::cout << "Variable: " << node->text << '\n';
            break;

        case SyntaxKind::UnaryOperator:
            std::cout << "UnaryOperator: " << node->text << '\n';
            printAST(node->right.get(), depth + 1, "operand: ");
            break;

        case SyntaxKind::BinaryOperator:
            std::cout << "BinaryOperator: " << node->text << '\n';
            printAST(node->left.get(), depth + 1, "left: ");
            printAST(node->right.get(), depth + 1, "right: ");
            break;

        default:
            std::cout << "Unsupported node kind\n";
            break;
    }
}

int main() {
    std::vector<std::string> testSets;

    testSets.push_back("1 + 2 * 3");
    testSets.push_back("1 + 2 * 3 / 4");
    testSets.push_back("1 + 2 * 3 / 4 - 5");
    testSets.push_back("1 + 2 * 3 / 4 - 5 * 6");
    testSets.push_back("x + y - z * w / t");
    testSets.push_back("- 1 + 2");
    testSets.push_back("- 1 + 2 * 3");

    // 错误测试
    testSets.push_back("1 +");          // 缺少右操作数
    testSets.push_back("* 2");          // 二元乘号不能作为表达式开头
    testSets.push_back("( 1 + 2");      // 缺少右括号
    testSets.push_back("1 + 2 )");      // 多余的右括号
    testSets.push_back("x y");          // 两个变量之间缺少运算符

    for (std::size_t i = 0; i < testSets.size(); ++i) {
        std::cout << "\nTest " << i + 1
                << ": \"" << testSets[i] << "\"\n";

        try {
            Lexical lexical(testSets[i]);
            const auto tokens = lexical.tokenize();
            Syntax syntax(tokens);
            auto AST = syntax.parse();
            printAST(AST.get());
        } 
        catch (const std::exception& e) {
            std::cout << e.what() << '\n';
        }
    }
}