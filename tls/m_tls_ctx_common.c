/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "m_config.h"
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_tls.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#ifdef __APPLE__
#  include <Security/Security.h>
#endif
#ifdef _WIN32
#  include <wincrypt.h>
#endif
#include "base/m_defs_int.h"
#include "m_tls_ctx_common.h"

/* NOTE: Client still includes RC4 for now due to many servers only supporting RC4 due to
 *       lazy sysadmins that had heard of BEAST many years ago, otherwise enablement of
 *       RC4 should be the only difference. */
#define TLS_CLIENT_CIPHERS "EECDH+ECDSA+AESGCM:EECDH+aRSA+AESGCM:EECDH+ECDSA+SHA384:EECDH+ECDSA+SHA256:EECDH+aRSA+SHA384:EECDH+aRSA+SHA256:EECDH+aRSA+RC4:EECDH:EDH+aRSA:AES256-GCM-SHA384:AES256-SHA256:AES256-SHA:AES128-SHA:RC4-SHA:!aNULL:!eNULL:!LOW:!3DES:!MD5:!EXP:!PSK:!SRP:!DSS"
#define TLS_SERVER_CIPHERS "EECDH+ECDSA+AESGCM:EECDH+aRSA+AESGCM:EECDH+ECDSA+SHA384:EECDH+ECDSA+SHA256:EECDH+aRSA+SHA384:EECDH+aRSA+SHA256:EECDH:EDH+aRSA:AES256-GCM-SHA384:AES256-SHA256:AES256-SHA:AES128-SHA:!aNULL:!eNULL:!LOW:!3DES:!RC4:!MD5:!EXP:!PSK:!SRP:!DSS"


SSL_CTX *M_tls_ctx_init(M_bool is_server)
{
	SSL_CTX *ctx;
	ctx  = SSL_CTX_new(is_server?SSLv23_server_method():SSLv23_client_method());
	if (ctx == NULL) {
		return NULL;
	}
	

	/* Set some default options */
	M_tls_ctx_set_protocols(ctx, M_TLS_PROTOCOL_DEFAULT);
	M_tls_ctx_set_ciphers(ctx, is_server?TLS_SERVER_CIPHERS:TLS_CLIENT_CIPHERS);

	/* Client-only settings */
	if (!is_server) {
	/* Sometimes ticket support can confuse old servers, disable */
#ifndef OPENSSL_NO_TLSEXT
		SSL_CTX_set_options(ctx, SSL_OP_NO_TICKET);
#endif

		/* Enable TLS_FALLBACK_SCSV as a POODLE mitigation */
		SSL_CTX_set_mode(ctx, SSL_MODE_SEND_FALLBACK_SCSV);
	}

	/* Server-only settings */
	if (is_server) {
#if OPENSSL_VERSION_NUMBER < 0x1000200fL
		EC_KEY    *ecdh;
#endif

		/* SSL_OP_CIPHER_SERVER_PREFERENCE tells the client we prefer the server's certifcate order
		 * SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS is for compatibility with Windows CE (no longer needed)
		 * SSL_OP_SINGLE_DH_USE tells not to reuse DH keys -- better security */
		SSL_CTX_set_options(ctx, SSL_OP_CIPHER_SERVER_PREFERENCE|SSL_OP_SINGLE_DH_USE);

		/* Enable Forward Secrecy via ECDH */
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL && !defined(LIBRESSL_VERSION_NUMBER)
		/* Always enabled in 1.1.0+ */
#elif OPENSSL_VERSION_NUMBER >= 0x1000200fL
		/* Auto is better since it will use the best curve supported by both sides */
		SSL_CTX_set_ecdh_auto(ctx, 1);
#else
		/* Choose a curve most clients should have */
		ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
		if (ecdh == NULL) {
			SSL_CTX_free(ctx);
			return NULL;
		}
		SSL_CTX_set_tmp_ecdh(ctx, ecdh); 
		EC_KEY_free(ecdh); /* Safe because of reference counts */
#endif

		/* Enable session caching */
		SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER); /* SSL_SESS_CACHE_OFF to disable */
	}

	/* Enable non-blocking support properly */
	SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE|SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

	/* XXX: should we enable SSL_OP_ALL to be more compatible? */

	return ctx;
}


char *M_tls_ctx_get_cipherlist(SSL_CTX *ctx)
{
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL && !defined(LIBRESSL_VERSION_NUMBER)
	STACK_OF(SSL_CIPHER) *sk_ciphers = SSL_CTX_get_ciphers(ctx);
#else
	STACK_OF(SSL_CIPHER) *sk_ciphers = ctx->cipher_list;
#endif

	int                   i;
	M_buf_t              *buf        = M_buf_create();

	for (i=0; i<sk_SSL_CIPHER_num(sk_ciphers); i++) {
		const SSL_CIPHER *cipher = sk_SSL_CIPHER_value(sk_ciphers, i);
		if (M_buf_len(buf))
			M_buf_add_byte(buf, ':');
		M_buf_add_str(buf, SSL_CIPHER_get_name(cipher));
	}

	return M_buf_finish_str(buf, NULL);
}


SSL_CTX *M_tls_ctx_duplicate_serverctx(SSL_CTX *orig_ctx, DH *dhparams, STACK_OF(X509) *trustlist, M_list_t *crls)
{
	SSL_CTX *ctx = M_tls_ctx_init(M_TRUE);
	char    *ciphers;
	size_t   i;

	/* Options/protocols */
	if (!SSL_CTX_set_options(ctx, SSL_CTX_get_options(orig_ctx)))
		goto fail;

	/* Mode */
	if (!SSL_CTX_set_mode(ctx, SSL_CTX_get_mode(orig_ctx)))
		goto fail;

	/* Ciphers */
	ciphers = M_tls_ctx_get_cipherlist(orig_ctx);
	if (ciphers == NULL)
		goto fail;

	if (!M_tls_ctx_set_ciphers(ctx, ciphers)) {
		M_free(ciphers);
		goto fail;
	}
	M_free(ciphers);

	/* Session support */
	if (!SSL_CTX_set_session_cache_mode(ctx, SSL_CTX_get_session_cache_mode(orig_ctx)))
		goto fail;

	/* Trust list (x509 store) */
	if (!M_tls_ctx_set_x509trust(ctx, trustlist))
		goto fail;

	/* CRLs */
	for (i=0; i<M_list_len(crls); i++) {
		const void *ptr   = M_list_at(crls, i);
		X509_CRL   *crl   = M_CAST_OFF_CONST(X509_CRL *, ptr);
		X509_STORE *store = SSL_CTX_get_cert_store(ctx);
		X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK|X509_V_FLAG_CRL_CHECK_ALL);
		X509_STORE_add_crl(store, crl);
	}

	/* DH Params */
	if (dhparams) {
		SSL_CTX_set_tmp_dh(ctx, dhparams);
	}

	/* XXX: Anything else of importance? */

	return ctx;
fail:
	M_tls_ctx_destroy(ctx);
	return NULL;
}


void M_tls_ctx_destroy(SSL_CTX *ctx)
{
	SSL_CTX_free(ctx);
}


M_bool M_tls_ctx_set_protocols(SSL_CTX *ctx, int protocols /* M_tls_protocols_t bitmap */)
{
	/* Always disable SSLv2 and SSLv3 */
	unsigned long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;

	/* Set defaults */
	if (protocols == M_TLS_PROTOCOL_DEFAULT)
		protocols = M_TLS_PROTOCOL_TLSv1_0 | M_TLS_PROTOCOL_TLSv1_1 | M_TLS_PROTOCOL_TLSv1_2;

	/* Openssl takes the inverse, so disable protocols that aren't set */
	if (!(protocols & M_TLS_PROTOCOL_TLSv1_0)) {
		options |= SSL_OP_NO_TLSv1;
	}
	if (!(protocols & M_TLS_PROTOCOL_TLSv1_1)) {
		options |= SSL_OP_NO_TLSv1_1;
	}
	if (!(protocols & M_TLS_PROTOCOL_TLSv1_2)) {
		options |= SSL_OP_NO_TLSv1_2;
	}

	if (ctx == NULL)
		return M_FALSE;

#if OPENSSL_VERSION_NUMBER < 0x1010000fL || defined(LIBRESSL_VERSION_NUMBER)
	SSL_CTX_set_options(ctx, (long)options);
#else
	SSL_CTX_set_options(ctx, options);
#endif

	return M_TRUE;
}


M_bool M_tls_ctx_set_ciphers(SSL_CTX *ctx, const char *ciphers)
{
	if (ctx == NULL || M_str_isempty(ciphers))
		return M_FALSE;

	if (SSL_CTX_set_cipher_list(ctx, ciphers) == 0)
		return M_FALSE;

	return M_TRUE;
}


static M_bool M_tls_ctx_set_cert_chain(SSL_CTX *ctx, const unsigned char *data, size_t data_len, M_bool is_intermediate, X509 **x509_out)
{
	BIO                 *bio; 
	STACK_OF(X509_INFO) *sk;
	int                  i;
	size_t               count = 0;
	M_bool               rv    = M_FALSE;

	if (ctx == NULL || data == NULL || data_len == 0)
		return M_FALSE;

	/* Load CAfile into BIO */
	bio = BIO_new_mem_buf(M_CAST_OFF_CONST(void *, data), (int)data_len);

	/* Parse the CAfile into a stack of X509 INFO structures */
	sk  = PEM_X509_INFO_read_bio(bio, NULL, NULL, NULL);
	BIO_free(bio);
	if (sk == NULL)
		return M_FALSE;

	/* Iterate across the stack and pull out the x509 certs and add them to our CTX*/
	for(i = 0; i < sk_X509_INFO_num(sk); i++) {
		X509_INFO *x509info = sk_X509_INFO_value(sk, i);
		if (x509info->x509 != NULL) {
			if (count == 0 && !is_intermediate) {
				if (SSL_CTX_use_certificate(ctx, x509info->x509) != 1) {
					goto fail;
				}
				if (x509_out != NULL) {
					/* NOTE: SSL_CTX_use_certificate() bumps the reference count, so should be safe to return */
					*x509_out = x509info->x509;
				}
			} else {
#if OPENSSL_VERSION_NUMBER >= 0x1000200fL
				if (SSL_CTX_add1_chain_cert(ctx, x509info->x509) != 1) {
					goto fail;
				}
#else
				if (SSL_CTX_add_extra_chain_cert(ctx, x509info->x509) != 1) {
					goto fail;
				}
				/* upref! */
				CRYPTO_add(&x509info->x509->references, 1, CRYPTO_LOCK_X509);
#endif
			}
			count++;
		}
	}

	if (count)
		rv = M_TRUE;

fail:
	/* Free the stack and all members */
	sk_X509_INFO_pop_free(sk, X509_INFO_free);

	return rv;
}


M_bool M_tls_ctx_set_cert(SSL_CTX *ctx, const unsigned char *key, size_t key_len, const unsigned char *crt, size_t crt_len, const unsigned char *intermediate, size_t intermediate_len, X509 **x509_out)
{
	BIO      *bio;
	EVP_PKEY *pkey   = NULL;
	M_bool    retval = M_FALSE;

	if (ctx == NULL || key == NULL || key_len == 0 || crt == NULL || crt_len == 0) {
		return M_FALSE;
	}

	bio  = BIO_new_mem_buf(M_CAST_OFF_CONST(void *, key), (int)key_len);
	pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
	BIO_free(bio);

	if (pkey == NULL)
		goto fail;

	if (SSL_CTX_use_PrivateKey(ctx, pkey) != 1)
		goto fail;

	if (!M_tls_ctx_set_cert_chain(ctx, crt, crt_len, M_FALSE, x509_out))
		goto fail;

	if (intermediate_len && !M_tls_ctx_set_cert_chain(ctx, intermediate, intermediate_len, M_TRUE, x509_out))
		goto fail;

	if (SSL_CTX_check_private_key(ctx) != 1)
		goto fail;

	retval = M_TRUE;

fail:
	if (pkey != NULL)
		EVP_PKEY_free(pkey);

	return retval;
}

#ifdef IOS
M_bool M_tls_ctx_load_os_trust(SSL_CTX *ctx)
{
	(void)ctx;
	return M_FALSE;
}
#elif defined(__APPLE__)
M_bool M_tls_ctx_load_os_trust(SSL_CTX *ctx)
{
	CFArrayRef  anchors;
	int         ret;
	int         i;
	size_t      count = 0;
	X509_STORE *store;

	if (ctx == NULL)
		return M_FALSE;

	ret = SecTrustCopyAnchorCertificates(&anchors);
	if (ret != 0) {
		return M_FALSE;
	}

	store = SSL_CTX_get_cert_store(ctx);
	for (i = 0; i < CFArrayGetCount(anchors); i++) {
		const void          *ptr = CFArrayGetValueAtIndex(anchors, i);
		SecCertificateRef    cr  = M_CAST_OFF_CONST(SecCertificateRef, ptr);
		CFDataRef            dref;
		X509                *x509;
		const unsigned char *data;

		dref = SecCertificateCopyData(cr);
		if (dref == NULL)
			continue;

		/* DER-encoded
 		 *
 		 * CFDataGetLength will be auto converted to long by
		 * the compiler (this is not undefined behavior). */ 
		data = CFDataGetBytePtr(dref);
		x509 = d2i_X509(NULL, &data, CFDataGetLength(dref));
		CFRelease(dref);
		if (x509 == NULL)
			continue;

		if (X509_STORE_add_cert(store, x509))
			count++;

		X509_free(x509);
	}
	CFRelease(anchors);

	if (!count)
		return M_FALSE;

	return M_TRUE;
}

#elif defined(_WIN32)
M_bool M_tls_ctx_load_os_trust(SSL_CTX *ctx)
{
	HCERTSTORE     hStore;
	PCCERT_CONTEXT pContext = NULL;
	X509_STORE    *store;
	size_t         count    = 0;

	if (ctx == NULL)
		return M_FALSE;

	hStore = CertOpenSystemStore(0, "ROOT");
	if (hStore == NULL)
		return M_FALSE;

	store = SSL_CTX_get_cert_store(ctx);

	while ((pContext=CertEnumCertificatesInStore(hStore, pContext)) != NULL) {
		BYTE * const *cert = &pContext->pbCertEncoded;
		X509         *x509 = d2i_X509(NULL, M_CAST_OFF_CONST(const unsigned char **, cert), (long)pContext->cbCertEncoded);
		if (x509) {
			if (X509_STORE_add_cert(store, x509))
				count++;
			X509_free(x509);
		}
	}

	CertFreeCertificateContext(pContext);
	CertCloseStore(hStore, 0);

	return M_TRUE;
} 

#else /* Unix */

M_bool M_tls_ctx_load_os_trust(SSL_CTX *ctx)
{
	static const char * const cafile_paths[] = {
		"/etc/ssl/certs/ca-certificates.crt",
		"/etc/pki/tls/cert.pem",
		"/etc/pki/tls/certs/ca-bundle.crt",
		"/usr/share/ssl/certs/ca-bundle.crt",
		"/etc/pki/tls/certs/ca-bundle.trust.crt",
		"/usr/local/share/certs/ca-root-nss.crt", /* FreeBSD via port security/ca_root_nss */
		NULL
	};
	static const struct {
		const char *path;
		const char *pattern;
	} cadirs[] = {
		{ "/system/etc/security/cacerts/", "*" }, /* Android */
		{ NULL, NULL }
	};
	size_t i;

	if (ctx == NULL)
		return M_FALSE;

	for (i=0; cafile_paths[i] != NULL; i++) {
		if (M_tls_ctx_set_trust_ca_file(ctx, NULL, cafile_paths[i]))
			return M_TRUE;
	}

	for (i=0; cadirs[i].path != NULL; i++) {
		if (M_tls_ctx_set_trust_ca_dir(ctx, NULL, cadirs[i].path, cadirs[i].pattern))
			return M_TRUE;
	}
	return M_FALSE;
}
#endif


M_bool M_tls_ctx_set_x509trust(SSL_CTX *ctx, STACK_OF(X509) *trustlist)
{
	int         i;
	X509_STORE *store;

	if (ctx == NULL || trustlist == NULL)
		return M_FALSE;

	store = SSL_CTX_get_cert_store(ctx);
	for (i=0; i < sk_X509_num(trustlist); i++) {
		X509_STORE_add_cert(store, sk_X509_value(trustlist, i));
	}

	return M_TRUE;
}

/* XXX: We should probably check the start/end dates for CA certificates and not load them
 *      if out of range as this is a known issue in OpenSSL where it will only match the
 *      first certificate in a trust list, not the most relevant */


M_bool M_tls_ctx_set_trust_ca(SSL_CTX *ctx, STACK_OF(X509) *trustlist_cache, const unsigned char *ca, size_t len)
{
	BIO                 *bio; 
	STACK_OF(X509_INFO) *sk;
	X509_STORE          *store;
	int                  i;
	size_t               count = 0;

	if (ctx == NULL || ca == NULL || len == 0)
		return M_FALSE;

	/* Load CAfile into BIO */
	bio = BIO_new_mem_buf(M_CAST_OFF_CONST(void *, ca), (int)len);

	/* Parse the CAfile into a stack of X509 INFO structures */
	sk  = PEM_X509_INFO_read_bio(bio, NULL, NULL, NULL);
	BIO_free(bio);
	if (sk == NULL)
		return M_FALSE;

	/* Iterate across the stack and pull out the x509 certs and add them to our store */
	store = SSL_CTX_get_cert_store(ctx);
	for(i = 0; i < sk_X509_INFO_num(sk); i++) {
		X509_INFO *x509info = sk_X509_INFO_value(sk, i);
		if (x509info->x509 != NULL) {
			X509_STORE_add_cert(store, x509info->x509);
			if (trustlist_cache != NULL) {
				/* NOTE: no upref here, we rely on the fact that the X509 store is referencing it */
				sk_X509_push(trustlist_cache, x509info->x509);
			}
			count++;
		}
		/* XXX: should we add CRLs? */
	}

	/* Free the stack and all members */
	sk_X509_INFO_pop_free(sk, X509_INFO_free);

	/* If nothing was loaded, return error */
	if (!count)
		return M_FALSE;

	return M_TRUE;
}


M_bool M_tls_ctx_set_trust_ca_file(SSL_CTX *ctx, STACK_OF(X509) *trustlist_cache, const char *path)
{
	M_bool         retval;
	unsigned char *ca  = NULL;
	size_t         len = 0;

	if (ctx == NULL || path == NULL)
		return M_FALSE;

	if (M_fs_file_read_bytes(path, 0, &ca, &len) != M_FS_ERROR_SUCCESS)
		return M_FALSE;

	retval = M_tls_ctx_set_trust_ca(ctx, trustlist_cache, ca, len);
	M_free(ca);
	return retval;
}


M_bool M_tls_ctx_set_trust_cert(SSL_CTX *ctx, STACK_OF(X509) *trustlist_cache, const unsigned char *crt, size_t len)
{
	X509       *x509;
	BIO        *bio;
	X509_STORE *store;

	if (ctx == NULL || crt == NULL || len == 0)
		return M_FALSE;

	/* Load Cert into X509 */
	bio  = BIO_new_mem_buf(M_CAST_OFF_CONST(void *, crt), (int)len);
	x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	BIO_free(bio);
	if (x509 == NULL)
		return M_FALSE;

	store = SSL_CTX_get_cert_store(ctx);
	X509_STORE_add_cert(store, x509);
	if (trustlist_cache != NULL) {
		/* NOTE: no upref here, we rely on the fact that the X509 store is referencing it */
		sk_X509_push(trustlist_cache, x509);
	}
	X509_free(x509);

	return M_TRUE;
}


M_bool M_tls_ctx_set_trust_cert_file(SSL_CTX *ctx, STACK_OF(X509) *trustlist_cache, const char *path)
{
	M_bool         retval;
	unsigned char *crt = NULL;
	size_t         len = 0;

	if (ctx == NULL || path == NULL)
		return M_FALSE;

	if (M_fs_file_read_bytes(path, 0, &crt, &len) != M_FS_ERROR_SUCCESS)
		return M_FALSE;

	retval = M_tls_ctx_set_trust_cert(ctx, trustlist_cache, crt, len);
	M_free(crt);
	return retval;
}


M_bool M_tls_ctx_set_trust_ca_dir(SSL_CTX *ctx, STACK_OF(X509) *trustlist_cache, const char *path, const char *pattern)
{
	M_list_str_t  *files;
	size_t         i;
	size_t         cnt;
	size_t         num_loaded = 0;

	if (ctx == NULL || M_str_isempty(path))
		return M_FALSE;

	files = M_fs_dir_walk_strs(path, pattern, M_FS_DIR_WALK_FILTER_FILE|M_FS_DIR_WALK_FILTER_RECURSE|
	                                          M_FS_DIR_WALK_FILTER_CASECMP|M_FS_DIR_WALK_FILTER_JAIL_SKIP|
	                                          M_FS_DIR_WALK_FILTER_AS_SET);
	if (files == NULL || M_list_str_len(files) == 0) {
		M_list_str_destroy(files);
		return M_FALSE;
	}

	cnt = M_list_str_len(files);
	for (i=0; i<cnt; i++) {
		char *filename = M_fs_path_join(path, M_list_str_at(files, i), M_FS_SYSTEM_AUTO);

		if (M_tls_ctx_set_trust_cert_file(ctx, trustlist_cache, filename))
			num_loaded++;

		M_free(filename);
	}

	M_list_str_destroy(files);

	if (!num_loaded) {
		return M_FALSE;
	}

	return M_TRUE;
}


unsigned char *M_tls_alpn_list(M_list_str_t *apps, size_t *applen)
{
	M_buf_t      *buf    = M_buf_create();
	size_t        i;
	unsigned char *out   = NULL;

	*applen = 0;

	if (apps == NULL || M_list_str_len(apps) == 0)
		return NULL;

	for (i=0; i<M_list_str_len(apps); i++) {
		const char *str = M_list_str_at(apps, i);
		size_t      len = M_str_len(str);
		if (str == NULL || len == 0 || len > 255) {
			M_buf_cancel(buf);
			return NULL;
		}
		M_buf_add_byte(buf, len & 0xFF);
		M_buf_add_str(buf, str);
	}

	out = M_buf_finish(buf, applen);

	return out;
}
