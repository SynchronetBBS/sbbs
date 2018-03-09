#include <stdbool.h>
#include <stdio.h>

#include <threadwrap.h>

#include "ssl.h"
//#include "js_socket.h"	// TODO... move this stuff in here?

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
	if (emsg == NULL) {
		switch(status) {
			case CRYPT_ERROR_PARAM1:
				emsg = strdup("Bad argument, parameter 1");
				break;
			case CRYPT_ERROR_PARAM2:
				emsg = strdup("Bad argument, parameter 2");
				break;
			case CRYPT_ERROR_PARAM3:
				emsg = strdup("Bad argument, parameter 3");
				break;
			case CRYPT_ERROR_PARAM4:
				emsg = strdup("Bad argument, parameter 4");
				break;
			case CRYPT_ERROR_PARAM5:
				emsg = strdup("Bad argument, parameter 5");
				break;
			case CRYPT_ERROR_PARAM6:
				emsg = strdup("Bad argument, parameter 6");
				break;
			case CRYPT_ERROR_PARAM7:
				emsg = strdup("Bad argument, parameter 7");
				break;

			/* Errors due to insufficient resources */

			case CRYPT_ERROR_MEMORY:
				emsg = strdup("Out of memory");
				break;
			case CRYPT_ERROR_NOTINITED:
				emsg = strdup("Data has not been initialised");
				break;
			case CRYPT_ERROR_INITED:
				emsg = strdup("Data has already been init'd");
				break;
			case CRYPT_ERROR_NOSECURE:
				emsg = strdup("Opn.not avail.at requested sec.level");
				break;
			case CRYPT_ERROR_RANDOM:
				emsg = strdup("No reliable random data available");
				break;
			case CRYPT_ERROR_FAILED:
				emsg = strdup("Operation failed");
				break;
			case CRYPT_ERROR_INTERNAL:
				emsg = strdup("Internal consistency check failed");
				break;

			/* Security violations */

			case CRYPT_ERROR_NOTAVAIL:
				emsg = strdup("This type of opn.not available");
				break;
			case CRYPT_ERROR_PERMISSION:
				emsg = strdup("No permiss.to perform this operation");
				break;
			case CRYPT_ERROR_WRONGKEY:
				emsg = strdup("Incorrect key used to decrypt data");
				break;
			case CRYPT_ERROR_INCOMPLETE:
				emsg = strdup("Operation incomplete/still in progress");
				break;
			case CRYPT_ERROR_COMPLETE:
				emsg = strdup("Operation complete/can't continue");
				break;
			case CRYPT_ERROR_TIMEOUT:
				emsg = strdup("Operation timed out before completion");
				break;
			case CRYPT_ERROR_INVALID:
				emsg = strdup("Invalid/inconsistent information");
				break;
			case CRYPT_ERROR_SIGNALLED:
				emsg = strdup("Resource destroyed by extnl.event");
				break;
			   
			/* High-level function errors */

			case CRYPT_ERROR_OVERFLOW:
				emsg = strdup("Resources/space exhausted");
				break;
			case CRYPT_ERROR_UNDERFLOW:
				emsg = strdup("Not enough data available");
				break;
			case CRYPT_ERROR_BADDATA:
				emsg = strdup("Bad/unrecognised data format");
				break;
			case CRYPT_ERROR_SIGNATURE:
				emsg = strdup("Signature/integrity check failed");
				break;

			/* Data access function errors */

			case CRYPT_ERROR_OPEN:
				emsg = strdup("Cannot open object");
				break;
			case CRYPT_ERROR_READ:
				emsg = strdup("Cannot read item from object");
				break;
			case CRYPT_ERROR_WRITE:
				emsg = strdup("Cannot write item to object");
				break;
			case CRYPT_ERROR_NOTFOUND:
				emsg = strdup("Requested item not found in object");
				break;
			case CRYPT_ERROR_DUPLICATE:
				emsg = strdup("Item already present in object");
				break;

			/* Data enveloping errors */

			case CRYPT_ENVELOPE_RESOURCE:
				emsg = strdup("Need resource to proceed");
				break;
		}
	}
	if (emsg) {
		safe_snprintf(estr, SSL_ESTR_LEN, "cryptlib error %d at %s:%d (%s)", status, file, line, emsg);
		free_crypt_attrstr(emsg);
	}
	else
		safe_snprintf(estr, SSL_ESTR_LEN, "cryptlib error %d at %s:%d", status, file, line);
	return false;
}

static pthread_once_t crypt_init_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t ssl_cert_mutex;
static bool cryptlib_initialized;

static void do_cryptEnd(void)
{
	cryptEnd();
}

static void internal_do_cryptInit(void)
{
	int ret;

	if((ret=cryptInit())==CRYPT_OK) {
		cryptAddRandom(NULL,CRYPT_RANDOM_SLOWPOLL);
		atexit(do_cryptEnd);
		cryptlib_initialized = true;
	}
	else {
		lprintf(LOG_ERR,"cryptInit() returned %d", ret);
	}
	pthread_mutex_init(&ssl_cert_mutex, NULL);
	return;
}

int DLLCALL do_cryptInit(void)
{
	if (pthread_once(&crypt_init_once, internal_do_cryptInit) != 0)
		return 0;
	return cryptlib_initialized;
}

bool DLLCALL is_crypt_initialized(void)
{
	return cryptlib_initialized;
}

#define DO(x)	get_error_string(x, ssl_context, estr, __FILE__, __LINE__)

CRYPT_CONTEXT DLLCALL get_ssl_cert(scfg_t *cfg, char estr[SSL_ESTR_LEN])
{
	CRYPT_KEYSET		ssl_keyset;
	CRYPT_CONTEXT		ssl_context = -1;	// MSVC requires this to be initialized
	CRYPT_CERTIFICATE	ssl_cert;
	int					i;
	char				sysop_email[sizeof(cfg->sys_inetaddr)+6];
    char				str[MAX_PATH+1];

	if(!do_cryptInit())
		return -1;
	pthread_mutex_lock(&ssl_cert_mutex);
	if (cfg->tls_certificate != -1 || !cfg->prepped) {
		pthread_mutex_unlock(&ssl_cert_mutex);
		return cfg->tls_certificate;
	}
	/* Get the certificate... first try loading it from a file... */
	SAFEPRINTF2(str,"%s%s",cfg->ctrl_dir,"ssl.cert");
	if(cryptStatusOK(cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, str, CRYPT_KEYOPT_READONLY))) {
		if(!DO(cryptGetPrivateKey(ssl_keyset, &ssl_context, CRYPT_KEYID_NAME, "ssl_cert", cfg->sys_pass))) {
			pthread_mutex_unlock(&ssl_cert_mutex);
			return -1;
		}
	}
	else {
		/* Couldn't do that... create a new context and use the cert from there... */
		if(!cryptStatusOK(i=cryptCreateContext(&ssl_context, CRYPT_UNUSED, CRYPT_ALGO_RSA))) {
			pthread_mutex_unlock(&ssl_cert_mutex);
			if (estr)
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
		if(!DO(cryptSetAttribute(ssl_cert, CRYPT_CERTINFO_SELFSIGNED, 1)))
			goto failure_return_3;
		if(!DO(cryptSetAttribute(ssl_cert, CRYPT_OPTION_CERT_VALIDITY, 3650)))
			goto failure_return_3;
		if(!DO(cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_COUNTRYNAME, "ZZ", 2)))
			goto failure_return_3;
		if(!DO(cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_ORGANIZATIONNAME, cfg->sys_name, strlen(cfg->sys_name))))
			goto failure_return_3;
		if(!DO(cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_DNSNAME, cfg->sys_inetaddr, strlen(cfg->sys_inetaddr))))
			goto failure_return_3;
		if(!DO(cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_COMMONNAME, cfg->sys_inetaddr, strlen(cfg->sys_inetaddr))))
			goto failure_return_3;
		sprintf(sysop_email, "sysop@%s", cfg->sys_inetaddr);
		if(!DO(cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_RFC822NAME, sysop_email, strlen(sysop_email))))
			goto failure_return_3;
		if(!DO(cryptSignCert(ssl_cert, ssl_context)))
			goto failure_return_3;
		if(!DO(cryptAddPublicKey(ssl_keyset, ssl_cert)))
			goto failure_return_3;
		cryptDestroyCert(ssl_cert);
		cryptKeysetClose(ssl_keyset);
		cryptDestroyContext(ssl_context);
		// Finally, load it from the file.
		if(cryptStatusOK(cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, str, CRYPT_KEYOPT_READONLY))) {
			if(!DO(cryptGetPrivateKey(ssl_keyset, &ssl_context, CRYPT_KEYID_NAME, "ssl_cert", cfg->sys_pass))) {
				ssl_context = -1;
			}
		}
	}

	cryptKeysetClose(ssl_keyset);
	pthread_mutex_unlock(&ssl_cert_mutex);
	cfg->tls_certificate = ssl_context;
	return ssl_context;

failure_return_3:
	cryptDestroyCert(ssl_cert);
failure_return_2:
	cryptKeysetClose(ssl_keyset);
failure_return_1:
	cryptDestroyContext(ssl_context);
	pthread_mutex_unlock(&ssl_cert_mutex);
	return -1;
}
