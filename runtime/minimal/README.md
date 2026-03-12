# Minimal Runtime SDK

`neuron build-nucleus` reads [`sources.manifest`](./sources.manifest) and
compiles a single-file `nucleus` runtime that executes `.ncon` containers.

Manifest entries are repo-relative source paths resolved against the Neuron tool
root (`<install>/` or repository root).
