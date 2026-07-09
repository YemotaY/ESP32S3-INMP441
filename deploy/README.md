# Deploying the SimonSays server

The server (STT + intent + debounce, `server/`) is a small host C program. It ships two
ways to run: a **scripted stub** (deterministic, no ML deps — great for demos and CI) and
a **whisper.cpp** backend for real speech-to-text. Both sit behind the same
`stt_backend_t` interface, so the socket/protocol code is identical either way.

## Run locally (scripted stub)

```sh
cmake -S . -B build && cmake --build build --target simonsays-server
./build/server/simonsays-server --port 8080 --script "light on" --script "light off"
```

Each `--script` transcript is emitted per VAD-detected utterance, letting you exercise the
full wake → stream → cut loop without any model.

## Run locally (whisper.cpp)

Build whisper.cpp as a shared lib, then configure with the backend enabled:

```sh
git clone --depth 1 https://github.com/ggerganov/whisper.cpp
cmake -S whisper.cpp -B whisper.cpp/build -DBUILD_SHARED_LIBS=ON \
      -DWHISPER_BUILD_EXAMPLES=OFF -DWHISPER_BUILD_TESTS=OFF
cmake --build whisper.cpp/build -j && cmake --install whisper.cpp/build --prefix /usr/local

cmake -S . -B build -DSIMONSAYS_WITH_WHISPER=ON -DWHISPER_ROOT=/usr/local
cmake --build build --target simonsays-server

# Fetch a quantized model, e.g. base.en:
#   bash whisper.cpp/models/download-ggml-model.sh base.en
./build/server/simonsays-server --port 8080 --whisper-model models/ggml-base.en.bin
```

If the server is built without `SIMONSAYS_WITH_WHISPER`, `--whisper-model` prints a warning
and falls back to the scripted stub.

## Container (Podman)

`deploy/Containerfile` is a multi-stage build (compile → slim runtime, non-root user).
Run from the repo root:

```sh
# Scripted-stub image (small, no ML deps):
podman build -f deploy/Containerfile -t localhost/simonsays-server:latest .
podman run --rm -p 8080:8080 localhost/simonsays-server:latest --script "light on"

# whisper image (fetches + builds whisper.cpp in the build stage):
podman build -f deploy/Containerfile --build-arg WITH_WHISPER=1 \
    -t localhost/simonsays-server:whisper .
podman run --rm -p 8080:8080 -v "$PWD/models:/models:ro,Z" \
    localhost/simonsays-server:whisper --whisper-model /models/ggml-base.en.bin
```

The whisper **model file is kept out of the image** (mount it as a volume) so the image
stays small and the model can be swapped without a rebuild.

## Pod (`podman play kube`)

`deploy/simonsays-pod.yaml` runs the server as a pod, publishing port 8080 to the host so
the ESP32-S3 can stream to it:

```sh
podman build -f deploy/Containerfile -t localhost/simonsays-server:latest .
podman play kube deploy/simonsays-pod.yaml
podman play kube --down deploy/simonsays-pod.yaml     # tear down
```

Edit the container `args` (and uncomment the `models` volume) in the manifest to switch to
the whisper image.
