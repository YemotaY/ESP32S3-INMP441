"""kwslib: custom wake-word training & deployment framework (no WakeNet, no TFLM).

Off-device counterpart to the on-device C engine. The int8 inference reference is
bit-exact with firmware/core, enabling the C<->Python parity guarantee.
"""
from . import quant, kernels, layers, model, optim, infer_int8, quantize, codegen, features, dataset

__all__ = [
    "quant", "kernels", "layers", "model", "optim",
    "infer_int8", "quantize", "codegen", "features", "dataset",
]
