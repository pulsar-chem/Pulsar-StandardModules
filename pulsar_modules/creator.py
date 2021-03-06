from .methods.composite_methods import CP
from .methods.composite_methods import HelgakerCBS
from .methods.composite_methods import FellerCBS
from .methods.composite_methods import FPA
from .methods.composite_methods import MyCrazyCompositeMethod
from .methods.composite_methods import QMMM
from .methods.optimizer import GeometryOptimizer
from pulsar import ModuleCreationFuncs


def insert_supermodule():
    cf = ModuleCreationFuncs()
    cf.add_py_creator("CP",CP.CP)
    cf.add_py_creator("CorrelationEnergy",HelgakerCBS.CorrelationEnergy)
    cf.add_py_creator("HelgakerCBS",HelgakerCBS.HelgakerCBS)
    cf.add_py_creator("FellerCBS",FellerCBS.FellerCBS)
    cf.add_py_creator("FPA",FPA.FPA)
    cf.add_py_creator("MyCrzyCompMeth",MyCrazyCompositeMethod.MyCzyCM)
    cf.add_py_creator("GeometryOptimizer",GeometryOptimizer.GeometryOptimizer)
    cf.add_py_creator("EEQMMM",QMMM.EEQMMM)
    return cf
