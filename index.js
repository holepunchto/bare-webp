const binding = require('./binding')

exports.decode = function decode(image) {
  const { width, height, data } = binding.decode(image)

  return {
    width,
    height,
    data: Buffer.from(data)
  }
}

exports.decodeAnimated = function decodeAnimated(image) {
  const decoder = binding.animatedDecoderInit(image)

  const { width, height, loops } = binding.animatedDecoderGetInfo(decoder)

  const frames = {
    next() {
      const frame = binding.animatedDecoderGetNextFrame(
        decoder,
        image // Keep a reference for lifetime management
      )

      if (frame === null) {
        return {
          done: true
        }
      }

      const { data, timestamp } = frame

      return {
        done: false,
        value: {
          width,
          height,
          timestamp,
          data: Buffer.from(data)
        }
      }
    },

    [Symbol.iterator]() {
      return frames
    }
  }

  return {
    width,
    height,
    loops,
    frames
  }
}

exports.encode = function encode(image, opts = {}) {
  const { quality = 90 } = opts

  const buffer = binding.encode(
    image.data,
    image.width,
    image.height,
    clamp(quality, 0, 100)
  )

  return Buffer.from(buffer)
}

exports.encodeAnimated = function encodeAnimated(image) {
  const encoder = binding.animatedEncoderInit(image.width, image.height)

  for (const { data, timestamp } of image.frames) {
    binding.animatedEncoderAddFrame(
      encoder,
      data,
      image.width,
      image.height,
      timestamp
    )
  }

  return Buffer.from(binding.animatedEncoderAssemble(encoder))
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value))
}
