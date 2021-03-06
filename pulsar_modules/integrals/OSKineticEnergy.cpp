#include <pulsar/system/AOOrdering.hpp>
#include <pulsar/system/SphericalTransformIntegral.hpp>
#include <pulsar/constants.h>

#include "Common/BasisSetCommon.hpp"
#include "Integrals/OSKineticEnergy.hpp"


// Get a value of S_IJ
#define S_IJ(i,j) (s_ij[((i)*(nam2) + j)])
#define T_IJ(i,j) (t_ij[((i)*(nam2) + j)])

using namespace pulsar::exception;
using namespace pulsar::system;
using namespace pulsar::datastore;

namespace psr_modules {
namespace integrals {

uint64_t OSKineticEnergy::calculate_(uint64_t shell1, uint64_t shell2,
                                     double * outbuffer, size_t bufsize)
{
    const BasisSetShell & sh1 = bs1_->shell(shell1);
    const BasisSetShell & sh2 = bs2_->shell(shell2);

    const size_t nfunc = sh1.n_functions() * sh2.n_functions();

    if(bufsize < nfunc)
        throw PulsarException("Buffer is too small", "size", bufsize, "required", nfunc);


    // degree of general contraction
    size_t ngen1 = sh1.n_general_contractions();
    size_t ngen2 = sh2.n_general_contractions();

    ////////////////////////////////////////
    // These vectors store the ordering of
    // cartesian basis functions for each
    // general contraction. Remember that
    // general contractions include combined AM
    // contractions.
    //////////////////////////////////////////
    std::vector<const std::vector<IJK> *> sh1_ordering, sh2_ordering;

    for(size_t g1 = 0; g1 < ngen1; g1++)
        sh1_ordering.push_back(&cartesian_ordering(sh1.general_am(g1)));
    for(size_t g2 = 0; g2 < ngen2; g2++)
        sh2_ordering.push_back(&cartesian_ordering(sh2.general_am(g2)));


    // The total AM of the shell. May be negative
    const int am1 = sh1.am();
    const int am2 = sh2.am();


    // coordinates from each shell
    const CoordType xyz1 = sh1.get_coords();
    const CoordType xyz2 = sh2.get_coords();

    const double AB[3] = { xyz1[0] - xyz2[0], xyz1[1] - xyz2[1], xyz1[2] - xyz2[2] };
    const double AB2[3] = { AB[0]*AB[0], AB[1]*AB[1], AB[2]*AB[2] };

    // Used for dimensioning and loops. Storage goes from
    // [0, am], so we need to add one.
    const int nam1 = std::abs(am1) + 1;
    const int nam2 = std::abs(am2) + 1;

    // We need to zero the workspace. Actually, not all of it,
    // but this is easier
    std::fill(work_.begin(), work_.end(), 0.0);

    /////////////////////////////////////////////////////////
    // General notes about the following
    //
    // This is the OS algorithm taken pretty much verbatim
    // from Helgaker, Jorgensen, & Olsen. For each x,y,z
    // direction, we construct arrays of Tij and Sij of length nam1*nam2.
    // These are placed in the pre-allocated workspace.
    //
    // We then handle multiplication with coefficients within the
    // general contractions.
    //
    // The indexing of these arrays is pretty straightforward.
    // Sij = ptr[i*nam2+j]. This is in the S_IJ and T_IJ macros, hopefully
    // to make the code clearer.
    //
    // Also note that we build the OS overlap cartesian terms directly here, rather
    // than a call to os_overlap_terms.
    /////////////////////////////////////////////////////////
    // loop over primitives
    const size_t nprim1 = sh1.n_primitives();
    const size_t nprim2 = sh2.n_primitives();

    for(size_t a = 0; a < nprim1; a++)
    {
        const double a1 = sh1.alpha(a);
        const double a1xyz[3] = { a1*xyz1[0], a1*xyz1[1], a1*xyz1[2] };
        const double a1sq = 2.0*a1*a1;

        for(size_t b = 0; b < nprim2; b++)
        {
            const double a2 = sh2.alpha(b);
            const double oop = 1.0/(a1 + a2); // = 1/p = 1/(a1 + a2)
            const double mu = a1*a2*oop;   // (a1*a2)/(a1+a2)


            const double oo2p = 0.5*oop;
            const double a2xyz[3] = { a2*xyz2[0], a2*xyz2[1], a2*xyz2[2] };

            const double P[3] = { (a1xyz[0]+a2xyz[0])*oop,
                                  (a1xyz[1]+a2xyz[1])*oop,
                                  (a1xyz[2]+a2xyz[2])*oop };

            const double PA[3] = { P[0] - xyz1[0], P[1] - xyz1[1], P[2] - xyz1[2] };
            const double PB[3] = { P[0] - xyz2[0], P[1] - xyz2[1], P[2] - xyz2[2] };
            const double PA2[3] = { PA[0]*PA[0], PA[1]*PA[1], PA[2]*PA[2] };


            // three cartesian directions
            for(int d = 0; d < 3; d++)
            {
                // the workspace for this direction
                // THSE ARE THEN ACCESSED THROUGH THE S_IJ AND T_IJ MACROS
                double * const RESTRICT s_ij = xyzwork_[d];
                double * const RESTRICT t_ij = xyzwork_[d+3];

                S_IJ(0,0) = sqrt(PI * oop) * exp(-mu * AB2[d]);
                T_IJ(0,0) = S_IJ(0,0)*(a1 - a1sq*(PA2[d] + oo2p));

                // do j = 0 for all remaining i
                for(int i = 1; i < nam1; i++)
                {
                    S_IJ(i,0) = PA[d]*S_IJ(i-1,0);
                    if(i > 1)
                        S_IJ(i,0) += (i-1)*oo2p*S_IJ(i-2,0);

                    T_IJ(i,0) = PA[d]*T_IJ(i-1,0) + 2*mu*S_IJ(i,0);
                    if(i > 1)
                        T_IJ(i,0) += (i-1)*oo2p*T_IJ(i-2,0) - a2*oop*(i-1)*S_IJ(i-2,0);
                }

                // now do i = 0 for all remaining j
                for(int j = 1; j < nam2; j++)
                {
                    S_IJ(0,j) = PB[d]*S_IJ(0,j-1);
                    if(j > 1)
                        S_IJ(0,j) += (j-1)*oo2p*S_IJ(0,j-2);
                    T_IJ(0,j) = PB[d]*T_IJ(0,j-1) + 2*mu*S_IJ(0,j); 
                    if(j > 1)
                        T_IJ(0,j) += (j-1)*oo2p*T_IJ(0,j-2) - a1*oop*(j-1)*S_IJ(0,j-2);
                }

                // now all the rest
                for(int i = 1; i < nam1; i++)
                for(int j = 1; j < nam2; j++)
                {
                    S_IJ(i,j) = PB[d]*S_IJ(i,j-1) + oo2p*i*S_IJ(i-1,j-1);
                    if(j > 1)
                        S_IJ(i,j) += oo2p*(j-1)*S_IJ(i,j-2);
                    T_IJ(i,j) = PB[d]*T_IJ(i,j-1) + oo2p*i*T_IJ(i-1,j-1) + 2*mu*S_IJ(i,j);
                    if(j > 1)
                        T_IJ(i,j) += oo2p*(j-1)*T_IJ(i,j-2) - a1*oop*(j-1)*S_IJ(i,j-2);
                }
            }

            // general contraction and combined am
            size_t outidx = 0;
            for(size_t g1 = 0; g1 < ngen1; g1++)
            for(size_t g2 = 0; g2 < ngen2; g2++)
            {
                const double prefac = sh1.coef(g1, a) * sh2.coef(g2, b);

                // go over the orderings for this AM
                for(const IJK & ijk1 : *(sh1_ordering[g1]))
                for(const IJK & ijk2 : *(sh2_ordering[g2]))
                {
                    const int xidx = ijk1[0]*nam2 + ijk2[0];
                    const int yidx = ijk1[1]*nam2 + ijk2[1];
                    const int zidx = ijk1[2]*nam2 + ijk2[2];

                                                                                              // vv from MEST vv
                    const double val = xyzwork_[3][xidx]*xyzwork_[1][yidx]*xyzwork_[2][zidx]  // Tij*Skl*Smn
                                     + xyzwork_[0][xidx]*xyzwork_[4][yidx]*xyzwork_[2][zidx]  // Sij*Tkl*Smn
                                     + xyzwork_[0][xidx]*xyzwork_[1][yidx]*xyzwork_[5][zidx]; // Sij*Skl*Tmn

                    // remember: a and b are indices of primitives
                    sourcework_[outidx++] += prefac * val;
                }
            }
        }
    }

    // performs the spherical transform, if necessary
    CartesianToSpherical_2Center(sh1, sh2, sourcework_, outbuffer, transformwork_, 1);

    return nfunc;
}



void OSKineticEnergy::initialize_(unsigned int deriv,
                                  const Wavefunction & wfn,
                                  const BasisSet & bs1,
                                  const BasisSet & bs2)
{
    if(deriv != 0)
        throw NotYetImplementedException("Not Yet Implemented: OSKineticEnergy integral with deriv != 0");

    // from common components
    bs1_ = NormalizeBasis(cache(), out, bs1);
    bs2_ = NormalizeBasis(cache(), out, bs2);

    ///////////////////////////////////////
    // Determine the size of the workspace
    ///////////////////////////////////////

    // storage size for each x,y,z component
    int max1 = bs1_->max_am();
    int max2 = bs2_->max_am();
    size_t worksize = (max1+1)*(max2+1);  // for each component, we store [0, am]

    // find the maximum number of cartesian functions, not including general contraction
    size_t maxsize1 = bs1_->max_property(n_cartesian_gaussian_for_shell_am);
    size_t maxsize2 = bs2_->max_property(n_cartesian_gaussian_for_shell_am);
    size_t transformwork_size = maxsize1 * maxsize2; 

    // find the maximum number of cartesian functions, including general contraction
    maxsize1 = bs1_->max_property(n_cartesian_gaussian_in_shell);
    maxsize2 = bs2_->max_property(n_cartesian_gaussian_in_shell);
    size_t sourcework_size = maxsize1 * maxsize2;

    // allocate all at once, then partition
    work_.resize(6*worksize + transformwork_size + sourcework_size);
    xyzwork_[0] = work_.data();
    xyzwork_[1] = xyzwork_[0] + worksize;
    xyzwork_[2] = xyzwork_[1] + worksize;
    xyzwork_[3] = xyzwork_[2] + worksize;
    xyzwork_[4] = xyzwork_[3] + worksize;
    xyzwork_[5] = xyzwork_[4] + worksize;
    transformwork_ = xyzwork_[5] + worksize;
    sourcework_ = transformwork_ + transformwork_size;
}

} // close namespace integrals
} // close namespace psr_modules
