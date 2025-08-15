const test = require('brittle')
const webp = require('.')

test('decode .webp', (t) => {
  const image = require('./test/fixtures/grapefruit.webp', {
    with: { type: 'binary' }
  })

  t.comment(webp.decode(image))
})

test('decode animated .webp', (t) => {
  const image = require('./test/fixtures/nyan.webp', {
    with: { type: 'binary' }
  })

  const decoded = webp.decodeAnimated(image)

  for (const frame of decoded.frames) {
    t.comment(frame)
  }
})

test('decode animated .webp, non-animated', (t) => {
  const image = require('./test/fixtures/grapefruit.webp', {
    with: { type: 'binary' }
  })

  const decoded = webp.decodeAnimated(image)

  for (const frame of decoded.frames) {
    t.comment(frame)
  }
})

test('encode .webp', (t) => {
  const image = require('./test/fixtures/grapefruit.webp', {
    with: { type: 'binary' }
  })

  const decoded = webp.decode(image)

  t.comment(webp.encode(decoded))
})

test('encode animated .webp', (t) => {
  const image = require('./test/fixtures/nyan.webp', {
    with: { type: 'binary' }
  })

  const decoded = webp.decodeAnimated(image)

  t.comment(webp.encodeAnimated(decoded))
})
