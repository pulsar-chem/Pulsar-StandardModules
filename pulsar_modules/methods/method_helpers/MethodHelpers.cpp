#include <unordered_map>
#include <pulsar/parallel/Parallel.hpp>
#include <pulsar/util/IterTools.hpp>
#include <pulsar/exception/PulsarException.hpp>
#include <pulsar/output/GlobalOutput.hpp>
#include <pulsar/util/Pybind11.hpp>
#include "pulsar_modules/common/ProgressBar.hpp"
#include "pulsar_modules/methods/method_helpers/MethodHelpers.hpp"

using std::vector;
using std::string;
using namespace pulsar;
using EMethodPtr=ModulePtr<EnergyMethod>;
using AtomMap_t=std::unordered_map<Atom,size_t>;

///Functor for running a task
class Task{
   private:
      EMethodPtr Method_;
      const Wavefunction& Wfn_;
      size_t Order_,TaskNum_;
   public:
      Task(const Wavefunction& Wfn,EMethodPtr&& Method, size_t Order,
           size_t TaskNum=0):
         Method_(std::move(Method)),Wfn_(Wfn),Order_(Order),TaskNum_(TaskNum){}
      DerivReturnType operator()()const{
        return Method_->deriv(Order_,Wfn_);
      }
};

void FillDeriv(vector<double>& Result,const vector<double>& SubResult,
               double Coeff,const System& Sys, 
               const AtomMap_t& SuperAtomMap,const AtomMap_t& SubAtomMap,
               size_t Order,vector<Atom> Idx,vector<size_t> Comp){
   //Handle energy
   if(Order==0)Result[0]+=Coeff*SubResult[0];
   else if(Idx.size()==(Order-1)){//Gradient lands here, Hessian and higher on recursion
      size_t SuperOff=0,SubOff=0;
      for(size_t i=0;i<Order-1;++i){//First offset
         size_t SuperStride=1,SubStride=1;
         for(size_t j=i;j<Order-1;++j){
            SuperStride*=3*SuperAtomMap.size();
            SubStride*=3*SubAtomMap.size();
         }
         SuperOff+=(SuperAtomMap.at(Idx[i])*3+Comp[i])*SuperStride;
         SubOff+=(SubAtomMap.at(Idx[i])*3+Comp[i])*SubStride;
      }
      for(const Atom& AtomI: Sys){//Unrolled loop
         //Second offsets
         const size_t SuperOff2=SuperAtomMap.at(AtomI)*3;
         const size_t SubOff2=SubAtomMap.at(AtomI)*3;
         for(size_t i: Range<0,3>())//Actual filling
            Result[SuperOff+SuperOff2+i]+=Coeff*SubResult[SubOff+SubOff2+i];
      }
   }
   else{//Hessian and higher land here
      for(const Atom& AtomI : Sys){
         Idx.push_back(AtomI);
         for(size_t i: Range<0,3>()){
            Comp.push_back(i);
            FillDeriv(Result,SubResult,Coeff,Sys,
                      SuperAtomMap,SubAtomMap,Order,Idx,Comp);
            Comp.pop_back();
         }
         Idx.pop_back();
      }
   }
   
}

vector<DerivReturnType> RunSeriesOfMethods(ModuleManager& MM,
                                   ID_t ID,
                                   const vector<string>& Keys,
                                   const vector<Wavefunction>& Wfns, 
                                   //const vector<double> Cs,
                                   size_t Deriv)
{
    const size_t NTasks=std::max(Keys.size(),Wfns.size());
    const bool SameMethod=(Keys.size()==1),SameSystem=(Wfns.size()==1);
    const bool BothSpecified=(!SameMethod && !SameSystem);
    if(SameMethod && SameSystem && NTasks!=1)
        throw PulsarException(
              "Minimally, either the number of systems or the number of methods"
              " must equal the number of coefficients",
              "NSystems=",Wfns.size(),"NMethods=",Keys.size(),"NCoefficients=",
              NTasks);
    if((BothSpecified || SameMethod) && Wfns.size()!=NTasks)
        throw PulsarException(
              "The number of coefficients must match the number of systems",
              "NSystems=",Wfns.size(),"NCoefficients=",NTasks);
    if((BothSpecified || SameSystem) && Keys.size()!=NTasks)
        throw PulsarException(
              "The number of coefficients must match the number of methods",
              "NMethods=",Keys.size(),"NCoefficients=",NTasks);
    


    
   vector<DerivReturnType> TempResults;
   ProgressBar PB(NTasks,pulsar::get_global_output());
   for(size_t i: Range<0>(NTasks)){
       const Wavefunction& WfnI=(SameSystem?Wfns[0]:Wfns[i]);
       const std::string& Key=(SameMethod?Keys[0]:Keys[i]);
       Task DaTask(WfnI,std::move(MM.get_module<EnergyMethod>(Key,ID)),Deriv);
       TempResults.push_back(DaTask());
       ++PB;
   }
   pulsar::get_global_output()<<std::endl;//For progress bar
   vector<DerivReturnType> Results;
   for(size_t i: Range<0>(NTasks))Results.push_back(TempResults[i]);
   return Results;
}

