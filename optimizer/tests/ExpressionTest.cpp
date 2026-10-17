#include "ExpressionProblem.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    auto p=minax::compileProblem({{"x",0,4,1}},
        "sum ( i , 1 , 3 , ( x - i ) * ( x - i ) )",{"x >= 0.5"});
    require(std::abs(p.objective({2})-2)<1e-12,"sum evaluation failed");
    require(std::abs(p.constraints[0].value({2})+1.5)<1e-12,">= residual direction failed");
    minax::Options o;o.initialSamples=16;o.learningSteps=4;o.localStarts=4;
    auto r=minax::optimize(p,o);
    require(r.feasible && std::abs(r.objective-2)<1e-8,"sum optimization failed");
    auto product=minax::compileProblem({{"x",0,1,0}},"prod ( i , 1 , 3 , i + x )",{});
    require(std::abs(product.objective({.5})-13.125)<1e-12,"prod evaluation failed");
    auto f=minax::compileProblem({{"x",0,1,.5}},"sin ( x )",{});
    require(std::abs(f.objective({.5})-std::sin(.5))<1e-12,"function evaluation failed");
    for(const auto& text:{"x < 1","x > 0","x != 0"}){
        bool rejected=false;
        try{minax::compileProblem({{"x",0,1,.5}},"x",{text});}
        catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"Unsupported relation accepted");
    }
    std::cout<<"Expression tests passed: sum optimization, prod, function, >=, strict comparisons\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
