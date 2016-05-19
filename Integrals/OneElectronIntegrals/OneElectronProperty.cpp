#include "OneElectronProperty.hpp"

#include <pulsar/system/BasisSet.hpp>
#include <pulsar/system/AOIterator.hpp>
#include <pulsar/modulebase/OneElectronIntegral.hpp>



using namespace pulsar::modulemanager;
using namespace pulsar::exception;
using namespace pulsar::system;
using namespace pulsar::datastore;
using namespace pulsar::modulebase;
using namespace pulsar::math;


std::vector<double>
OneElectronProperty::Calculate_(const Wavefunction & wfn,
                                const std::string & bs1,
                                const std::string & bs2)

{
    const auto basisset1 = wfn.system->GetBasisSet(bs1);
    const auto basisset2 = wfn.system->GetBasisSet(bs2);
    const size_t nshell1 = basisset1.NShell();
    const size_t nshell2 = basisset2.NShell();

    size_t worksize = basisset1.MaxNFunctions()*basisset2.MaxNFunctions();

    std::vector<double> work(worksize);

    auto mod = CreateChildFromOption<OneElectronIntegral>("KEY_ONEEL_MOD");
    mod->SetBases(*wfn.system, bs1, bs2);

    double val = 0; 
    for(size_t n1 = 0; n1 < nshell1; n1++)
    {
        const auto & sh1 = basisset1.Shell(n1);
        const size_t rowstart = basisset1.ShellStart(n1);

        for(size_t n2 = 0; n2 < nshell2; n2++)
        {
            const auto & sh2  = basisset2.Shell(n2);
            const size_t colstart = basisset2.ShellStart(n2);

            // calculate
            size_t ncalc = mod->Calculate(0, n1, n2, work.data(), worksize);

            // iterate and fill in the matrix
            AOIterator<2> aoit({sh1, sh2}, false);

            // make sure the right number of integrals was returned
            if(ncalc != aoit.NFunctions())
                throw GeneralException("Bad number of integrals returned",
                                       "ncalc", ncalc, "expected", aoit.NFunctions());


            do {
                const size_t i = rowstart+aoit.ShellFunctionIdx<0>();
                const size_t j = colstart+aoit.ShellFunctionIdx<1>();

                for(auto s : wfn.opdm->GetSpins(Irrep::A))
                    val += wfn.opdm->Get(Irrep::A, s)(i,j) * work[aoit.TotalIdx()];
            } while(aoit.Next());
        }
    }

    return {val};

}