# kws-framework — custom wake-word training & deployment

A tiny, self-contained framework (Python 3 + numpy only — **no WakeNet, no TFLM**) that
trains a DS-CNN wake-word model, quantizes it to int8, and generates the C header the
ESP32-S3 firmware runs. The int8 inference reference is **bit-exact** with the on-device
C engine (`firmware/core/src/kws_model.c`), proven by a committed parity fixture.

## Layout

- `kwslib/` — library:
  - `quant.py` / `kernels.py` — bit-exact mirrors of `firmware/core/src/nn/{quant,kernels}.c`
  - `layers.py` / `model.py` / `optim.py` — float DS-CNN forward+backward + Adam
  - `quantize.py` — float → int8 (TFLite-style per-channel/per-tensor)
  - `infer_int8.py` — int8 inference reference (mirrors `kws_model.c`)
  - `codegen.py` — emits `kws_model_data.h`
  - `features.py` / `dataset.py` — log-mel front-end + dataset/augmentation
- `tools/`:
  - `train.py` — dataset → train → quantize → codegen
  - `gen_parity_case.py` — regenerate the committed C↔Python parity fixture

## Usage

```sh
# Train a demo model and emit a deployable C header
python3 tools/train.py --epochs 80 --out artifacts/kws_model_data.h

# Regenerate the parity fixture (committed under tests/host/generated/)
python3 tools/gen_parity_case.py
```

## Tests

Run from the repo root with `make test` (Python tests are wired into CTest), or directly:

```sh
cd tests/py && for t in test_quant test_layers_gradcheck test_train_learns \
    test_quantize_codegen test_parity_regen; do python3 $t.py; done
```

The C parity test (`tests/host/test_parity.c`) asserts the C engine reproduces the Python
int8 logits from the committed fixture bit-for-bit.
