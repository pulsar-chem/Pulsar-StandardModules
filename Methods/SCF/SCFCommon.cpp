#include <pulsar/system/AOIterator.hpp>
#include "Methods/SCF/SCFCommon.hpp"

using Eigen::SelfAdjointEigenSolver;
using Eigen::MatrixXd;
using Eigen::VectorXd;

using namespace pulsar::modulemanager;
using namespace pulsar::modulebase;
using namespace pulsar::system;
using namespace pulsar::datastore;
using namespace pulsar::math;
using namespace pulsar::exception;
using namespace pulsar::output;


namespace pulsarmethods{


std::vector<double>
FillTwoElectronVector(ModulePtr<TwoElectronIntegral> & mod,
                      const BasisSet & bs)
{
    const size_t nao = bs.NFunctions();
    const size_t nshell = bs.NShell();
    const size_t maxnfunc = bs.MaxNFunctions();

    const size_t nao12 = (nao*(nao+1))/2;
    const size_t nao1234 = (nao12*(nao12+1))/2;
    const size_t bufsize = maxnfunc*maxnfunc*maxnfunc*maxnfunc;

    std::vector<double> eri(nao1234);
    std::vector<double> eribuf(bufsize);

    size_t i_start = 0;
    for(size_t i = 0; i < nshell; i++)
    {
        const auto & sh1 = bs.Shell(i);
        size_t j_start = 0;

        for(size_t j = 0; j <= i; j++)
        {
            const auto & sh2 = bs.Shell(j);
            size_t k_start = 0;

            for(size_t k = 0; k < nshell; k++)
            {
                const auto & sh3 = bs.Shell(k);
                size_t l_start = 0;

                for(size_t l = 0; l <= k; l++)
                {
                    if(INDEX2(k,l) > INDEX2(i,j))
                        continue;

                    const auto & sh4 = bs.Shell(l);

                    uint64_t ncalc = mod->Calculate(i, j, k, l, eribuf.data(), bufsize); 

                    AOIterator<4> aoit({sh1, sh2, sh3, sh4}, false);

                    // make sure the right number of integrals was returned
                    if(ncalc != aoit.NFunctions())
                        throw GeneralException("Bad number of integrals returned",
                                               "ncalc", ncalc, "expected", aoit.NFunctions());

                    do { 
                        const size_t full_i = i_start+aoit.ShellFunctionIdx<0>();
                        const size_t full_j = j_start+aoit.ShellFunctionIdx<1>();
                        const size_t full_k = k_start+aoit.ShellFunctionIdx<2>();
                        const size_t full_l = l_start+aoit.ShellFunctionIdx<3>();

                        eri.at(INDEX4(full_i, full_j, full_k, full_l)) = eribuf.at(aoit.TotalIdx());
                    } while(aoit.Next());

                    l_start += sh4.NFunctions();   
                }
                k_start += sh3.NFunctions();
            }
            j_start += sh2.NFunctions();
        }
        i_start += sh1.NFunctions();
    }

    return std::move(eri);
}


IrrepSpinVectorD FindOccupations(size_t nelec)
{
    IrrepSpinVectorD occ;

    if(nelec %2 == 0)
    {
        size_t ndocc = nelec/2;
        VectorXd docc(ndocc);
        for(size_t i = 0; i < ndocc; i++)
            docc(i) = 2.0;
        occ.Take(Irrep::A, 0, std::make_shared<EigenVectorImpl>(std::move(docc)));
    }
    else
    {
        size_t nbetaocc = nelec/2; // integer division
        size_t nalphaocc = nelec - nbetaocc;

        VectorXd alphaocc(nalphaocc), betaocc(nbetaocc);
        for(size_t i = 0; i < nalphaocc; i++) alphaocc(i) = 1.0;
        for(size_t i = 0; i < nbetaocc; i++) betaocc(i) = 1.0;

        occ.Take(Irrep::A,  1, std::make_shared<EigenVectorImpl>(std::move(alphaocc)));
        occ.Take(Irrep::A, -1, std::make_shared<EigenVectorImpl>(std::move(betaocc)));
    }

    return occ;
}


IrrepSpinMatrixD FormDensity(const IrrepSpinMatrixD & Cmat,
                             const IrrepSpinVectorD & occ)
{
    IrrepSpinMatrixD Dmat;

    for(auto ir : Cmat.GetIrreps())
    for(auto s : Cmat.GetSpins(ir))
    {
        const MatrixXd & c = *(convert_to_eigen(Cmat.Get(ir, s)));
        const VectorXd & o = *(convert_to_eigen(occ.Get(ir, s)));

        MatrixXd d(c.rows(), c.cols());

        for(long i = 0; i < c.rows(); i++)
        for(long j = 0; j < c.cols(); j++)
        {
            d(i,j) = 0.0;
            for(long m = 0; m < o.size(); m++)
               d(i,j) += o(m) * c(i,m) * c(j,m);
        }

        auto dimpl = std::make_shared<EigenMatrixImpl>(std::move(d));
        Dmat.Take(ir, s, std::move(dimpl));
    }

    return Dmat;
}


double CalculateRMSDens(const IrrepSpinMatrixD & m1, const IrrepSpinMatrixD & m2)
{
    if(!m1.SameStructure(m2))
        throw GeneralException("Density matrices have different structure");

    double rms = 0.0;

    for(Irrep ir : m1.GetIrreps())
    for(int spin : m1.GetSpins(ir))
    {

        const MatrixXd & mat1 = *(convert_to_eigen(m1.Get(ir, spin)));
        const MatrixXd & mat2 = *(convert_to_eigen(m1.Get(ir, spin)));

        if(mat1.rows() != mat2.rows())
            throw GeneralException("Density matrices have different number of rows");
        if(mat1.cols() != mat2.cols())
            throw GeneralException("Density matrices have different number of columns");

        for(long i = 0; i < mat1.rows(); i++)
        for(long j = 0; j < mat1.cols(); j++)
        {
            const double diff = mat1(i,j) - mat2(i,j);
            rms += diff*diff;
        }
    }

    return sqrt(rms);
}


double CalculateEnergy(const MatrixXd & Hcore, double nucrep,
                       const IrrepSpinMatrixD & Dmat,
                       const IrrepSpinMatrixD & Fmat,
                       OutputStream & out)
{
    double energy = 0.0;
    double oneelectron = 0.0;
    double twoelectron = 0.0;

    for(auto ir : Dmat.GetIrreps())
    for(auto s : Dmat.GetSpins(ir))
    {
        const MatrixXd & d = *(convert_to_eigen(Dmat.Get(ir, s)));
        const MatrixXd & f = *(convert_to_eigen(Fmat.Get(ir, s)));

        for(long i = 0; i < d.rows(); i++)
        for(long j = 0; j < d.cols(); j++)
        {
            oneelectron += d(i,j) * Hcore(i,j);
            twoelectron += 0.5 * d(i,j) * f(i,j);
        }
    }

    twoelectron -= 0.5*oneelectron;
    energy = oneelectron + twoelectron;

    out.Output("            One electron: %16.8e\n", oneelectron);
    out.Output("            Two electron: %16.8e\n", twoelectron);
    out.Output("        Total Electronic: %16.8e\n", energy);
    out.Output("       Nuclear Repulsion: %16.8e\n", nucrep);

    energy += nucrep;
    out.Output("            Total energy: %16.8e\n", energy);

    return energy;
}

Eigen::MatrixXd FormS12(const Eigen::MatrixXd & S)
{
    // diagonalize the overlap
    SelfAdjointEigenSolver<MatrixXd> esolve(S);
    MatrixXd s_evec = esolve.eigenvectors();
    VectorXd s_eval = esolve.eigenvalues();

    // not sure an easier way to do this
    for(int i = 0; i < s_eval.size(); i++)
        s_eval(i) = 1.0/sqrt(s_eval(i));

    // the S^(-1/2) matrix
    return (s_evec * s_eval.asDiagonal() * s_evec.transpose());
}

}//End namespace
