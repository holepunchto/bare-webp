#include <assert.h>
#include <bare.h>
#include <js.h>
#include <stdlib.h>
#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>

static void
bare_webp__on_finalize_image(js_env_t *env, void *data, void *finalize_hint) {
  WebPFree(data);
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

  len = width * height * 4;

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
  WebPAnimDecoderDelete(data);
}

static js_value_t *
bare_webp_animated_decoder_init(js_env_t *env, js_callback_info_t *info) {
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

  WebPData data = {webp, len};

  WebPAnimDecoderOptions options = {MODE_RGBA};

  WebPAnimDecoder *decoder = WebPAnimDecoderNew(&data, &options);

  WebPAnimInfo animation;
  err = WebPAnimDecoderGetInfo(decoder, &animation);
  assert(err == 1);

  uint32_t width = animation.canvas_width;
  uint32_t height = animation.canvas_height;
  uint32_t loops = animation.loop_count;

  js_value_t *result;
  err = js_create_object(env, &result);
  assert(err == 0);

  js_value_t *handle;
  err = js_create_external(env, (void *) decoder, bare_webp__on_finalize_decoder, NULL, &handle);
  assert(err == 0);

  err = js_set_named_property(env, result, "decoder", handle);
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

  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 1);

  WebPAnimDecoder *decoder;
  err = js_get_value_external(env, argv[0], (void **) &decoder);
  assert(err == 0);

  js_value_t *result;

  if (WebPAnimDecoderHasMoreFrames(decoder)) {
    err = js_create_object(env, &result);
    assert(err == 0);

    uint8_t *data;
    int timestamp;
    err = WebPAnimDecoderGetNext(decoder, &data, &timestamp);
    assert(err == 1);

    WebPAnimInfo animation;
    err = WebPAnimDecoderGetInfo(decoder, &animation);
    assert(err == 1);

    uint32_t width = animation.canvas_width;
    uint32_t height = animation.canvas_height;

    size_t len = width * height * 4;

    js_value_t *buffer;

    uint8_t *view;
    err = js_create_unsafe_arraybuffer(env, len, (void **) &view, &buffer);

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

  V("initAnimatedDecoder", bare_webp_animated_decoder_init)
  V("getNextAnimatedDecoderFrame", bare_webp_animated_decoder_get_next_frame)
#undef V

  return exports;
}

BARE_MODULE(bare_webp, bare_webp_exports)
