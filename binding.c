#include <assert.h>
#include <bare/module.h>
#include <js.h>
#include <stdbool.h>
#include <stdlib.h>
#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>
#include <webp/mux.h>

// Cap a single frame whatever budget the caller asks for. Every frame is canvas
// sized, so 64 Mpx at 4 bytes per pixel bounds one frame at 256 MiB.
#define BARE_WEBP_MAX_FRAME_PIXELS (1ull << 26)

// Budget for the pixels one decode may produce in total, for callers that
// retain what they are handed. A frame count is cheap to inflate in the file,
// so the per-frame cap alone leaves the total unbounded.
#define BARE_WEBP_DEFAULT_MAX_PIXELS (1ull << 26)

typedef struct {
  WebPAnimDecoder *handle;

  // The decoder reads the image on every call, so it keeps a copy rather than
  // the data of the typed array, which may move or be detached between calls.
  WebPData image;

  uint64_t pixels;
  int64_t max_pixels;
} bare_webp_decoder_t;

static void
bare_webp__on_finalize_image(js_env_t *env, void *data, void *finalize_hint) {
  WebPFree(data);
}

// Checked against the header before libwebp allocates the canvas, which it does
// as soon as a decoder is created.
static bool
bare_webp__check_dimensions(js_env_t *env, const uint8_t *webp, size_t len) {
  int err;

  int width, height;

  if (!WebPGetInfo(webp, len, &width, &height)) {
    err = js_throw_error(env, NULL, "Invalid image");
    assert(err == 0);

    return false;
  }

  if ((uint64_t) width * height > BARE_WEBP_MAX_FRAME_PIXELS) {
    err = js_throw_error(env, NULL, "WebP dimensions exceed maximum");
    assert(err == 0);

    return false;
  }

  return true;
}

static js_value_t *
bare_webp_decode(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 1);

  uint8_t *webp;
  size_t len;
  err = js_get_typedarray_info(env, argv[0], NULL, (void **) &webp, &len, NULL, NULL);
  assert(err == 0);

  if (!bare_webp__check_dimensions(env, webp, len)) return NULL;

  js_value_t *result;
  err = js_create_object(env, &result);
  assert(err == 0);

  int width, height;

  uint8_t *data = WebPDecodeRGBA(webp, len, &width, &height);

  if (data == NULL) {
    err = js_throw_error(env, NULL, "Invalid image");
    assert(err == 0);

    return NULL;
  }

#define V(n) \
  { \
    js_value_t *val; \
    err = js_create_int64(env, n, &val); \
    assert(err == 0); \
    err = js_set_named_property(env, result, #n, val); \
    assert(err == 0); \
  }

  V(width);
  V(height);
#undef V

  len = (size_t) width * height * 4;

  js_value_t *buffer;
  err = js_create_external_arraybuffer(env, data, len, bare_webp__on_finalize_image, NULL, &buffer);
  assert(err == 0);

  err = js_set_named_property(env, result, "data", buffer);
  assert(err == 0);

  return result;
}

static js_value_t *
bare_webp_encode(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 4;
  js_value_t *argv[4];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 4);

  uint8_t *data;
  err = js_get_typedarray_info(env, argv[0], NULL, (void **) &data, NULL, NULL, NULL);
  assert(err == 0);

  int64_t width;
  err = js_get_value_int64(env, argv[1], &width);
  assert(err == 0);

  int64_t height;
  err = js_get_value_int64(env, argv[2], &height);
  assert(err == 0);

  double quality;
  err = js_get_value_double(env, argv[3], &quality);
  assert(err == 0);

  uint8_t *webp;

  size_t len = WebPEncodeRGBA(data, width, height, width * 4, quality, &webp);

  if (len == 0) {
    err = js_throw_error(env, NULL, "Invalid image");
    assert(err == 0);

    return NULL;
  }

  js_value_t *result;
  err = js_create_external_arraybuffer(env, webp, len, bare_webp__on_finalize_image, NULL, &result);
  assert(err == 0);

  return result;
}

static void
bare_webp__on_finalize_decoder(js_env_t *env, void *data, void *finalize_hint) {
  bare_webp_decoder_t *decoder = (bare_webp_decoder_t *) data;

  WebPAnimDecoderDelete(decoder->handle);

  WebPDataClear(&decoder->image);

  free(decoder);
}

static js_value_t *
bare_webp_animated_decoder_init(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 2;
  js_value_t *argv[2];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 2);

  uint8_t *webp;
  size_t len;
  err = js_get_typedarray_info(env, argv[0], NULL, (void **) &webp, &len, NULL, NULL);
  assert(err == 0);

  int64_t max_pixels = 0;
  err = js_get_value_int64(env, argv[1], &max_pixels);
  assert(err == 0);

  if (!bare_webp__check_dimensions(env, webp, len)) return NULL;

  bare_webp_decoder_t *decoder = malloc(sizeof(bare_webp_decoder_t));
  assert(decoder != NULL);

  WebPDataInit(&decoder->image);

  if (!WebPDataCopy(&(WebPData){webp, len}, &decoder->image)) {
    free(decoder);

    err = js_throw_error(env, NULL, "Out of memory");
    assert(err == 0);

    return NULL;
  }

  WebPAnimDecoderOptions options;
  WebPAnimDecoderOptionsInit(&options);

  options.color_mode = MODE_RGBA;

  WebPAnimDecoder *handle = WebPAnimDecoderNew(&decoder->image, &options);

  if (handle == NULL) {
    WebPDataClear(&decoder->image);

    free(decoder);

    err = js_throw_error(env, NULL, "Invalid image");
    assert(err == 0);

    return NULL;
  }

  decoder->handle = handle;
  decoder->pixels = 0;
  decoder->max_pixels = max_pixels;

  js_value_t *result;
  err = js_create_external(env, (void *) decoder, bare_webp__on_finalize_decoder, NULL, &result);
  assert(err == 0);

  return result;
}

static js_value_t *
bare_webp_animated_decoder_get_info(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 1);

  bare_webp_decoder_t *decoder;
  err = js_get_value_external(env, argv[0], (void **) &decoder);
  assert(err == 0);

  WebPAnimInfo animation;
  err = WebPAnimDecoderGetInfo(decoder->handle, &animation);

  if (err != 1) {
    err = js_throw_error(env, NULL, "Invalid image");
    assert(err == 0);

    return NULL;
  }

  uint32_t width = animation.canvas_width;
  uint32_t height = animation.canvas_height;
  uint32_t loops = animation.loop_count;

  if ((uint64_t) width * height > BARE_WEBP_MAX_FRAME_PIXELS) {
    err = js_throw_error(env, NULL, "WebP dimensions exceed maximum");
    assert(err == 0);

    return NULL;
  }

  js_value_t *result;
  err = js_create_object(env, &result);
  assert(err == 0);

#define V(n) \
  { \
    js_value_t *val; \
    err = js_create_uint32(env, n, &val); \
    assert(err == 0); \
    err = js_set_named_property(env, result, #n, val); \
    assert(err == 0); \
  }

  V(width);
  V(height);
  V(loops);
#undef V

  return result;
}

static js_value_t *
bare_webp_animated_decoder_get_next_frame(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 2;
  js_value_t *argv[2];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 2);

  bare_webp_decoder_t *decoder;
  err = js_get_value_external(env, argv[0], (void **) &decoder);
  assert(err == 0);

  js_value_t *result;

  if (WebPAnimDecoderHasMoreFrames(decoder->handle)) {
    err = js_create_object(env, &result);
    assert(err == 0);

    uint8_t *data;
    int timestamp;
    err = WebPAnimDecoderGetNext(decoder->handle, &data, &timestamp);

    if (err != 1) {
      err = js_throw_error(env, NULL, "Invalid image");
      assert(err == 0);

      return NULL;
    }

    WebPAnimInfo animation;
    err = WebPAnimDecoderGetInfo(decoder->handle, &animation);

    if (err != 1) {
      err = js_throw_error(env, NULL, "Invalid image");
      assert(err == 0);

      return NULL;
    }

    uint32_t width = animation.canvas_width;
    uint32_t height = animation.canvas_height;

    if ((uint64_t) width * height > BARE_WEBP_MAX_FRAME_PIXELS) {
      err = js_throw_error(env, NULL, "WebP dimensions exceed maximum");
      assert(err == 0);

      return NULL;
    }

    decoder->pixels += (uint64_t) width * height;

    if (decoder->max_pixels > 0 && decoder->pixels > (uint64_t) decoder->max_pixels) {
      err = js_throw_error(env, NULL, "WebP exceeds maximum decoded size");
      assert(err == 0);

      return NULL;
    }

    size_t len = (size_t) width * height * 4;

    js_value_t *buffer;

    uint8_t *view;
    err = js_create_unsafe_arraybuffer(env, len, (void **) &view, &buffer);

    if (err < 0) return NULL;

    memcpy(view, data, len);

    err = js_set_named_property(env, result, "data", buffer);
    assert(err == 0);

    js_value_t *value;
    err = js_create_int64(env, timestamp, &value);
    assert(err == 0);

    err = js_set_named_property(env, result, "timestamp", value);
    assert(err == 0);
  } else {
    err = js_get_null(env, &result);
    assert(err == 0);
  }

  return result;
}

static void
bare_webp__on_finalize_encoder(js_env_t *env, void *data, void *finalize_hint) {
  WebPAnimEncoderDelete(data);
}

static js_value_t *
bare_webp_animated_encoder_init(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 2;
  js_value_t *argv[2];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 2);

  int64_t width;
  err = js_get_value_int64(env, argv[0], &width);
  assert(err == 0);

  int64_t height;
  err = js_get_value_int64(env, argv[1], &height);
  assert(err == 0);

  WebPAnimEncoderOptions options;
  WebPAnimEncoderOptionsInit(&options);

  WebPAnimEncoder *encoder = WebPAnimEncoderNew(width, height, &options);

  if (encoder == NULL) {
    err = js_throw_error(env, NULL, "Invalid image");
    assert(err == 0);

    return NULL;
  }

  js_value_t *result;
  err = js_create_external(env, encoder, bare_webp__on_finalize_encoder, NULL, &result);
  assert(err == 0);

  return result;
}

static js_value_t *
bare_webp_animated_encoder_add_frame(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 6;
  js_value_t *argv[6];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 6);

  WebPAnimEncoder *encoder;
  err = js_get_value_external(env, argv[0], (void **) &encoder);
  assert(err == 0);

  uint8_t *webp;
  err = js_get_typedarray_info(env, argv[1], NULL, (void **) &webp, NULL, NULL, NULL);
  assert(err == 0);

  int64_t width;
  err = js_get_value_int64(env, argv[2], &width);
  assert(err == 0);

  int64_t height;
  err = js_get_value_int64(env, argv[3], &height);
  assert(err == 0);

  double quality;
  err = js_get_value_double(env, argv[4], &quality);
  assert(err == 0);

  int64_t timestamp;
  err = js_get_value_int64(env, argv[5], &timestamp);
  assert(err == 0);

  WebPConfig config;
  WebPConfigInit(&config);

  config.quality = quality;

  WebPPicture frame;
  WebPPictureInit(&frame);

  frame.width = width;
  frame.height = height;

  err = WebPPictureImportRGBA(&frame, webp, width * 4);

  if (err != 1) {
    err = js_throw_error(env, NULL, "Invalid frame");
    assert(err == 0);

    return NULL;
  }

  err = WebPAnimEncoderAdd(encoder, &frame, timestamp, &config);

  WebPPictureFree(&frame);

  if (err != 1) {
    err = js_throw_error(env, NULL, "Invalid frame");
    assert(err == 0);

    return NULL;
  }

  return NULL;
}

static void
bare_webp__on_finalize_data(js_env_t *env, void *data, void *finalize_hint) {
  WebPFree(data);
}

static js_value_t *
bare_webp_animated_encoder_assemble(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 1);

  WebPAnimEncoder *encoder;
  err = js_get_value_external(env, argv[0], (void **) &encoder);
  assert(err == 0);

  WebPData data;
  WebPDataInit(&data);

  err = WebPAnimEncoderAssemble(encoder, &data);

  if (err != 1) {
    err = js_throw_error(env, NULL, "Failed to assemble animation");
    assert(err == 0);

    return NULL;
  }

  js_value_t *buffer;
  err = js_create_external_arraybuffer(env, (void *) data.bytes, data.size, bare_webp__on_finalize_data, NULL, &buffer);
  assert(err == 0);

  return buffer;
}

static js_value_t *
bare_webp_exports(js_env_t *env, js_value_t *exports) {
  int err;

#define V(name, fn) \
  { \
    js_value_t *val; \
    err = js_create_function(env, name, -1, fn, NULL, &val); \
    assert(err == 0); \
    err = js_set_named_property(env, exports, name, val); \
    assert(err == 0); \
  }

  V("decode", bare_webp_decode)
  V("encode", bare_webp_encode)

  V("animatedDecoderInit", bare_webp_animated_decoder_init)
  V("animatedDecoderGetInfo", bare_webp_animated_decoder_get_info)
  V("animatedDecoderGetNextFrame", bare_webp_animated_decoder_get_next_frame)

  V("animatedEncoderInit", bare_webp_animated_encoder_init)
  V("animatedEncoderAddFrame", bare_webp_animated_encoder_add_frame)
  V("animatedEncoderAssemble", bare_webp_animated_encoder_assemble)
#undef V

  js_value_t *max_pixels;
  err = js_create_int64(env, BARE_WEBP_DEFAULT_MAX_PIXELS, &max_pixels);
  assert(err == 0);

  err = js_set_named_property(env, exports, "defaultMaxPixels", max_pixels);
  assert(err == 0);

  return exports;
}

BARE_MODULE(bare_webp, bare_webp_exports)
