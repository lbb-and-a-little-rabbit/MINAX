#include "Evaluator.h"
#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main() {
    try {
        const std::vector<std::string> names{"x", "y"};
        auto registry = FunctionRegistry::builtins();
        // 自定义函数必须同时进入语义分析和求值的同一份注册表。
        registry.add("square", 1, 1, [](const auto& a) { return a[0] * a[0]; });
        Semantic semantic(names, registry.semanticFunctions());
        Evaluator evaluator(registry, names.size());

        const std::string source =
            "sum ( i , 1 , 3 , i * x ) + square ( y )";
        Lexical lexical(source);
        Syntax parser(lexical.tokenize());
        auto ast = parser.parse();
        auto analysis = semantic.analyze(*ast);

        // AST 和绑定只生成一次，反复给不同变量值求值。
        for (const std::vector<double>& values :
             std::vector<std::vector<double>>{{2, 3}, {1, 4}}) {
            std::cout << source << '\n'
                      << "x=" << values[0] << ", y=" << values[1]
                      << ", result=" << evaluator.evaluate(*ast, values, analysis) << "\n\n";
        }

        Lexical constraintLexer("x + y <= 6");
        Syntax constraintParser(constraintLexer.tokenize());
        auto constraint = constraintParser.parseConstraint();
        auto constraintAnalysis = semantic.analyzeConstraint(constraint);
        auto value = evaluator.evaluateConstraint(constraint, {2, 3}, constraintAnalysis);
        std::cout << "Constraint: " << value.left << " <= " << value.right
                  << " -> " << std::boolalpha << value.satisfied << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

