#include "ExpressionProblem.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>

// 输入见 examples/problem.txt。stdout 只输出一条 JSON，适合 PHP proc_open。
static std::string quote(const std::string& s){
    std::ostringstream out;out<<'"';
    for(unsigned char c:s){
        if(c=='"'||c=='\\')out<<'\\'<<c;
        else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<int>(c)<<std::dec;
        else out<<c;
    }
    out<<'"';return out.str();
}
static std::string line(){std::string s;if(!std::getline(std::cin,s))throw std::invalid_argument("Unexpected end of input");if(!s.empty()&&s.back()=='\r')s.pop_back();return s;}
template<class... T> static void fields(const std::string& s,T&... values){
    std::istringstream input(s);input.imbue(std::locale::classic());
    if(!(input>>...>>values))throw std::invalid_argument("Invalid input line");
    input>>std::ws;if(!input.eof())throw std::invalid_argument("Extra input fields");
}
static void number(double x){if(std::isfinite(x))std::cout<<x;else std::cout<<"null";}
int main(){
    std::cout.imbue(std::locale::classic());std::cout<<std::setprecision(17);
    try{
        auto goal=line();if(goal!="min" && goal!="max")throw std::invalid_argument("First line must be min or max");
        int n;fields(line(),n);if(n<1||n>32)throw std::invalid_argument("Expected 1..32 variables");
        std::vector<minax::Variable> variables;
        for(int i=0;i<n;++i){minax::Variable v;fields(line(),v.name,v.lower,v.upper,v.initial);variables.push_back(v);}
        auto expression=line();int m;fields(line(),m);if(m<0||m>64)throw std::invalid_argument("Expected 0..64 constraints");
        std::vector<std::string> constraints;for(int i=0;i<m;++i)constraints.push_back(line());
        auto p=minax::compileProblem(variables,expression,constraints,goal=="min"?minax::Goal::Minimize:minax::Goal::Maximize);
        auto r=minax::optimize(p);
        std::cout<<"{\"status\":"<<quote(r.feasible?"feasible_found":"no_feasible_found")
                 <<",\"globally_certified\":false,\"termination\":"<<quote(r.termination)<<",\"objective\":";
        number(r.objective);std::cout<<",\"variables\":{";
        for(std::size_t i=0;i<r.x.size();++i){if(i)std::cout<<',';std::cout<<quote(variables[i].name)<<':';number(r.x[i]);}
        std::cout<<"},\"max_violation\":";number(r.maxViolation);
        std::cout<<",\"evaluations\":"<<r.evaluations<<",\"invalid_points\":"<<r.invalidPoints
                 <<",\"seconds\":"<<r.seconds<<",\"constraints\":[";
        for(std::size_t i=0;i<r.constraints.size();++i){if(i)std::cout<<',';const auto& c=r.constraints[i];
            std::cout<<"{\"name\":"<<quote(c.name)<<",\"residual\":";number(c.residual);
            std::cout<<",\"violation\":";number(c.violation);
            std::cout<<",\"tolerance\":"<<c.tolerance<<",\"satisfied\":"<<(c.satisfied?"true":"false")<<'}';}
        std::cout<<"]}\n";return r.feasible?0:2;
    }catch(const std::exception& e){std::cout<<"{\"error\":"<<quote(e.what())<<"}\n";return 1;}
}
