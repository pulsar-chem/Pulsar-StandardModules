#include <pulsar/output/Output.hpp>
#include <pulsar/modulemanager/ModuleManager.hpp>
#include "NuclearRepulsion.hpp"


using namespace pulsar::modulemanager;
using namespace pulsar::exception;
using namespace pulsar::system;
using namespace pulsar::datastore;


uint64_t NuclearRepulsion::Calculate_(uint64_t deriv, const System & sys,
                                      double * outbuffer, size_t bufsize)
{
    if(deriv != 0)
        throw NotYetImplementedException("Not Yet Implemented: Nuclear Repulsion with deriv != 0");

    if(bufsize == 0)
        throw GeneralException("Not enough space in output buffer");


    double enuc = 0.0;
    for(auto it1 = sys.begin(); it1 != sys.end(); ++it1)
    {
        auto it2 = it1;
        std::advance(it2, 1);

        for(; it2 != sys.end(); ++it2)
            enuc += (it1->GetZ() * it2->GetZ()) / it1->Distance(*it2);
    }

    outbuffer[0] = enuc;
    return 1;
}
