#include <stdbool.h>
#include <stdio.h>
#include "ssl.h"
#include "js_socket.h"	// TODO... move this stuff in here?

static scfg_t	scfg;

void DLLCALL free_crypt_attrstr(char *attr)
{
	free(attr);
}

char* DLLCALL get_crypt_attribute(CRYPT_SESSION sess, C_IN CRYPT_ATTRIBUTE_TYPE attr)
{
	int		len = 0;
	char	*estr = NULL;

	if (cryptStatusOK(cryptGetAttributeString(sess, attr, NULL, &len))) {
		estr = malloc(len + 1);
		if (estr) {
			cryptGetAttributeString(sess, attr, estr, &len);
			estr[len] = 0;
			return estr;
		}
	}
	return NULL;
}

char* DLLCALL get_crypt_error(CRYPT_SESSION sess)
{
	return get_crypt_attribute(sess, CRYPT_ATTRIBUTE_ERRORMESSAGE);
}

static bool get_error_string(int status, CRYPT_SESSION sess, char estr[SSL_ESTR_LEN], char *file, int line)
{
	char	*emsg;

	if (cryptStatusOK(status))
		return true;

	emsg = get_crypt_error(sess);
	if (emsg) {
		safe_snprintf(estr, SSL_ESTR_LEN, "cryptlib error %d at %s:%d (%s)", status, file, line, emsg);
		free_crypt_attrstr(emsg);
	}
	else
		safe_snprintf(estr, SSL_ESTR_LEN, "cryptlib error %d at %s:%d", status, file, line);
	return false;
}

#define DO(x)	get_error_string(x, ssl_context, estr, __FILE__, __LINE__)

CRYPT_CONTEXT DLLCALL get_ssl_cert(scfg_t *cfg, char estr[SSL_ESTR_LEN])
{
	CRYPT_KEYSET		ssl_keyset;
	CRYPT_CONTEXT		ssl_context;
	CRYPT_CERTIFICATE	ssl_cert;
	int					i;
	char				sysop_email[sizeof(cfg->sys_inetaddr)+6];
    char				str[MAX_PATH+1];

	if(!do_cryptInit())
		return -1;
	memset(&ssl_context, 0, sizeof(ssl_context));
	/* Get the certificate... first try loading it from a file... */
	SAFEPRINTF2(str,"%s%s",cfg->ctrl_dir,"ssl.cert");
	if(cryptStatusOK(cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, str, CRYPT_KEYOPT_NONE))) {
		if(!DO(cryptGetPrivateKey(ssl_keyset, &ssl_context, CRYPT_KEYID_NAME, "ssl_cert", cfg->sys_pass)))
			return -1;
	}
	else {
		/* Couldn't do that... create a new context and use the cert from there... */
		if(!cryptStatusOK(i=cryptCreateContext(&ssl_context, CRYPT_UNUSED, CRYPT_ALGO_RSA))) {
			sprintf(estr, "cryptlib error %d creating SSL context",i);
			return -1;
		}
		if(!DO(cryptSetAttributeString(ssl_context, CRYPT_CTXINFO_LABEL, "ssl_cert", 8)))
			goto failure_return_1;
		if(!DO(cryptGenerateKey(ssl_context)))
			goto failure_return_1;
		if(!DO(cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, str, CRYPT_KEYOPT_CREATE)))
			goto failure_return_1;
		if(!DO(cryptAddPrivateKey(ssl_keyset, ssl_context, cfg->sys_pass)))
			goto failure_return_2;
		if(!DO(cryptCreateCert(&ssl_cert, CRYPT_UNUSED, CRYPT_CERTTYPE_CERTIFICATE)))
			goto failure_return_2;
		if(!DO(cryptSetAttribute(ssl_cert, CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, ssl_context)))
			goto failure_return_3;
		if(!DO(cryptSetAttribute(ssl_cert, CRYPT_CERTINFO_XYZZY, 1 )))
			goto failure_return_3;
		if(!DO(cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_COMMONNAME, cfg->sys_inetaddr, strlen(cfg->sys_inetaddr))))
			goto failure_return_3;
		sprintf(sysop_email, "sysop@%s", scfg.sys_inetaddr);
		if(!DO(cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_RFC822NAME, cfg->sys_inetaddr, strlen(cfg->sys_inetaddr))))
			goto failure_return_3;
		if(!DO(cryptSignCert(ssl_cert, ssl_context)))
			goto failure_return_3;
		if(!DO(cryptAddPublicKey(ssl_keyset, ssl_cert)))
			goto failure_return_3;
		cryptDestroyCert(ssl_cert);
	}

	cryptKeysetClose(ssl_keyset);
	return ssl_context;

failure_return_3:
	cryptDestroyCert(ssl_cert);
failure_return_2:
	cryptKeysetClose(ssl_keyset);
failure_return_1:
	cryptDestroyContext(ssl_context);
	return -1;
}
