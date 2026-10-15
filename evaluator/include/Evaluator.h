#pragma once

#include "Semantic.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

// 注册顺序决定 functionId。语义分析和求值必须使用同一个注册表快照。
struct RuntimeFunction {
    FunctionInfo info;
    std::function<double(const std::vector<double>&)> implementation;
};

class FunctionRegistry {
public:
    void add(std::string name, std::size_t minArgs, std::size_t maxArgs,
             std::function<double(const std::vector<double>&)> implementation);
    FunctionTable semanticFunctions() const;
    const RuntimeFunction& at(std::size_t id) const;
    static FunctionRegistry builtins();

private:
    std::vector<RuntimeFunction> entries;
};

struct EvaluationOptions {
    // 作用于整次求值，包括嵌套聚合以及约束的左右两侧。
    std::size_t maxNodeVisits = 5000000;
    std::size_t maxReductionIterations = 1000000;
    std::size_t maxDepth = 512;
    double absoluteTolerance = 1e-9;
    double relativeTolerance = 1e-9;
};

struct ConstraintValue {
    double left = 0.0;
    double right = 0.0;
    bool satisfied = false;
};

class Evaluator {
public:
    Evaluator(FunctionRegistry registry, std::size_t variableCount,
              EvaluationOptions options = {});

    // root 必须已经由 Semantic 成功分析。
    // values 顺序必须与 Semantic 的 variableNames 顺序相同。
    // semanticResult 必须来自这棵 root，而不是其他表达式。
    double evaluate(const ASTNode& root, const std::vector<double>& values,
                    const SemanticResult& semanticResult) const;

    ConstraintValue evaluateConstraint(
        const Constraint& constraint,
        const std::vector<double>& values,
        const SemanticResult& semanticResult) const;

private:
    struct Context {
        const std::vector<double>& values;
        std::vector<double> locals;
        std::vector<bool> localActive;
        std::size_t visits = 0;
        std::size_t iterations = 0;
    };

    FunctionRegistry registry;
    std::size_t variableCount;
    EvaluationOptions options;

    Context makeContext(const std::vector<double>& values,
                        const SemanticResult& result) const;
    double eval(const ASTNode& node, Context& context, std::size_t depth) const;
    double evalReduction(const ASTNode& node, Context& context,
                         std::size_t depth) const;
    bool nearlyEqual(double a, double b) const;
};

