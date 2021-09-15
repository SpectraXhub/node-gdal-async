import * as gdal from '..'
import { assert } from 'chai'
import * as path from 'path'

if (process.env.GDAL_DATA !== undefined) {
  throw new Error(
    'Sorry, this test requires that the GDAL_DATA environment option is not set'
  )
}

describe('gdal', () => {
  afterEach(global.gc)

  describe('"lastError" property', () => {
    describe('get()', () => {
      it('should return null when no previous error', () => {
        // note: this needs to be the first test run
        assert.isNull(gdal.lastError)
      })
      it('should return an object normally', () => {
        // This is not a public API
        /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
        (gdal as any)._triggerCPLError()

        assert.deepEqual(gdal.lastError, {
          code: gdal.CPLE_AppDefined,
          level: gdal.CE_Failure,
          message: 'Mock error'
        })
      })
    })
    describe('set()', () => {
      it('should allow reset by setting to null', () => {
        /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
        (gdal as any)._triggerCPLError()

        assert.equal(!!gdal.lastError, true);
        /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
        (gdal as any).lastError = null
        assert.isNull(gdal.lastError)
      })
      it('should throw when not null', () => {
        assert.throws(() => {
        /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
          (gdal as any).lastError = {}
        }, /null/)
      })
    })
  })
  describe('"version" property', () => {
    it('should exist', () => {
      assert.match(gdal.version, /^\d+\.\d+\.\d+[a-zA-Z]*$/)
    })
  })
  describe('"config" property', () => {
    describe('get()', () => {
      it('should not throw', () => {
        gdal.config.get('CPL_LOG')
      })
    })
    describe('set()', () => {
      it('should set option', () => {
        gdal.config.set('CPL_DEBUG', 'ON')
        assert.equal(gdal.config.get('CPL_DEBUG'), 'ON')
        gdal.config.set('CPL_DEBUG', null)
        assert.isNull(gdal.config.get('CPL_DEBUG'))
      })
    })
    describe('GDAL_DATA behavior', () => {
      const data_path = path.resolve(__dirname, '../deps/libgdal/gdal/data')
      if (gdal.bundled) {
        it('should set GDAL_DATA config option to locally bundled path', () => {
          assert.equal(gdal.config.get('GDAL_DATA'), data_path)
        })
      }
      it('should respect GDAL_DATA environment over locally bundled path', (done) => {
        const env = Object.assign({}, process.env)
        env.GDAL_DATA = 'bogus'
        // eslint-disable-next-line @typescript-eslint/no-var-requires
        const cp = require('child_process')
        // The manual delete/global.gc() allows for error-free unit testing of the ASAN build
        const command =
          "\"const gdal = require('./lib/gdal.js'); console.log(gdal.config.get('GDAL_DATA')); delete gdal.drivers; global.gc();\""
        let execPath = process.execPath
        if (process.platform === 'win32') {
          // quotes to avoid errors like ''C:\Program' is not recognized as an internal or external command'
          execPath = `"${execPath}"`
        }
        cp.exec(
          `${execPath} ${[ '--expose_gc', '-e', command ].join(' ')}`,
          { env },
          (err, stdout) => {
            if (err) throw err
            assert.equal(env.GDAL_DATA, stdout.trim())
            done()
          }
        )
      })
    })
  })
  describe('decToDMS()', () => {
    it('should throw when axis not provided', () => {
      assert.throws(() => {
        /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
        (gdal as any).decToDMS(12.2)
      })
    })
    it('should return correct result', () => {
      assert.equal(gdal.decToDMS(14.12511, 'lat', 2), " 14d 7'30.40\"N")
      assert.equal(gdal.decToDMS(14.12511, 'lat', 1), " 14d 7'30.4\"N")
      assert.equal(gdal.decToDMS(14.12511, 'long', 2), " 14d 7'30.40\"E")
      assert.equal(gdal.decToDMS(14.12511, 'long', 1), " 14d 7'30.4\"E")
    })
  })
  describe('Node.js Async callback error convention', () => {
    it('should return null for error on success', (done) => {
      gdal.openAsync(`${__dirname}/data/sample.tif`, (error, result) => {
        assert.isNull(error)
        assert.instanceOf(result, gdal.Dataset)
        done()
      })
    })
    it('should return an Error object and an undefined result on error', (done) => {
      gdal.openAsync('notfound', (error, result) => {
        assert.instanceOf(error, Error)
        assert.isUndefined(result)
        done()
      })
    })
  })

  it('should handle exceptions in progress callbacks', () => {
    const driver = gdal.drivers.get('MEM')
    const outputFilename = ''
    assert.throws(() => {
      driver.createCopy(
        outputFilename,
        gdal.open(`${__dirname}/data/12_791_1476.jpg`),
        {},
        false,
        (): void => {
          throw new Error('progress callback error')
        })
    }, /sync progress callback exception/)
  })
})
