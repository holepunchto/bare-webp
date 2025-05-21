const test = require('brittle')
const png = require('.')

test('decode .webp', (t) => {
  const image = require('./test/fixtures/grapefruit.webp', {
    with: { type: 'binary' }
  })

  t.comment(png.decode(image))
})

test('encode .webp', (t) => {
  const image = require('./test/fixtures/grapefruit.webp', {
    with: { type: 'binary' }
  })

  const decoded = png.decode(image)

  t.comment(png.encode(decoded))
})
