#include "Evaluator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <regex>
#include <stdexcept>
#include <utility>

namespace {
[[noreturn]] void fail(std::size_t position, const std::string& message) {
    throw std::runtime_error("Evaluation error at position " +
                             std::to_string(position) + " : " + message);
}

double finiteResult(double value, std::size_t position) {
    if (!std::isfinite(value)) {
        fail(position, "Non-finite result (overflow or invalid function domain)");
    }
    return value;
}

const ASTNode& required(const std::unique_ptr<ASTNode>& node,
                        std::size_t position, const char* description) {
    if (!node) fail(position, std::string("Missing AST child: ") + description);
    return *node;
}

std::int64_t integerBound(double value, std::size_t position) {
    // double 在此范围内可以逐个精确表示整数。
    constexpr double limit = 9007199254740991.0; // 2^53 - 1
    if (!std::isfinite(value) || std::trunc(value) != value ||
        value < -limit || value > limit) {
        fail(position, "Reduction bound must be an integer within +/- (2^53 - 1)");
    }
    return static_cast<std::int64_t>(value);
}

// 即使主体求值抛异常，也恢复局部位置的值及有效状态。
struct RestoreLocal {
    std::vector<double>& values;
    std::vector<bool>& active;
    std::size_t slot;
    double oldValue;
    bool oldActive;
    ~RestoreLocal() {
        values[slot] = oldValue;
        active[slot] = oldActive;
    }
};
} // namespace

void FunctionRegistry::add(
    std::string name, std::size_t minArgs, std::size_t maxArgs,
    std::function<double(const std::vector<double>&)> implementation) {
    static const std::regex pattern(R"([A-Za-z_][A-Za-z0-9_]*)");
    if (!std::regex_match(name, pattern) || name == "sum" || name == "prod")
        throw std::invalid_argument("Invalid or reserved function name: " + name);
    if (minArgs > maxArgs || !implementation)
        throw std::invalid_argument("Invalid function definition: " + name);
    for (const auto& entry : entries)
        if (entry.info.name == name)
            throw std::invalid_argument("Duplicate function: " + name);
    entries.push_back({{std::move(name), minArgs, maxArgs},
                       std::move(implementation)});
}

FunctionTable FunctionRegistry::semanticFunctions() const {
    FunctionTable result;
    result.reserve(entries.size());
    for (const auto& entry : entries) result.push_back(entry.info);
    return result;
}

const RuntimeFunction& FunctionRegistry::at(std::size_t id) const {
    if (id >= entries.size())
        throw std::out_of_range("Function binding is out of range");
    return entries[id];
}

FunctionRegistry FunctionRegistry::builtins() {
    FunctionRegistry result;
    result.add("sin", 1, 1, [](const auto& a) { return std::sin(a[0]); });
    result.add("cos", 1, 1, [](const auto& a) { return std::cos(a[0]); });
    result.add("tan", 1, 1, [](const auto& a) { return std::tan(a[0]); });
    result.add("exp", 1, 1, [](const auto& a) { return std::exp(a[0]); });
    result.add("log", 1, 1, [](const auto& a) {
        if (a[0] <= 0.0) throw std::domain_error("log requires x > 0");
        return std::log(a[0]);
    });
    result.add("sqrt", 1, 1, [](const auto& a) {
        if (a[0] < 0.0) throw std::domain_error("sqrt requires x >= 0");
        return std::sqrt(a[0]);
    });
    result.add("abs", 1, 1, [](const auto& a) { return std::fabs(a[0]); });
    result.add("pow", 2, 2, [](const auto& a) { return std::pow(a[0], a[1]); });
    // 当前 min/max 都是固定两个参数，与之前的 max 声明保持一致。
    result.add("min", 2, 2, [](const auto& a) { return std::min(a[0], a[1]); });
    result.add("max", 2, 2, [](const auto& a) { return std::max(a[0], a[1]); });
    return result;
}

Evaluator::Evaluator(FunctionRegistry functions, std::size_t count,
                     EvaluationOptions settings)
    : registry(std::move(functions)), variableCount(count), options(settings) {
    if (options.maxDepth == 0 || options.maxNodeVisits == 0 ||
        options.maxReductionIterations == 0)
        throw std::invalid_argument("Evaluation limits must be positive");
    if (!std::isfinite(options.absoluteTolerance) ||
        !std::isfinite(options.relativeTolerance) ||
        options.absoluteTolerance < 0.0 || options.relativeTolerance < 0.0)
        throw std::invalid_argument("Tolerances must be finite and non-negative");
}

Evaluator::Context Evaluator::makeContext(
    const std::vector<double>& values, const SemanticResult& result) const {
    if (values.size() != variableCount)
        throw std::invalid_argument("Variable value count does not match declaration");
    for (double value : values)
        if (!std::isfinite(value))
            throw std::invalid_argument("Variable values must be finite");
    return {values, std::vector<double>(result.localSlotCount, 0.0),
            std::vector<bool>(result.localSlotCount, false), 0, 0};
}

double Evaluator::evaluate(const ASTNode& root,
                          const std::vector<double>& values,
                          const SemanticResult& result) const {
    auto context = makeContext(values, result);
    return eval(root, context, 0);
}

double Evaluator::eval(const ASTNode& node, Context& context,
                       std::size_t depth) const {
    if (depth >= options.maxDepth) fail(node.position, "Maximum AST depth exceeded");
    if (context.visits >= options.maxNodeVisits)
        fail(node.position, "Node evaluation budget exceeded");
    ++context.visits;

    switch (node.kind) {
        case SyntaxKind::Number:
            return finiteResult(node.number, node.position);

        case SyntaxKind::Variable: {
            const auto slot = node.binding.slot;
            if (node.binding.source == VariableSource::Global) {
                if (slot >= context.values.size())
                    fail(node.position, "Global variable binding out of range");
                return context.values[slot];
            }
            if (node.binding.source == VariableSource::Local) {
                if (slot >= context.locals.size() || !context.localActive[slot])
                    fail(node.position, "Local variable is out of scope or unbound");
                return context.locals[slot];
            }
            fail(node.position, "Unbound variable; run semantic analysis first");
        }

        case SyntaxKind::UnaryOperator: {
            const double operand = eval(required(node.right, node.position, "operand"),
                                        context, depth + 1);
            if (node.op == OperatorId::Add) return operand;
            if (node.op == OperatorId::Subtract) return -operand;
            fail(node.position, "Invalid unary operator");
        }

        case SyntaxKind::BinaryOperator: {
            // 分开写，明确保证先求左侧再求右侧。
            const double a = eval(required(node.left, node.position, "left"),
                                  context, depth + 1);
            const double b = eval(required(node.right, node.position, "right"),
                                  context, depth + 1);
            double value;
            switch (node.op) {
                case OperatorId::Add:      value = a + b; break;
                case OperatorId::Subtract: value = a - b; break;
                case OperatorId::Multiply: value = a * b; break;
                case OperatorId::Divide:
                    // 不用容差判零：非零小数也可能是合法除数。
                    if (b == 0.0) fail(node.position, "Division by zero");
                    value = a / b;
                    break;
                default: fail(node.position, "Invalid binary operator");
            }
            return finiteResult(value, node.position);
        }

        case SyntaxKind::FunctionCall: {
            const RuntimeFunction* function;
            try {
                function = &registry.at(node.functionId);
            } catch (const std::exception& error) {
                fail(node.position, error.what());
            }
            if (function->info.name != node.text)
                fail(node.position, "Function registry differs from semantic registry");
            if (node.arguments.size() < function->info.minArgs ||
                node.arguments.size() > function->info.maxArgs)
                fail(node.position, "Invalid function argument count");

            std::vector<double> arguments;
            arguments.reserve(node.arguments.size());
            for (const auto& argument : node.arguments)
                arguments.push_back(eval(required(argument, node.position, "argument"),
                                         context, depth + 1));
            double value;
            try {
                value = function->implementation(arguments);
            } catch (const std::exception& error) {
                fail(node.position, "Function '" + node.text + "': " + error.what());
            } catch (...) {
                fail(node.position, "Function '" + node.text + "' threw an unknown exception");
            }
            return finiteResult(value, node.position);
        }

        case SyntaxKind::Reduction:
            return evalReduction(node, context, depth);
    }
    fail(node.position, "Unknown AST node");
}

double Evaluator::evalReduction(const ASTNode& node, Context& context,
                                std::size_t depth) const {
    if (node.reduction != ReductionKind::Sum &&
        node.reduction != ReductionKind::Product)
        fail(node.position, "Invalid reduction kind");

    // 先在外层环境求上下界，再激活本次下标。
    const auto first = integerBound(
        eval(required(node.lower, node.position, "lower"), context, depth + 1),
        node.position);
    const auto last = integerBound(
        eval(required(node.upper, node.position, "upper"), context, depth + 1),
        node.position);
    const auto& body = required(node.body, node.position, "body");
    const auto slot = node.indexSlot;
    if (slot >= context.locals.size())
        fail(node.position, "Reduction binding out of range");

    double result = node.reduction == ReductionKind::Sum ? 0.0 : 1.0;
    if (first > last) return result; // 空求和 0，空连乘 1。

    const auto count = static_cast<std::uint64_t>(last - first) + 1;
    if (count > options.maxReductionIterations - context.iterations)
        fail(node.position, "Reduction iteration budget exceeded");

    RestoreLocal restore{context.locals, context.localActive, slot,
                         context.locals[slot], context.localActive[slot]};
    context.localActive[slot] = true;

    for (std::int64_t index = first; ; ++index) {
        // 嵌套循环也消耗同一个总预算。
        if (context.iterations >= options.maxReductionIterations)
            fail(node.position, "Reduction iteration budget exceeded");
        ++context.iterations;
        context.locals[slot] = static_cast<double>(index);
        const double value = eval(body, context, depth + 1);
        result = finiteResult(
            node.reduction == ReductionKind::Sum ? result + value : result * value,
            node.position);
        if (index == last) break;
    }
    return result;
}

bool Evaluator::nearlyEqual(double a, double b) const {
    if (a == b) return true;
    // 缩放后比较，避免 a-b 或 tolerance*scale 对极大有限数溢出。
    const double scale = std::max(std::fabs(a), std::fabs(b));
    return std::fabs(a / scale - b / scale) <=
           options.absoluteTolerance / scale + options.relativeTolerance;
}

ConstraintValue Evaluator::evaluateConstraint(
    const Constraint& constraint, const std::vector<double>& values,
    const SemanticResult& result) const {
    auto context = makeContext(values, result);
    ConstraintValue value;
    value.left = eval(required(constraint.left, constraint.position, "constraint left"),
                      context, 0);
    value.right = eval(required(constraint.right, constraint.position, "constraint right"),
                       context, 0);
    const bool close = nearlyEqual(value.left, value.right);
    switch (constraint.relation) {
        case OperatorId::Equal:        value.satisfied = close; break;
        case OperatorId::NotEqual:     value.satisfied = !close; break;
        case OperatorId::Less:         value.satisfied = value.left < value.right; break;
        case OperatorId::Greater:      value.satisfied = value.left > value.right; break;
        case OperatorId::LessEqual:    value.satisfied = value.left <= value.right || close; break;
        case OperatorId::GreaterEqual: value.satisfied = value.left >= value.right || close; break;
        default: fail(constraint.position, "Invalid comparison operator");
    }
    return value;
}

