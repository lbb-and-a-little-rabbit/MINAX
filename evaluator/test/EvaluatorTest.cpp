#include "Evaluator.h"
#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::logic_error(message);
}
void closeTo(double actual, double expected) {
    require(std::isfinite(actual) &&
            std::fabs(actual - expected) <= 1e-10 * (1.0 + std::fabs(expected)),
            "Expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}
template<class F> void rejects(F action, const std::string& fragment) {
    try { action(); }
    catch (const std::exception& error) {
        require(std::string(error.what()).find(fragment) != std::string::npos,
                std::string("Unexpected exception: ") + error.what());
        return;
    }
    throw std::logic_error("Expected exception: " + fragment);
}
}

int main() {
    int total = 0, passed = 0;
    auto test = [&](const std::string& name, const std::function<void()>& action) {
        ++total;
        try {
            action();
            ++passed;
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            std::cout << "FAIL " << name << ": " << error.what() << '\n';
        }
    };

    const std::vector<std::string> names{"x", "y", "n"};
    auto registry = FunctionRegistry::builtins();
    registry.add("square", 1, 1, [](const auto& a) { return a[0] * a[0]; });
    Semantic semantic(names, registry.semanticFunctions());
    Evaluator evaluator(registry, names.size());

    auto eval = [&](const std::string& source, const std::vector<double>& values) {
        Lexical lexer(source);
        Syntax parser(lexer.tokenize());
        auto ast = parser.parse();
        auto result = semantic.analyze(*ast);
        return evaluator.evaluate(*ast, values, result);
    };
    auto constraint = [&](const std::string& source, const std::vector<double>& values) {
        Lexical lexer(source);
        Syntax parser(lexer.tokenize());
        auto ast = parser.parseConstraint();
        auto result = semantic.analyzeConstraint(ast);
        return evaluator.evaluateConstraint(ast, values, result).satisfied;
    };

    struct Case { const char* source; double expected; };
    // x=2, y=3, n=4
    for (const auto& c : std::vector<Case>{
        {"x + 2 * y", 8},
        {"- ( x + y ) / 2", -2.5},
        {"8 - 3 - 2", 3},
        {"8 / 4 / 2", 1},
        {"x - - y", 5},
        {"sin ( 0 ) + cos ( 0 )", 1},
        {"sqrt ( 9 ) + abs ( - 2 )", 5},
        {"pow ( x , y )", 8},
        {"max ( x , y ) + min ( x , y )", 5},
        {"log ( exp ( 1 ) )", 1},
        {"square ( y )", 9},
        {"sum ( i , 1 , 3 , i * x )", 12},
        {"prod ( i , 1 , 3 , x + i )", 60},
        {"sum ( i , - 2 , 2 , i )", 0},
        {"sum ( i , 3 , 1 , 1 / 0 )", 0},
        {"prod ( i , 3 , 1 , 1 / 0 )", 1},
        {"sum ( i , 1 , n , i )", 10},
        {"sum ( i , 1 , 3 , prod ( j , 1 , i , j ) )", 9},
        {"sum ( i , 1 , 3 , sum ( i , 1 , i , i ) + i )", 16},
        {"sum ( x , 1 , x , x ) + x", 5},
        {"sum ( i , 1 , 3 , i ) + prod ( j , 1 , 3 , j )", 12}
    }) {
        test(c.source, [&] { closeTo(eval(c.source, {2, 3, 4}), c.expected); });
    }

    for (const auto& c : std::vector<std::pair<std::string, std::string>>{
        {"1 / 0", "Division by zero"},
        {"sqrt ( - 1 )", "sqrt requires"},
        {"log ( 0 )", "log requires"},
        {"exp ( 1000 )", "Non-finite"},
        {"pow ( - 1 , 0.5 )", "Non-finite"},
        {"sum ( i , 1.5 , 3 , i )", "bound must be an integer"},
        {"sum ( i , 1 , 1e20 , i )", "bound must be an integer"},
        {"sum ( i , 1 , 1000001 , i )", "iteration budget"}
    }) {
        test(c.first, [&] { rejects([&] { eval(c.first, {2, 3, 4}); }, c.second); });
    }

    for (const auto& c : std::vector<std::pair<std::string, bool>>{
        {"x = y", false}, {"x != y", true},
        {"x < y", true}, {"x > y", false},
        {"x <= y", true}, {"x >= y", false},
        {"0.1 + 0.2 = 0.3", true},
        {"0.1 + 0.2 != 0.3", false},
        {"sum ( i , 1 , 3 , i ) = prod ( j , 1 , 3 , j )", true}
    }) {
        test(c.first, [&] {
            require(constraint(c.first, {2, 3, 4}) == c.second, "Wrong constraint result");
        });
    }

    test("repeated evaluations of one AST", [&] {
        Lexical lexer("sum ( i , 1 , 3 , i * x ) + y");
        Syntax parser(lexer.tokenize());
        auto ast = parser.parse();
        auto result = semantic.analyze(*ast);
        closeTo(evaluator.evaluate(*ast, {2, 3, 4}, result), 15);
        closeTo(evaluator.evaluate(*ast, {1, 7, 4}, result), 13);
    });
    test("recovery after domain error", [&] {
        Lexical lexer("sum ( i , 1 , 2 , sqrt ( x - i ) )");
        Syntax parser(lexer.tokenize());
        auto ast = parser.parse();
        auto result = semantic.analyze(*ast);
        rejects([&] { evaluator.evaluate(*ast, {0, 3, 4}, result); }, "sqrt requires");
        closeTo(evaluator.evaluate(*ast, {2, 3, 4}, result), 1);
    });
    test("wrong variable count", [&] {
        rejects([&] { eval("x", {2}); }, "value count");
    });
    test("nonfinite input", [&] {
        rejects([&] { eval("x", {std::numeric_limits<double>::infinity(), 3, 4}); },
                "must be finite");
    });
    test("missing local storage", [&] {
        Lexical lexer("sum ( i , 1 , 2 , i )");
        Syntax parser(lexer.tokenize());
        auto ast = parser.parse();
        semantic.analyze(*ast);
        rejects([&] { evaluator.evaluate(*ast, {2, 3, 4}, SemanticResult{}); },
                "binding out of range");
    });
    test("unbound AST", [&] {
        Lexical lexer("x");
        Syntax parser(lexer.tokenize());
        auto ast = parser.parse();
        rejects([&] { evaluator.evaluate(*ast, {2, 3, 4}, SemanticResult{}); },
                "Unbound variable");
    });
    test("nested total iteration budget", [&] {
        EvaluationOptions options;
        options.maxReductionIterations = 5;
        Evaluator limited(registry, names.size(), options);
        Lexical lexer("sum ( i , 1 , 3 , sum ( j , 1 , 3 , j ) )");
        Syntax parser(lexer.tokenize());
        auto ast = parser.parse();
        auto result = semantic.analyze(*ast);
        rejects([&] { limited.evaluate(*ast, {2, 3, 4}, result); }, "iteration budget");
    });
    test("node visit budget", [&] {
        EvaluationOptions options;
        options.maxNodeVisits = 2;
        Evaluator limited(registry, names.size(), options);
        Lexical lexer("x + y");
        Syntax parser(lexer.tokenize());
        auto ast = parser.parse();
        auto result = semantic.analyze(*ast);
        rejects([&] { limited.evaluate(*ast, {2, 3, 4}, result); }, "Node evaluation budget");
    });
    test("depth limit", [&] {
        EvaluationOptions options;
        options.maxDepth = 2;
        Evaluator limited(registry, names.size(), options);
        Lexical lexer("- - x");
        Syntax parser(lexer.tokenize());
        auto ast = parser.parse();
        auto result = semantic.analyze(*ast);
        rejects([&] { limited.evaluate(*ast, {2, 3, 4}, result); }, "depth");
    });
    test("registry mismatch", [&] {
        FunctionRegistry other;
        other.add("cos", 1, 1, [](const auto& a) { return std::cos(a[0]); });
        Evaluator wrong(other, names.size());
        Lexical lexer("sin ( x )");
        Syntax parser(lexer.tokenize());
        auto ast = parser.parse();
        auto result = semantic.analyze(*ast);
        rejects([&] { wrong.evaluate(*ast, {2, 3, 4}, result); }, "registry differs");
    });

    std::cout << "\nTotal: " << total << ", passed: " << passed
              << ", failed: " << total - passed << '\n';
    return total == passed ? 0 : 1;
}

