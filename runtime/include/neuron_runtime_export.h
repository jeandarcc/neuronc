#ifndef NEURON_RUNTIME_EXPORT_H
#define NEURON_RUNTIME_EXPORT_H

#if defined(_WIN32)
#if defined(NEURON_RUNTIME_BUILD_SHARED)
#define NEURON_RUNTIME_API __declspec(dllexport)
#elif defined(NEURON_RUNTIME_USE_SHARED)
#define NEURON_RUNTIME_API __declspec(dllimport)
#else
#define NEURON_RUNTIME_API
#endif
#elif defined(__GNUC__) && defined(NEURON_RUNTIME_BUILD_SHARED)
#define NEURON_RUNTIME_API __attribute__((visibility("default")))
#else
#define NEURON_RUNTIME_API
#endif

#endif // NEURON_RUNTIME_EXPORT_H
