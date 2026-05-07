"""TLS-PSK support for connecting to Synchronet's broker.js.

Python 3.13+ has native ssl.SSLContext.set_psk_client_callback().
On older versions, we fall back to injecting the callback via ctypes
into libssl — fragile and CPython/Unix-specific, but functional.
"""

import ssl
import sys


def make_psk_context(identity, psk):
    """Create an ssl.SSLContext configured for TLS-PSK client auth.

    Args:
        identity: PSK identity string (sysop alias, lowercased).
        psk: PSK as a string (sysop password, lowercased).

    Returns:
        ssl.SSLContext ready for use with paho-mqtt's tls_set_context().
    """
    id_bytes = identity.encode("utf-8") if isinstance(identity, str) else identity
    psk_bytes = psk.encode("utf-8") if isinstance(psk, str) else psk

    if hasattr(ssl.SSLContext, "set_psk_client_callback"):
        return _make_psk_context_native(id_bytes, psk_bytes)
    return _make_psk_context_ctypes(id_bytes, psk_bytes)


def _make_psk_context_native(id_bytes, psk_bytes):
    """Python 3.13+ native PSK support."""
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    ctx.maximum_version = ssl.TLSVersion.TLSv1_2
    ctx.set_ciphers("PSK:@SECLEVEL=0")
    ctx.set_psk_client_callback(lambda hint: (id_bytes.decode(), psk_bytes))
    return ctx


def _make_psk_context_ctypes(id_bytes, psk_bytes):
    """Fallback for Python < 3.13 via ctypes into libssl.

    Fragile — depends on CPython struct layout and Unix ldd.
    """
    import ctypes
    import ctypes.util
    import struct
    import subprocess

    # Find the libssl that Python's ssl module actually links against,
    # not whatever ctypes.util.find_library returns (may be a different version).
    def find_python_libssl():
        import _ssl
        try:
            result = subprocess.run(
                ["ldd", _ssl.__file__], capture_output=True, text=True
            )
            for line in result.stdout.splitlines():
                if "libssl" in line and "=>" in line:
                    return line.split("=>")[1].strip().split()[0]
        except Exception:
            pass
        raise ImportError(
            "Cannot find libssl matching Python's ssl module. "
            "Upgrade to Python 3.13+ for native TLS-PSK support."
        )

    libssl = ctypes.CDLL(find_python_libssl())

    PSK_CLIENT_CB = ctypes.CFUNCTYPE(
        ctypes.c_uint,                    # return: psk length
        ctypes.c_void_p,                  # SSL *
        ctypes.c_char_p,                  # hint (from server, may be NULL)
        ctypes.POINTER(ctypes.c_char),    # identity (OUT buffer)
        ctypes.c_uint,                    # max_identity_len
        ctypes.POINTER(ctypes.c_ubyte),   # psk (OUT buffer)
        ctypes.c_uint,                    # max_psk_len
    )

    libssl.SSL_CTX_set_psk_client_callback.argtypes = [ctypes.c_void_p, PSK_CLIENT_CB]
    libssl.SSL_CTX_set_psk_client_callback.restype = None
    libssl.SSL_CTX_set_cipher_list.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    libssl.SSL_CTX_set_cipher_list.restype = ctypes.c_int

    ptr_size = struct.calcsize("P")

    def get_ssl_ctx_ptr(ctx):
        # CPython PySSLContext: PyObject_HEAD (2 ptrs) + SSL_CTX*
        return ctypes.c_void_p.from_address(id(ctx) + 2 * ptr_size).value

    def psk_callback(ssl_ptr, hint, id_buf, max_id_len, psk_buf, max_psk_len):
        n = min(len(id_bytes), max_id_len - 1)
        for i in range(n):
            id_buf[i] = id_bytes[i:i+1]
        id_buf[n] = b'\0'
        m = min(len(psk_bytes), max_psk_len)
        for i in range(m):
            psk_buf[i] = psk_bytes[i]
        return m

    cb = PSK_CLIENT_CB(psk_callback)

    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS)
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    ctx.minimum_version = ssl.TLSVersion.TLSv1_2
    ctx.maximum_version = ssl.TLSVersion.TLSv1_2
    ctx.options |= ssl.OP_NO_TICKET

    ctx_ptr = get_ssl_ctx_ptr(ctx)
    libssl.SSL_CTX_set_psk_client_callback(ctx_ptr, cb)
    rc = libssl.SSL_CTX_set_cipher_list(ctx_ptr, b"DHE-PSK-AES128-CBC-SHA256:PSK:@SECLEVEL=0")
    if rc != 1:
        raise ssl.SSLError("Failed to set PSK cipher list")

    # prevent GC of callback
    ctx._psk_cb = cb

    return ctx
