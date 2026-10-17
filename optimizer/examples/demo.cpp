#include "Optimizer.h"
#include <iomanip>
#include <iostream>
int main(){
    minax::Problem p;
    p.variables={{"x",-5,5,0},{"y",-5,5,0}};
    p.objective=[](const minax::Vector& x){return (x[0]-2)*(x[0]-2)+(x[1]+1)*(x[1]+1);};
    p.constraints.push_back({[](const minax::Vector& x){return x[0]+x[1]-2;},
        minax::Relation::Equal,1e-7,1,"x + y = 2"});
    try{
        auto r=minax::optimize(p);
        std::cout<<std::setprecision(12)<<"feasible="<<r.feasible
                 <<" globally_certified="<<r.globallyCertified<<'\n';
        for(std::size_t i=0;i<r.x.size();++i)std::cout<<p.variables[i].name<<"="<<r.x[i]<<'\n';
        std::cout<<"objective="<<r.objective<<" max_violation="<<r.maxViolation
                 <<" evaluations="<<r.evaluations<<" stop="<<r.termination<<'\n';
        return r.feasible?0:2;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
