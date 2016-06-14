#ifndef _GUARD_REFERENCERI_HPP_
#define _GUARD_REFERENCERI_HPP_

#include <pulsar/modulebase/TwoElectronIntegral.hpp>
#include <pulsar/system/BasisSet.hpp>

class ReferenceERI : public pulsar::modulebase::TwoElectronIntegral
{
public:
    using pulsar::modulebase::TwoElectronIntegral::TwoElectronIntegral;

    virtual void Initialize_(unsigned int deriv,
                             const pulsar::datastore::Wavefunction & wfn,
                             const pulsar::system::BasisSet & bs1,
                             const pulsar::system::BasisSet & bs2,
                             const pulsar::system::BasisSet & bs3,
                             const pulsar::system::BasisSet & bs4);


    virtual uint64_t Calculate_(size_t shell1, size_t shell2,
                                size_t shell3, size_t shell4,
                                double * outbuffer, size_t bufsize);


private:
    std::shared_ptr<pulsar::system::BasisSet> bs1_, bs2_, bs3_, bs4_;

    std::vector<double> work_;
    double * sourcework_;
    double * transformwork_;

};


#endif
