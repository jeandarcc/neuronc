#include "graphics/assets/graphics_asset_internal.h"
#include "graphics/graphics_core_internal.h"
#include "graphics/platform/graphics_platform_internal.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define COBJMACROS
#include <objbase.h>
#include <wincodec.h>
#include <windows.h>
#endif

#include <stdlib.h>

int neuron_graphics_load_png_wic(const char *path, uint8_t **out_pixels,
                                 uint32_t *out_width,
                                 uint32_t *out_height) {
#if defined(_WIN32)
  if (path == NULL || out_pixels == NULL || out_width == NULL ||
      out_height == NULL) {
    neuron_graphics_set_error("Invalid PNG decode arguments");
    return 0;
  }

  *out_pixels = NULL;
  *out_width = 0;
  *out_height = 0;

  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  int co_initialized = SUCCEEDED(hr);
  if (hr == RPC_E_CHANGED_MODE) {
    hr = S_OK;
    co_initialized = 0;
  }
  if (FAILED(hr)) {
    neuron_graphics_set_error("CoInitializeEx failed (0x%08lx)",
                              (unsigned long)hr);
    return 0;
  }

  wchar_t *wide_path = neuron_graphics_utf8_to_wide(path);
  if (wide_path == NULL) {
    neuron_graphics_set_error("Failed to convert UTF-8 texture path to UTF-16");
    if (co_initialized) {
      CoUninitialize();
    }
    return 0;
  }

  IWICImagingFactory *factory = NULL;
  IWICBitmapDecoder *decoder = NULL;
  IWICBitmapFrameDecode *frame = NULL;
  IWICFormatConverter *converter = NULL;
  uint8_t *pixels = NULL;

  hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                        &IID_IWICImagingFactory, (void **)&factory);
  if (FAILED(hr)) {
    neuron_graphics_set_error(
        "CoCreateInstance(IWICImagingFactory) failed (0x%08lx)",
        (unsigned long)hr);
    goto cleanup;
  }

  hr = IWICImagingFactory_CreateDecoderFromFilename(
      factory, wide_path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
      &decoder);
  if (FAILED(hr)) {
    neuron_graphics_set_error("WIC decoder open failed for '%s' (0x%08lx)",
                              path, (unsigned long)hr);
    goto cleanup;
  }

  hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
  if (FAILED(hr)) {
    neuron_graphics_set_error("WIC GetFrame failed (0x%08lx)",
                              (unsigned long)hr);
    goto cleanup;
  }

  hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
  if (FAILED(hr)) {
    neuron_graphics_set_error("WIC CreateFormatConverter failed (0x%08lx)",
                              (unsigned long)hr);
    goto cleanup;
  }

  hr = IWICFormatConverter_Initialize(
      converter, (IWICBitmapSource *)frame, &GUID_WICPixelFormat32bppRGBA,
      WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
  if (FAILED(hr)) {
    neuron_graphics_set_error("WIC converter initialize failed (0x%08lx)",
                              (unsigned long)hr);
    goto cleanup;
  }

  UINT width = 0;
  UINT height = 0;
  hr = IWICBitmapFrameDecode_GetSize(frame, &width, &height);
  if (FAILED(hr) || width == 0 || height == 0) {
    neuron_graphics_set_error("WIC frame size query failed");
    goto cleanup;
  }

  const size_t stride = (size_t)width * 4u;
  const size_t byte_count = stride * (size_t)height;
  pixels = (uint8_t *)malloc(byte_count);
  if (pixels == NULL) {
    neuron_graphics_set_error("Out of memory decoding PNG '%s'", path);
    goto cleanup;
  }

  hr = IWICFormatConverter_CopyPixels(converter, NULL, (UINT)stride,
                                      (UINT)byte_count, pixels);
  if (FAILED(hr)) {
    neuron_graphics_set_error("WIC CopyPixels failed (0x%08lx)",
                              (unsigned long)hr);
    goto cleanup;
  }

  *out_pixels = pixels;
  *out_width = (uint32_t)width;
  *out_height = (uint32_t)height;
  pixels = NULL;

cleanup:
  if (pixels != NULL) {
    free(pixels);
  }
  if (converter != NULL) {
    IWICFormatConverter_Release(converter);
  }
  if (frame != NULL) {
    IWICBitmapFrameDecode_Release(frame);
  }
  if (decoder != NULL) {
    IWICBitmapDecoder_Release(decoder);
  }
  if (factory != NULL) {
    IWICImagingFactory_Release(factory);
  }
  free(wide_path);
  if (co_initialized) {
    CoUninitialize();
  }

  return *out_pixels != NULL;
#else
  (void)path;
  (void)out_pixels;
  (void)out_width;
  (void)out_height;
  return 0;
#endif
}
