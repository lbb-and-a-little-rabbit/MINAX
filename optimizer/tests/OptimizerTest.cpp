#include "Optimizer.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace minax;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static Problem base(){Problem p;p.variables={{"x",-5,5,0},{"y",-5,5,0}};return p;}
static void check(const char* name,const Problem& p,double expected,double accuracy,Options o){
    auto r=optimize(p,o);
    std::cout<<name<<" seed="<<o.seed<<" feasible="<<r.feasible<<" f="<<r.objective
             <<" error="<<std::abs(r.objective-expected)<<" violation="<<r.maxViolation
             <<" evaluations="<<r.evaluations<<" seconds="<<r.seconds<<'\n';
    require(r.feasible,"Expected feasible point");
    require(std::abs(r.objective-expected)<=accuracy,"Objective accuracy failed");
    require(!r.globallyCertified,"Must not claim global certificate");
    require(r.evaluations<=o.maxEvaluations,"Evaluation budget exceeded");
    for(const auto& c:r.constraints)require(c.satisfied,"Constraint verification failed");
}
int main(){try{
    Options o;o.maxSeconds=60;o.initialSamples=32;o.learningSteps=16;o.candidatePool=128;
    o.localStarts=10;o.maxEvaluations=14000;
    for(auto seed:{42u,2026u}){
        o.seed=seed;
        auto p=base();p.objective=[](const Vector& x){return (x[0]-2)*(x[0]-2)+(x[1]+1)*(x[1]+1);};
        check("quadratic",p,0,1e-8,o);
        p.constraints.push_back({[](const Vector& x){return x[0]+x[1]-2;},Relation::Equal,1e-7,1,"equality"});
        check("equality",p,.5,2e-6,o);
        p=base();p.objective=[](const Vector& x){return (x[0]-2)*(x[0]-2)+(x[1]-2)*(x[1]-2);};
        p.constraints.push_back({[](const Vector& x){return x[0]*x[0]+x[1]*x[1]-1;},Relation::LessEqual,1e-7,1,"circle"});
        check("circle",p,9-4*std::sqrt(2.0),2e-6,o);
        p=base();p.goal=Goal::Maximize;p.objective=[](const Vector& x){return 7-(x[0]-1)*(x[0]-1)-(x[1]+2)*(x[1]+2);};
        check("maximize",p,7,1e-8,o);
        p=base();p.variables={{"x",-1e6,1e6,0},{"y",-1e-3,1e-3,0}};
        p.objective=[](const Vector& x){return std::pow(x[0]/1e6-.2,2)+std::pow(x[1]/1e-3+.3,2);};
        check("different_scales",p,0,1e-8,o);
        p=base();p.variables={{"x",-1,4,.5}};
        p.objective=[](const Vector& x){if(x[0]<=0)throw std::domain_error("log domain");return std::pow(std::log(x[0])-.3,2);};
        check("partial_domain",p,0,1e-8,o);
    }
    auto p=base();p.objective=[](const Vector& x){return x[0]*x[0]+x[1]*x[1];};
    p.constraints={{[](const Vector& x){return x[0];},Relation::LessEqual,1e-7,1,"x<=0"},
                   {[](const Vector& x){return 1-x[0];},Relation::LessEqual,1e-7,1,"x>=1"}};
    auto r=optimize(p,o);require(!r.feasible,"Contradictory constraints accepted");std::cout<<"contradictory: correctly no feasible point found\n";
    p.constraints.clear();o.maxEvaluations=10;r=optimize(p,o);
    require(r.evaluations<=10 && r.termination=="evaluation_budget","Budget not respected");
    std::cout<<"budget: "<<r.evaluations<<" evaluations\n";
    o.maxEvaluations=70;r=optimize(p,o);
    require(r.evaluations<=70 && r.termination=="evaluation_budget","Local callback budget not respected");
    std::cout<<"local callback budget: "<<r.evaluations<<" evaluations\n";
    o.maxEvaluations=10;
    p.objective=[](const Vector&){return std::numeric_limits<double>::quiet_NaN();};
    r=optimize(p,o);require(!r.feasible && !std::isfinite(r.objective),"Invalid function accepted");
    std::cout<<"all invalid: correctly rejected\n";
    p.variables[0].upper=p.variables[0].lower;bool rejected=false;
    try{optimize(p,o);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Invalid bounds accepted");
    std::cout<<"invalid bounds: correctly rejected\nALL TESTS PASSED\n";return 0;
}catch(const std::exception& e){std::cerr<<"TEST FAILURE: "<<e.what()<<'\n';return 1;}}
