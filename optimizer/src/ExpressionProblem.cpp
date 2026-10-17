#include "ExpressionProblem.h"
#include <memory>
#include <stdexcept>
#include <utility>

namespace minax {
Problem compileProblem(std::vector<Variable> variables,
                       const std::string& objective,
                       const std::vector<std::string>& constraints,
                       Goal goal, FunctionRegistry registry, double tolerance) {
    std::vector<std::string> names;
    for(const auto& v:variables) names.push_back(v.name);
    Semantic semantic(names, registry.semanticFunctions());
    auto evaluator=std::make_shared<Evaluator>(std::move(registry), names.size());
    Syntax syntax(Lexical(objective).tokenize());
    std::shared_ptr<ASTNode> root=syntax.parse();
    auto binding=semantic.analyze(*root);
    Problem p;
    p.variables=std::move(variables);
    p.goal=goal;
    p.objective=[evaluator,root,binding](const Vector& x){
        return evaluator->evaluate(*root,x,binding);
    };
    for(const auto& text:constraints){
        Syntax parser(Lexical(text).tokenize());
        auto constraint=std::make_shared<::Constraint>(parser.parseConstraint());
        const auto relation=constraint->relation;
        if(relation!=OperatorId::Equal && relation!=OperatorId::LessEqual && relation!=OperatorId::GreaterEqual)
            throw std::invalid_argument("Optimizer accepts only =, <=, >= constraints: "+text);
        auto info=semantic.analyzeConstraint(*constraint);
        minax::Constraint c;
        c.name=text;
        c.tolerance=tolerance;
        c.relation=relation==OperatorId::Equal?Relation::Equal:Relation::LessEqual;
        c.value=[evaluator,constraint,info,relation](const Vector& x){
            auto v=evaluator->evaluateConstraint(*constraint,x,info);
            return relation==OperatorId::GreaterEqual?v.right-v.left:v.left-v.right;
        };
        p.constraints.push_back(std::move(c));
    }
    return p;
}
}
