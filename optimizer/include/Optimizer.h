#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace minax {
using Vector = std::vector<double>;
using Function = std::function<double(const Vector&)>;
enum class Goal { Minimize, Maximize };
enum class Relation { LessEqual, Equal };

struct Variable {
    std::string name;
    double lower, upper, initial;
};
struct Constraint {
    Function value;                  // g(x)<=0 或 h(x)=0
    Relation relation=Relation::LessEqual;
    double tolerance=1e-7;           // 原函数单位中的绝对容差
    double scale=1.0;                // 典型残差量级，用于模型和局部约束缩放
    std::string name;
};
struct Problem {
    std::vector<Variable> variables;
    Function objective;
    std::vector<Constraint> constraints;
    Goal goal=Goal::Minimize;
};
struct Options {
    std::uint64_t seed=42;
    std::size_t initialSamples=48;
    std::size_t learningSteps=40;     // 设为0可关闭代理模型选点，作为比较基线
    std::size_t candidatePool=384;
    std::size_t localStarts=12;
    int localMaxEvaluations=1000;
    std::size_t maxEvaluations=16000; // 一次评估=目标+所有约束；包含最终复核
    double maxSeconds=20.0;
    double xTolerance=1e-9;          // 归一化变量空间中的停止容差，不是精度保证
    double initialStep=0.1;
};
struct ConstraintReport {
    std::string name;
    double residual=std::numeric_limits<double>::quiet_NaN();
    double violation=std::numeric_limits<double>::infinity();
    double tolerance=0;
    bool satisfied=false;
};
struct Result {
    bool feasible=false;
    bool globallyCertified=false;    // 这套方法不产生全局最优证明
    Vector x;
    double objective=std::numeric_limits<double>::quiet_NaN();
    double maxViolation=std::numeric_limits<double>::infinity();
    std::vector<ConstraintReport> constraints;
    std::size_t evaluations=0;
    std::size_t functionCalls=0;
    std::size_t invalidPoints=0;
    std::size_t learningEvaluations=0;
    double seconds=0;
    std::string termination;         // 搜索停止原因，不代表数学最优性
    std::vector<int> localStopCodes;
};

// 有限边界、连续变量、确定性函数。严格比较/整数变量不在本版范围内。
// 定义域错误请抛 std::domain_error/std::runtime_error；非有限返回也视为无效。
Result optimize(const Problem& problem, const Options& options={});
}
