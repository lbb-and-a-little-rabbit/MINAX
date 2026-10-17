#pragma once
#include "Optimizer.h"
#include "Evaluator.h"

namespace minax {
// 表达式只编译一次，返回的回调持有 AST 和求值器，不引用局部临时对象。
// 支持 =、<=、>=；不悄悄把严格比较改成非严格比较。
Problem compileProblem(std::vector<Variable> variables,
                       const std::string& objective,
                       const std::vector<std::string>& constraints,
                       Goal goal=Goal::Minimize,
                       FunctionRegistry registry=FunctionRegistry::builtins(),
                       double constraintTolerance=1e-7);
}
