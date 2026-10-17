#include "Optimizer.h"
#include <nlopt.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <random>
#include <stdexcept>

namespace minax {
namespace {
using Clock=std::chrono::steady_clock;
constexpr double BAD=1e20;
struct Stop { std::string reason; };
struct Sample {
    Vector z, x, residuals;
    double f=std::numeric_limits<double>::quiet_NaN();
    double excess=0, maxViolation=0;
    bool valid=true, feasible=false;
};

// 小规模高斯过程：RBF核，标准化输出，Cholesky求解，不显式求逆。
// 固定核尺度，不宣称校准过的统计置信度；方差仅用于启发式探索。
class GaussianProcess {
    std::vector<Vector> points;
    Vector lower, alpha;
    double mean=0, deviation=1, length=0.3;
    static double kernel(const Vector& a,const Vector& b,double length){
        double d=0;for(std::size_t j=0;j<a.size();++j)d+=(a[j]-b[j])*(a[j]-b[j]);
        return std::exp(-d/(2*length*length));
    }
public:
    bool fit(const std::vector<Vector>& x,const Vector& y){
        points=x;const std::size_t n=x.size();if(n<3)return false;
        mean=std::accumulate(y.begin(),y.end(),0.0)/n;
        double v=0;for(double a:y)v+=(a-mean)*(a-mean);
        deviation=std::max(1e-8,std::sqrt(v/n));
        length=0.3*std::sqrt(static_cast<double>(x.front().size()));
        for(double jitter:{1e-8,1e-6,1e-4}){
            lower.assign(n*n,0);bool ok=true;
            for(std::size_t i=0;i<n && ok;++i)for(std::size_t j=0;j<=i;++j){
                double s=kernel(x[i],x[j],length)+(i==j?jitter:0);
                for(std::size_t k=0;k<j;++k)s-=lower[i*n+k]*lower[j*n+k];
                if(i==j){if(!(s>0) || !std::isfinite(s)){ok=false;break;}lower[i*n+j]=std::sqrt(s);}
                else lower[i*n+j]=s/lower[j*n+j];
            }
            if(!ok)continue;
            Vector tmp(n);alpha.assign(n,0);
            for(std::size_t i=0;i<n;++i){double s=(y[i]-mean)/deviation;for(std::size_t j=0;j<i;++j)s-=lower[i*n+j]*tmp[j];tmp[i]=s/lower[i*n+i];}
            for(std::size_t i=n;i-->0;){double s=tmp[i];for(std::size_t j=i+1;j<n;++j)s-=lower[j*n+i]*alpha[j];alpha[i]=s/lower[i*n+i];}
            return true;
        }
        return false;
    }
    double lowerConfidenceBound(const Vector& z)const{
        const auto n=points.size();Vector k(n),v(n);double mu=0;
        for(std::size_t i=0;i<n;++i){k[i]=kernel(z,points[i],length);mu+=k[i]*alpha[i];}
        double variance=1;
        for(std::size_t i=0;i<n;++i){double s=k[i];for(std::size_t j=0;j<i;++j)s-=lower[i*n+j]*v[j];v[i]=s/lower[i*n+i];variance-=v[i]*v[i];}
        return mean+deviation*(mu-2.0*std::sqrt(std::max(0.0,variance)));
    }
};

struct Context {
    const Problem& p;const Options& o;
    Clock::time_point start=Clock::now();
    std::size_t evaluations=0,calls=0,invalid=0;
    Sample cache,best;bool cached=false,hasBest=false;
    double fScale=1;
    double seconds()const{return std::chrono::duration<double>(Clock::now()-start).count();}
    void check()const{
        // 预留一次真实评估给最终复核。
        if(evaluations>=o.maxEvaluations-1)throw Stop{"evaluation_budget"};
        if(seconds()>=o.maxSeconds)throw Stop{"time_budget"};
    }
    double signedValue(double f)const{return p.goal==Goal::Minimize?f:-f;}
    bool better(const Sample& a,const Sample& b)const{
        if(a.valid!=b.valid)return a.valid;
        if(!a.valid)return false;
        if(a.feasible!=b.feasible)return a.feasible;
        if(!a.feasible && a.excess!=b.excess)return a.excess<b.excess;
        return signedValue(a.f)<signedValue(b.f);
    }
    Sample evaluate(const Vector& z,bool verify=false){
        if(!verify){check();if(cached && cache.z==z)return cache;}
        ++evaluations;Sample s;s.z=z;s.x.resize(z.size());
        for(std::size_t j=0;j<z.size();++j){
            s.x[j]=p.variables[j].lower+z[j]*(p.variables[j].upper-p.variables[j].lower);
            s.x[j]=std::clamp(s.x[j],p.variables[j].lower,p.variables[j].upper);
        }
        auto call=[&](const Function& f){
            ++calls;
            try{double r=f(s.x);if(!std::isfinite(r))s.valid=false;return r;}
            catch(const std::runtime_error&){s.valid=false;return std::numeric_limits<double>::quiet_NaN();}
            catch(const std::domain_error&){s.valid=false;return std::numeric_limits<double>::quiet_NaN();}
        };
        s.f=call(p.objective);
        for(const auto& c:p.constraints){
            double r=call(c.value);s.residuals.push_back(r);
            if(!std::isfinite(r))continue;
            double violation=c.relation==Relation::Equal?std::abs(r):std::max(0.0,r);
            s.maxViolation=std::max(s.maxViolation,violation);
            s.excess=std::max(s.excess,std::max(0.0,violation-c.tolerance)/c.scale);
        }
        if(!std::isfinite(s.excess))s.valid=false;
        s.feasible=s.valid && s.excess==0;
        if(!s.valid){++invalid;s.excess=std::numeric_limits<double>::infinity();s.maxViolation=s.excess;}
        cache=s;cached=true;
        if(!hasBest || better(s,best)){best=s;hasBest=true;}
        return s;
    }
    double merit(const Sample& s)const{
        if(!s.valid)return 1e6;
        // 代理模型仅用于选点；最终优劣按真实可行性和原目标判断。
        return std::atan(signedValue(s.f)/fScale)+10*std::log1p(std::min(s.excess,1e30));
    }
};
struct Callback { Context* c;int constraint; };
double callback(const Vector& z,Vector&,void* data){
    auto& cb=*static_cast<Callback*>(data);auto s=cb.c->evaluate(z);
    if(cb.constraint<0){if(!s.valid)return BAD;return cb.c->signedValue(s.f)/cb.c->fScale;}
    std::size_t i=static_cast<std::size_t>(cb.constraint);
    return std::isfinite(s.residuals[i])?s.residuals[i]/cb.c->p.constraints[i].scale:BAD;
}

void validate(const Problem& p,const Options& o){
    if(p.variables.empty() || p.variables.size()>32 || !p.objective)throw std::invalid_argument("Expected 1..32 continuous variables and an objective");
    for(const auto& v:p.variables)if(!std::isfinite(v.lower)||!std::isfinite(v.upper)||!std::isfinite(v.initial)||!(v.lower<v.upper)||!std::isfinite(v.upper-v.lower)||v.initial<v.lower||v.initial>v.upper)throw std::invalid_argument("Require finite lower < upper and initial inside bounds");
    for(const auto& c:p.constraints)if(!c.value||!std::isfinite(c.tolerance)||c.tolerance<0||!std::isfinite(c.scale)||c.scale<=0)throw std::invalid_argument("Invalid constraint tolerance/scale");
    if(o.initialSamples<4 || o.initialSamples>256 || o.learningSteps>128 || o.candidatePool<16 || o.candidatePool>10000 || o.localStarts<1 || o.localStarts>100 || o.localMaxEvaluations<1 || o.maxEvaluations<3 || !std::isfinite(o.maxSeconds)||o.maxSeconds<=0 || !std::isfinite(o.xTolerance)||o.xTolerance<=0 || !std::isfinite(o.initialStep)||o.initialStep<=0||o.initialStep>1)throw std::invalid_argument("Invalid optimization options");
}
}

Result optimize(const Problem& p,const Options& o){
    validate(p,o);Context c{p,o};Result result;
    std::mt19937_64 random(o.seed);std::uniform_real_distribution<double> uniform(0,1);
    auto randomPoint=[&](){Vector z(p.variables.size());for(double& x:z)x=uniform(random);return z;};
    Vector initial(p.variables.size());
    for(std::size_t j=0;j<initial.size();++j)initial[j]=(p.variables[j].initial-p.variables[j].lower)/(p.variables[j].upper-p.variables[j].lower);
    std::vector<Sample> training;
    try{
        training.push_back(c.evaluate(initial));
        // 拉丁超立方采样：每个维度均匀分层，并独立打乱层次。
        const std::size_t count=o.initialSamples-1,dim=initial.size();
        std::vector<Vector> design(count,Vector(dim));std::vector<std::size_t> order(count);
        std::iota(order.begin(),order.end(),0);
        for(std::size_t j=0;j<dim;++j){std::shuffle(order.begin(),order.end(),random);for(std::size_t i=0;i<count;++i)design[i][j]=(order[i]+uniform(random))/count;}
        for(const auto& z:design)training.push_back(c.evaluate(z));
        Vector magnitude;for(const auto& s:training)if(s.valid)magnitude.push_back(std::abs(s.f));
        if(!magnitude.empty()){std::sort(magnitude.begin(),magnitude.end());c.fScale=std::max(1.0,magnitude[magnitude.size()/2]);}
        for(std::size_t iteration=0;iteration<o.learningSteps;++iteration){
            c.check();std::vector<Vector> x;Vector y;
            for(const auto& s:training)if(s.valid){x.push_back(s.z);y.push_back(c.merit(s));}
            GaussianProcess gp;bool fitted=gp.fit(x,y);Vector selected=randomPoint();double best=std::numeric_limits<double>::infinity();
            // 部分轮次直接探索，避免代理模型把未知区域过早排除。
            if(fitted && iteration%5!=0){
                auto promising=*std::min_element(training.begin(),training.end(),[&](const auto& a,const auto& b){return c.merit(a)<c.merit(b);});
                for(std::size_t k=0;k<o.candidatePool;++k){
                    if(k%32==0)c.check();
                    Vector candidate=randomPoint();
                    if(k%2==0){std::normal_distribution<double> normal(0,0.12);for(std::size_t j=0;j<dim;++j)candidate[j]=std::clamp(promising.z[j]+normal(random),0.0,1.0);}
                    bool duplicate=false;for(const auto& old:training){double d=0;for(std::size_t j=0;j<dim;++j)d+=(candidate[j]-old.z[j])*(candidate[j]-old.z[j]);if(d<1e-16){duplicate=true;break;}}
                    if(duplicate)continue;
                    double score=gp.lowerConfidenceBound(candidate);if(score<best){best=score;selected=candidate;}
                }
            }
            training.push_back(c.evaluate(selected));++result.learningEvaluations;
        }
        std::sort(training.begin(),training.end(),[&](const auto& a,const auto& b){return c.better(a,b);});
        std::vector<Vector> starts{initial};
        // 一半从优质采样点出发，另一半随机；避免起点都集中在一个局部区域。
        for(const auto& s:training){if(starts.size()>=(o.localStarts+1)/2)break;if(s.valid && std::find(starts.begin(),starts.end(),s.z)==starts.end())starts.push_back(s.z);}
        while(starts.size()<o.localStarts)starts.push_back(randomPoint());
        for(auto z:starts){
            c.check();nlopt::opt solver(nlopt::LN_COBYLA,static_cast<unsigned>(dim));
            solver.set_lower_bounds(Vector(dim,0));solver.set_upper_bounds(Vector(dim,1));
            solver.set_xtol_abs(o.xTolerance);solver.set_initial_step(o.initialStep);
            solver.set_maxeval(o.localMaxEvaluations);solver.set_maxtime(std::max(0.001,o.maxSeconds-c.seconds()));
            std::vector<Callback> callbacks;callbacks.reserve(p.constraints.size()+1);
            callbacks.push_back({&c,-1});solver.set_min_objective(callback,&callbacks.back());
            for(std::size_t i=0;i<p.constraints.size();++i){
                callbacks.push_back({&c,static_cast<int>(i)});const auto& constraint=p.constraints[i];
                if(constraint.relation==Relation::Equal)solver.add_equality_constraint(callback,&callbacks.back(),constraint.tolerance/constraint.scale);
                else solver.add_inequality_constraint(callback,&callbacks.back(),constraint.tolerance/constraint.scale);
            }
            double value=0;
            try{auto code=solver.optimize(z,value);result.localStopCodes.push_back(static_cast<int>(code));}
            catch(const nlopt::roundoff_limited&){result.localStopCodes.push_back(static_cast<int>(nlopt::ROUNDOFF_LIMITED));}
            // nlopt.hpp 将未知回调异常转换为 FAILURE；检查预算以恢复准确停止原因。
            catch(const std::runtime_error&){c.check();throw;}
            c.evaluate(z);
        }
        result.termination="search_completed";
    }catch(const Stop& stop){result.termination=stop.reason;}
    // 重新调用真实函数，不能把代理模型预测当作最终答案。
    if(c.hasBest){
        Sample final=c.evaluate(c.best.z,true);result.x=final.x;result.objective=final.f;
        result.feasible=final.feasible;result.maxViolation=final.maxViolation;
        for(std::size_t i=0;i<p.constraints.size();++i){
            const auto& constraint=p.constraints[i];double r=final.residuals[i];
            double v=std::isfinite(r)?(constraint.relation==Relation::Equal?std::abs(r):std::max(0.0,r)):std::numeric_limits<double>::infinity();
            result.constraints.push_back({constraint.name,r,v,constraint.tolerance,std::isfinite(r)&&v<=constraint.tolerance});
        }
    }
    result.evaluations=c.evaluations;result.functionCalls=c.calls;result.invalidPoints=c.invalid;result.seconds=c.seconds();
    return result;
}
}
