# bare-webp

WebP support for Bare.

```
npm i bare-webp
```

## Usage

```js
const webp = require('bare-webp')

const image = require('./my-image.webp', { with: { type: 'binary' } })

const decoded = webp.decode(image)
// {
//   width: 200,
//   height: 400,
//   data: <Buffer>
// }

const encoded = webp.encode(decoded)
// <Buffer>
```

## License

Apache-2.0
