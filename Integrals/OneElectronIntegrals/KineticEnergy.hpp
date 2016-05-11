#ifndef _GUARD_KINETICENERGY_HPP_
#define _GUARD_KINETICENERGY_HPP_

#include <pulsar/modulebase/OneElectronIntegral.hpp>
#include <pulsar/system/BasisSet.hpp>

class KineticEnergy : public pulsar::modulebase::OneElectronIntegral
{
    public:
        using pulsar::modulebase::OneElectronIntegral::OneElectronIntegral;

        virtual void SetBases_(const pulsar::system::System & sys,
                               const std::string & bs1, const std::string & bs2);

        virtual uint64_t Calculate_(uint64_t deriv, uint64_t shell1, uint64_t shell2,
                                    double * outbuffer, size_t bufsize);

    private:
        std::vector<double> work_;

        double * transformwork_;
        double * sourcework_;
        double * xyzwork_[6];

        std::shared_ptr<pulsar::system::BasisSet> bs1_, bs2_;
};


#endif
