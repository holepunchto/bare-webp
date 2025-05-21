const binding = require('./binding')

exports.decode = function decode(image) {
  const { width, height, data } = binding.decode(image)

  return {
    width,
    height,
    data: Buffer.from(data)
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

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value))
}
