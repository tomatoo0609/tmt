import ctypes, sys
z = ctypes.CDLL('libzstd.so.1')
z.ZSTD_getFrameContentSize.restype = ctypes.c_ulonglong
z.ZSTD_decompress.restype = ctypes.c_size_t
z.ZSTD_isError.restype = ctypes.c_uint
z.ZSTD_getErrorName.restype = ctypes.c_char_p
src = open(sys.argv[1], 'rb').read()
n = z.ZSTD_getFrameContentSize(src, len(src))
print('content size', n)
if n >= (1 << 63): n = len(src) * 20
dst = ctypes.create_string_buffer(n)
r = z.ZSTD_decompress(dst, n, src, len(src))
if z.ZSTD_isError(r): print('error', z.ZSTD_getErrorName(r)); sys.exit(1)
open(sys.argv[2], 'wb').write(dst.raw[:r]); print('ok', r)
