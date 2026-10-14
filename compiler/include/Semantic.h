#pragma once

#include "Syntax.h"

// ==================== 外部函数描述 ====================

// 本阶段只检查函数名字和参数数量。
// 实际计算实现由后续求值器提供。
struct FunctionInfo {
    std::string name;

    std::size_t minArgs = 0;
    std::size_t maxArgs = 0;
};

// 函数编号就是在这个列表中的下标。
// 例如：functions[0] 是 sin，则 sin 的 functionId 为 0。
using FunctionTable = std::vector<FunctionInfo>;

// ==================== 优化变量描述 ====================

struct VariableInfo {
    std::string name;

    // 对应求值时 values 数组中的下标
    std::size_t slot = INVALID_BINDING_ID;
};

// ==================== 局部作用域 ====================

// 当前每层作用域只需保存下标名字和局部编号。
// 后续扩展其他局部变量时仍可复用。
struct LocalScope {
    std::unordered_map<std::string, std::size_t> symbols;
};

// ==================== 单次分析结果 ====================

struct SemanticResult {
    // 后续求值器需要准备多少个局部存储位置。
    //
    // 普通表达式没有 sum/prod 时为 0。
    // 每个 Reduction 声明分配一个独立编号。
    //
    // 对约束而言，左右两侧共同使用这一个编号空间。
    std::size_t localSlotCount = 0;
};

// ==================== 语义错误 ====================

[[noreturn]]
void semanticError(std::size_t position, const std::string& message);

// ==================== 语义分析器 ====================

class Semantic {
private:
    // ---------- 外部优化变量 ----------
    std::vector<VariableInfo> variables;

    // 名字 → variables 中的下标，同时也是变量 slot
    std::unordered_map<std::string, std::size_t> variableLookup;

    // ---------- 外部函数 ----------
    FunctionTable functions;

    // 名字 → functions 中的下标，也就是 functionId
    std::unordered_map<std::string, std::size_t> functionLookup;

    // ---------- 单次遍历状态 ----------

    // 从外层到内层排列，back() 是当前最内层作用域
    std::vector<LocalScope> scopes;

    // 下一个可以分配的局部编号
    std::size_t nextLocalSlot = 0;

    // ================= 初始化 =================

    // 根据外部名字列表建立优化变量表。
    void buildVariableTable(const std::vector<std::string>& variableNames);

    // 根据 functions 建立函数名字索引。
    void buildFunctionTable();

    // ================= 分析状态 =================

    // 开始分析一棵表达式或一条约束前调用。
    // 只清除局部作用域和局部编号，不清除外部注册表。
    void resetAnalysisState();

    // ================= 作用域管理 =================

    // 进入一层新的空作用域。
    void enterScope();

    // 离开当前作用域。
    // 不回收已经分配的局部编号。
    void leaveScope();

    // 在当前层声明一个下标，并分配唯一的局部编号。
    std::size_t declareLocal(const std::string& name, std::size_t position);

    // 先从内到外查局部作用域，再查优化变量表。
    VariableBinding resolveVariable(const std::string& name, std::size_t position) const;

    // 查函数表，找到后返回函数编号。
    std::size_t resolveFunction(const std::string& name, std::size_t position) const;

    // ================= AST 遍历 =================

    // 根据 node.kind 分发到各类节点检查。
    // 递归调用时使用本函数，不能调用公开 analyze()，
    // 否则会错误重置作用域。
    void analyzeExpression(ASTNode& node);

    // 变量：检查声明并填写 binding。
    void analyzeVariable(ASTNode& node);

    // 一元运算：检查运算符与操作数。
    void analyzeUnary(ASTNode& node);

    // 二元运算：检查运算符与左右操作数。
    void analyzeBinary(ASTNode& node);

    // 普通函数：检查名字、参数个数、参数表达式。
    void analyzeFunctionCall(ASTNode& node);

    // 求和/连乘：检查上下界，声明下标，检查主体。
    void analyzeReduction(ASTNode& node);

public:
    // variableNames 的顺序决定优化变量编号。
    // functionTable 的顺序决定函数编号。
    //
    // 建立后求值器必须沿用相同的变量、函数编号规则。
    Semantic(std::vector<std::string> variableNames, FunctionTable functionTable);

    // 分析一棵完整的算术表达式树，写入绑定信息。
    // 每次调用重新从局部编号 0 开始。
    SemanticResult analyze(ASTNode& root);

    // 分析一条约束，左右两侧共享本次局部编号空间。
    SemanticResult analyzeConstraint(Constraint& constraint);
};