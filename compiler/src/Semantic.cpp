#include "Semantic.h"

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <regex>

// ==================== 语义错误 ====================

[[noreturn]]
void semanticError(std::size_t position, const std::string& message) {
    std::stringstream ss;
    ss << "Semantic error at position " << position << " : " << message;
    throw std::runtime_error(ss.str());
}

// ==================== 构造与注册表 ====================

Semantic::Semantic(std::vector<std::string> variableNames, FunctionTable functionTable) {
    // TODO：
    // 1. 将 functionTable 移动到成员 functions。
    // 2. 调用 buildVariableTable(variableNames)。
    // 3. 调用 buildFunctionTable()。
    //
    // variables / functions 在分析不同 AST 时保持不变。
    // scopes / nextLocalSlot 属于单次分析状态。
    //
    // 完成后删除下面的占位异常。
    functions = std::move(functionTable);
    buildVariableTable(variableNames);
    buildFunctionTable();
}

void Semantic::buildVariableTable(const std::vector<std::string>& variableNames) {
    // TODO：
    // 1. 清空 variables 和 variableLookup。
    // 2. 按传入列表顺序，为每个变量分配 slot：
    //    第一个为 0，第二个为 1，以此类推。
    // 3. 检查名字是否合法：
    //    [A-Za-z_][A-Za-z0-9_]*
    // 4. 检查是否重复声明。
    // 5. 填写 variables 和 variableLookup。
    //
    // 示例：
    // {"x", "y"}
    // variables[0] = {"x", 0}
    // variables[1] = {"y", 1}
    // variableLookup["x"] = 0
    // variableLookup["y"] = 1
    //
    // 外部声明没有表达式中的源位置，
    // 名字无效或重复时可抛出 std::invalid_argument。
    static const std::regex identifierPattern(
        R"([A-Za-z_][A-Za-z0-9_]*)"
    );

    variables.clear();
    variableLookup.clear();
    for (std::size_t i = 0; i < variableNames.size(); i++) {
        std::string name = variableNames[i];
        if (!std::regex_match(name, identifierPattern)) {
            throw std::invalid_argument("Invalid variable name : \"" + name + "\"");
        }
        if (variableLookup.find(name) != variableLookup.end()) {
            throw std::invalid_argument("Duplicate variable name : \"" + name + "\"");
        }
        VariableInfo info;
        info.name = name;
        info.slot = i;
        variables.push_back(info);
        variableLookup[name] = i;
    }
}

void Semantic::buildFunctionTable() {
    // TODO：
    // 1. 清空 functionLookup。
    // 2. 遍历成员 functions，以列表下标作为 functionId。
    // 3. 检查函数名字是否合法。
    // 4. 检查函数名字不能重复。
    // 5. 检查 minArgs <= maxArgs。
    // 6. 拒绝注册名为 sum 或 prod 的普通函数：
    //    当前解析器已经把这两种调用作为 Reduction 处理。
    // 7. 将函数名和编号写入 functionLookup。
    //
    // 允许函数与优化变量同名，因为它们使用不同的名字表。
    // 不需要把函数实际执行代码放进当前语义分析器。
    //
    // 注册信息不合法时可抛出 std::invalid_argument。
    static const std::regex identifierPattern(
        R"([A-Za-z_][A-Za-z0-9_]*)"
    );

    functionLookup.clear();
    for (std::size_t i = 0; i < functions.size(); i++) {
        std::string name = functions[i].name;
        if (name == "sum" || name == "prod") {
            throw std::invalid_argument("Cannot register built-in function \"" + name + "\"");
        }
        if (functionLookup.find(name) != functionLookup.end()) {
            throw std::invalid_argument("Duplicate function name : \"" + name + "\"");
        }
        if (!std::regex_match(name, identifierPattern)) {
            throw std::invalid_argument("Invalid function name : \"" + name + "\"");
        }
        if (functions[i].minArgs > functions[i].maxArgs) {
            throw std::invalid_argument("Invalid argument count range for function \"" + name + "\"");
        }

        functionLookup[name] = i;
    }
}

// ==================== 状态与作用域 ====================

void Semantic::resetAnalysisState() {
    // TODO：
    // 1. 清空 scopes。
    // 2. 将 nextLocalSlot 重置为 0。
    // 3. 不要清空优化变量表和函数表。
    //
    // 只在公开分析入口开始时调用，
    // 递归分析子表达式时不能调用。
    scopes.clear();
    nextLocalSlot = 0;
}

void Semantic::enterScope() {
    // TODO：
    // 向 scopes 末尾添加一个空 LocalScope。
    // 添加后，scopes.back() 就是最内层作用域。
    scopes.emplace_back();
}

void Semantic::leaveScope() {
    // TODO：
    // 1. 检查 scopes 不能为空。
    // 2. 移除最后一层作用域。
    // 3. 不减少 nextLocalSlot。
    //
    // 不回收编号是为了让本次分析中的不同下标声明
    // 获得不同 slot，简化后续求值。
    //
    // 空栈退出属于内部使用错误，可抛出 std::logic_error。
    if (scopes.empty()) {
        throw std::logic_error("Empty scope stack");
    }
    scopes.pop_back();
}

std::size_t Semantic::declareLocal(const std::string& name, std::size_t position) {
    // TODO：
    // 1. 确认已经存在当前作用域。
    // 2. 只检查当前层是否已经声明同名变量。
    // 3. 如果当前层重复，调用 semanticError。
    // 4. 分配 nextLocalSlot，再递增 nextLocalSlot。
    // 5. 将 name -> slot 放入 scopes.back().symbols。
    // 6. 返回新分配的 slot。
    //
    // 允许遮蔽外层局部变量和优化变量。
    // 不要因为外层存在同名变量就判定重复。
    if (scopes.empty()) {
        throw std::logic_error("Cannot declare a local variable without an active scope");
    }
    auto& symbols = scopes.back().symbols;
    if (symbols.find(name) != symbols.end()) {
        semanticError(position, "Duplicate variable name : \"" + name + "\"");
    }
    symbols[name] = nextLocalSlot;
    return nextLocalSlot++;
}

VariableBinding Semantic::resolveVariable(const std::string& name, std::size_t position) const {
    // TODO：
    // 1. 从 scopes 的末尾向开头查找 name。
    // 2. 在局部作用域中找到时，返回：
    //    source = VariableSource::Local
    //    slot   = 找到的局部编号
    // 3. 局部都找不到，再查询 variableLookup。
    // 4. 找到优化变量时，返回：
    //    source = VariableSource::Global
    //    slot   = 对应优化变量编号
    // 5. 都找不到，调用 semanticError，报告未定义变量。
    //
    // 查找时不要使用 map[name]：
    // operator[] 可能创建一个原本不存在的名字。
    for (std::size_t i = scopes.size(); i > 0; i--) {
        auto& symbols = scopes[i - 1].symbols;
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            VariableBinding binding;
            binding.source = VariableSource::Local;
            binding.slot = it->second;
            return binding;
        }     
    }
    if (variableLookup.find(name) != variableLookup.end()) {
        VariableBinding binding;
        binding.source = VariableSource::Global;
        binding.slot = variableLookup.at(name);
        return binding;
    }
    semanticError(position, "Undefined variable : \"" + name + "\"");
}

std::size_t Semantic::resolveFunction(const std::string& name, std::size_t position) const {
    // TODO：
    // 1. 在 functionLookup 中查找 name。
    // 2. 找到则返回对应 functionId。
    // 3. 找不到则调用 semanticError，报告未定义函数。
    //
    // 函数调用不能通过优化变量表来解析。
    auto it = functionLookup.find(name);
    if (it != functionLookup.end()) {
        return it->second;
    }
    semanticError(position, "Undefined function : \"" + name + "\"");
}

// ==================== 对外分析入口 ====================

SemanticResult Semantic::analyze(ASTNode& root) {
    // TODO：
    // 1. 调用 resetAnalysisState()。
    // 2. 调用 analyzeExpression(root)。
    // 3. 成功后确认 scopes 已恢复为空。
    // 4. 返回 SemanticResult，localSlotCount = nextLocalSlot。
    //
    // 异常处理：
    // 分析过程中失败时清理局部状态，然后重新抛出原异常。
    //
    // 本实现会直接修改 AST。失败时可能已经写入部分绑定，
    // 因此调用者不能把失败的 AST 交给求值器执行。
    resetAnalysisState();
    try {
        analyzeExpression(root);

        if (!scopes.empty()) {
            throw std::logic_error("Scope stack is not empty");
        }

        SemanticResult result;
        result.localSlotCount = nextLocalSlot;
        return result;
    } 
    catch (...) {
        resetAnalysisState();
        throw;
    }
}

SemanticResult Semantic::analyzeConstraint(Constraint& constraint) {
    // TODO：
    // 1. 调用 resetAnalysisState()。
    // 2. 检查 constraint.left 和 constraint.right 非空。
    // 3. 检查 relation 属于：
    //    Equal、NotEqual、Greater、Less、
    //    GreaterEqual、LessEqual。
    // 4. 调用 analyzeExpression 检查左侧。
    // 5. 调用 analyzeExpression 检查右侧。
    // 6. 返回总的 localSlotCount。
    //
    // 注意：
    // 左右两侧之间不能 resetAnalysisState()，
    // 因为两侧使用同一次分析的局部编号空间。
    //
    // 不要对左右节点调用公开的 analyze()；
    // 应调用不会重置状态的 analyzeExpression()。
    //
    // 出错时清理局部状态，再重新抛出。
    // 此阶段不判断约束是否成立，也不设置比较容差。
    resetAnalysisState();
    if (!constraint.left || !constraint.right) {
        throw std::logic_error("Invalid constraint : constraint expression is null");
    }
    auto opid = constraint.relation; 
    if (opid != OperatorId::Equal &&
        opid != OperatorId::NotEqual &&
        opid != OperatorId::Greater &&
        opid != OperatorId::Less &&
        opid != OperatorId::GreaterEqual &&
        opid != OperatorId::LessEqual) {
        throw std::logic_error("Invalid constraint : invalid relation operator");
    }

    try {
        analyzeExpression(*constraint.left);
        analyzeExpression(*constraint.right);
        if (!scopes.empty()) {
            throw std::logic_error("Scope stack is not empty");
        }
        SemanticResult result;
        result.localSlotCount = nextLocalSlot;
        return result;
    }
    catch (...) {
        resetAnalysisState();
        throw;
    }
}

// ==================== AST 分发 ====================

void Semantic::analyzeExpression(ASTNode& node) {
    // TODO：
    // 根据 node.kind 使用 switch 分发：
    //
    // Number:
    //     确认 number 为有限值，可使用 std::isfinite。
    //     不进行变量查找。
    //
    // Variable:
    //     调用 analyzeVariable(node)。
    //
    // UnaryOperator:
    //     调用 analyzeUnary(node)。
    //
    // BinaryOperator:
    //     调用 analyzeBinary(node)。
    //
    // FunctionCall:
    //     调用 analyzeFunctionCall(node)。
    //
    // Reduction:
    //     调用 analyzeReduction(node)。
    //
    // 未知节点类型应报告错误，不能静默跳过。
    switch(node.kind) {
        case SyntaxKind::Number:
            if (!std::isfinite(node.number)) {
                semanticError(node.position, "Invalid number : \"" + node.text + "\"");
            }
            break;
        case SyntaxKind::Variable:
            analyzeVariable(node);
            break;
        case SyntaxKind::UnaryOperator:
            analyzeUnary(node);
            break;
        case SyntaxKind::BinaryOperator:
            analyzeBinary(node);
            break;
        case SyntaxKind::FunctionCall:
            analyzeFunctionCall(node);
            break;
        case SyntaxKind::Reduction:
            analyzeReduction(node);
            break;
        default:
            throw std::logic_error("Unknown node kind");
        }
}

// ==================== 变量 ====================

void Semantic::analyzeVariable(ASTNode& node) {
    // TODO：
    // 1. 使用 node.text 作为变量名。
    // 2. 调用 resolveVariable(node.text, node.position)。
    // 3. 将返回结果写入 node.binding。
    //
    // 重新分析 AST 时，应覆盖原有绑定。
    // 不能因为 node.binding 已有值就跳过查找。
    std::string name = node.text;
    node.binding = resolveVariable(name, node.position);
}

// ==================== 一元运算 ====================

void Semantic::analyzeUnary(ASTNode& node) {
    // TODO：
    // 1. 检查 node.op 只能是 Add 或 Subtract。
    // 2. 检查 node.right 非空。
    // 3. 当前设计的一元节点不应有 left 子节点。
    // 4. 递归调用 analyzeExpression(*node.right)。
    //
    // 不需要计算正负号的结果。
    auto opid = node.op;
    if (opid != OperatorId::Add && opid != OperatorId::Subtract) {
        throw std::logic_error("Invalid unary operator");
    }
    if (!node.right) {
        throw std::logic_error("Invalid unary operator : right node is null");
    }
    if (node.left) {
        throw std::logic_error("Invalid unary operator : left node is not null");
    }
    analyzeExpression(*node.right);
}

// ==================== 二元运算 ====================

void Semantic::analyzeBinary(ASTNode& node) {
    // TODO：
    // 1. 检查 node.op 属于当前支持的算术运算：
    //    Add、Subtract、Multiply、Divide。
    // 2. 检查 left 和 right 都非空。
    // 3. 递归检查 left。
    // 4. 递归检查 right。
    //
    // 不接受比较运算，比较关系由 Constraint 保存。
    //
    // 对于 1 / x，不在这里猜测 x 是否为零。
    // 运行时数值合法性由求值器负责。
    auto opid = node.op;
    if (opid != OperatorId::Add &&
        opid != OperatorId::Subtract &&
        opid != OperatorId::Multiply &&
        opid != OperatorId::Divide) {
        throw std::logic_error("Invalid binary operator");
    }
    if (!node.left || !node.right) {
        throw std::logic_error("Invalid binary operator : left or right node is null");
    }
    analyzeExpression(*node.left);
    analyzeExpression(*node.right);
}

// ==================== 普通函数调用 ====================

void Semantic::analyzeFunctionCall(ASTNode& node) {
    // TODO：
    // 1. 调用 resolveFunction，取得 functionId。
    // 2. 从 functions[functionId] 取得函数描述。
    // 3. 检查 arguments.size() 位于：
    //    [minArgs, maxArgs]。
    // 4. 检查每个参数节点非空。
    // 5. 递归调用 analyzeExpression 检查各参数。
    // 6. 成功后把 functionId 写入 node.functionId。
    //
    // 参数数量错误时，错误信息中建议包含：
    // 函数名、期望参数数量、实际参数数量。
    //
    // 不实际调用函数。
    // sqrt(x) 的 x 是否为负数，通常留到求值时检查。
    const std::size_t functionId = resolveFunction(node.text, node.position);

    const FunctionInfo& info = functions[functionId];

    const std::size_t argumentCount = node.arguments.size();

    if (argumentCount < info.minArgs || argumentCount > info.maxArgs) {
        std::stringstream ss;

        ss << "Function \"" << node.text << "\" expects ";

        if (info.minArgs == info.maxArgs) {
            ss << info.minArgs;
        } 
        else {
            ss << info.minArgs << " to " << info.maxArgs;
        }

        ss << " arguments, but " << argumentCount << " provided";

        semanticError(node.position, ss.str());
    }

    for (std::size_t i = 0; i < argumentCount; ++i) {
        const auto& arg = node.arguments[i];

        if (!arg) {
            throw std::logic_error(
                "Invalid function call: argument " +
                std::to_string(i + 1) +
                " of function \"" + node.text + "\" is null"
            );
        }

        analyzeExpression(*arg);
    }

    node.functionId = functionId;
}

// ==================== 求和、连乘 ====================

void Semantic::analyzeReduction(ASTNode& node) {
    // TODO：
    // 1. 检查 reduction 为 Sum 或 Product。
    // 2. 检查 lower、upper、body 都非空。
    // 3. 在现有作用域中分析 lower。
    // 4. 在现有作用域中分析 upper。
    //
    // 5. 调用 enterScope()。
    // 6. 调用 declareLocal(node.indexName, node.indexPosition)。
    // 7. 将得到的局部编号写入 node.indexSlot。
    // 8. 分析 body。
    // 9. 调用 leaveScope()。
    //
    // 顺序非常重要：
    // 新下标只在 body 中可见，不在本次 lower/upper 中可见。
    //
    // 例如：
    // sum(i, 1, 3, sum(j, 1, i, j*x))
    // 内层上界中的 i 可以绑定到外层下标。
    //
    // 异常安全：
    // 进入作用域后，如果声明下标或分析主体抛出异常，
    // 也必须退出当前作用域，再重新抛出。
    // 可使用 try/catch 或局部作用域守卫实现。
    //
    // 本阶段不执行循环、不把上下界强制转换成整数。
    // 上下界的运行时值是否合法，由后续求值器检查。
    if (node.reduction != ReductionKind::Sum && node.reduction != ReductionKind::Product) {
        throw std::logic_error("Invalid reduction");
    }
    if (!node.lower || !node.upper || !node.body) {
        throw std::logic_error("Invalid reduction : lower, upper or body node is null");
    }
    analyzeExpression(*node.lower);
    analyzeExpression(*node.upper);

    enterScope();
    try {
        node.indexSlot = declareLocal(node.indexName, node.indexPosition);
        analyzeExpression(*node.body);
    }
    catch (...) {
        leaveScope();
        throw;
    }
    leaveScope();
}