#include <stdbool.h>
#include <stdio.h>

#include <threadwrap.h>
#include "xpprintf.h"

#include "ssl.h"
//#include "js_socket.h"	// TODO... move this stuff in here?

void DLLCALL free_crypt_attrstr(char *attr)
{
	free(attr);
}

char* DLLCALL get_crypt_attribute(CRYPT_HANDLE sess, C_IN CRYPT_ATTRIBUTE_TYPE attr)
{
	int		len = 0;
	char	*estr = NULL;

	if (cryptStatusOK(cryptGetAttributeString(sess, attr, NULL, &len))) {
		estr = malloc(len + 1);
		if (estr) {
			if (cryptStatusError(cryptGetAttributeString(sess, attr, estr, &len))) {
				free(estr);
				return NULL;
			}
			estr[len] = 0;
			return estr;
		}
	}
	return NULL;
}

char* DLLCALL get_crypt_error(CRYPT_HANDLE sess)
{
	return get_crypt_attribute(sess, CRYPT_ATTRIBUTE_ERRORMESSAGE);
}

static int DLLCALL crypt_ll(int error)
{
	switch(error) {
		case CRYPT_ERROR_INCOMPLETE:
		case CRYPT_ERROR_NOSECURE:
		case CRYPT_ERROR_BADDATA:
		case CRYPT_ERROR_INVALID:
			return LOG_WARNING;
		case CRYPT_ERROR_INTERNAL:
			return LOG_NOTICE;
		case CRYPT_ERROR_COMPLETE:
		case CRYPT_ERROR_READ:
		case CRYPT_ERROR_WRITE:
		case CRYPT_ENVELOPE_RESOURCE:
			return LOG_DEBUG;
		case CRYPT_ERROR_NOTAVAIL:
		case CRYPT_ERROR_TIMEOUT:
			return LOG_INFO;
	}
	return LOG_ERR;
}

static const char *crypt_lstr(int level)
{
	switch(level) {
		case LOG_EMERG:
			return "!ERROR";
		case LOG_ALERT:
			return "!ERROR";
		case LOG_CRIT:
			return "!ERROR";
		case LOG_ERR:
			return "ERROR";
		case LOG_WARNING:
			return "WARNING";
		case LOG_NOTICE:
			return "note";
		case LOG_INFO:
			return "info";
		case LOG_DEBUG:
			return "dbg";
	}
	return "!!!!!!!!";
}

bool DLLCALL get_crypt_error_string(int status, CRYPT_HANDLE sess, char **estr, const char *action, int *lvl)
{
	char	*emsg = NULL;
	bool	allocated = false;
	int	level;

	if (cryptStatusOK(status)) {
		if (estr)
			*estr = NULL;
		return true;
	}

	level = crypt_ll(status);
	if (lvl)
		*lvl = level;

	if (estr) {
		if (sess != CRYPT_UNUSED)
			emsg = get_crypt_error(sess);
		if (emsg != NULL)
			allocated = true;
		if (emsg == NULL) {
			switch(status) {
				case CRYPT_ERROR_PARAM1:
					emsg = "Bad argument, parameter 1";
					break;
				case CRYPT_ERROR_PARAM2:
					emsg = "Bad argument, parameter 2";
					break;
				case CRYPT_ERROR_PARAM3:
					emsg = "Bad argument, parameter 3";
					break;
				case CRYPT_ERROR_PARAM4:
					emsg = "Bad argument, parameter 4";
					break;
				case CRYPT_ERROR_PARAM5:
					emsg = "Bad argument, parameter 5";
					break;
				case CRYPT_ERROR_PARAM6:
					emsg = "Bad argument, parameter 6";
					break;
				case CRYPT_ERROR_PARAM7:
					emsg = "Bad argument, parameter 7";
					break;

				/* Errors due to insufficient resources */

				case CRYPT_ERROR_MEMORY:
					emsg = "Out of memory";
					break;
				case CRYPT_ERROR_NOTINITED:
					emsg = "Data has not been initialised";
					break;
				case CRYPT_ERROR_INITED:
					emsg = "Data has already been init'd";
					break;
				case CRYPT_ERROR_NOSECURE:
					emsg = "Opn.not avail.at requested sec.level";
					break;
				case CRYPT_ERROR_RANDOM:
					emsg = "No reliable random data available";
					break;
				case CRYPT_ERROR_FAILED:
					emsg = "Operation failed";
					break;
				case CRYPT_ERROR_INTERNAL:
					emsg = "Internal consistency check failed";
					break;

				/* Security violations */

				case CRYPT_ERROR_NOTAVAIL:
					emsg = "This type of opn.not available";
					break;
				case CRYPT_ERROR_PERMISSION:
					emsg = "No permiss.to perform this operation";
					break;
				case CRYPT_ERROR_WRONGKEY:
					emsg = "Incorrect key used to decrypt data";
					break;
				case CRYPT_ERROR_INCOMPLETE:
					emsg = "Operation incomplete/still in progress";
					break;
				case CRYPT_ERROR_COMPLETE:
					emsg = "Operation complete/can't continue";
					break;
				case CRYPT_ERROR_TIMEOUT:
					emsg = "Operation timed out before completion";
					break;
				case CRYPT_ERROR_INVALID:
					emsg = "Invalid/inconsistent information";
					break;
				case CRYPT_ERROR_SIGNALLED:
					emsg = "Resource destroyed by extnl.event";
					break;

				/* High-level function errors */

				case CRYPT_ERROR_OVERFLOW:
					emsg = "Resources/space exhausted";
					break;
				case CRYPT_ERROR_UNDERFLOW:
					emsg = "Not enough data available";
					break;
				case CRYPT_ERROR_BADDATA:
					emsg = "Bad/unrecognized data format";
					break;
				case CRYPT_ERROR_SIGNATURE:
					emsg = "Signature/integrity check failed";
					break;

				/* Data access function errors */

				case CRYPT_ERROR_OPEN:
					emsg = "Cannot open object";
					break;
				case CRYPT_ERROR_READ:
					emsg = "Cannot read item from object";
					break;
				case CRYPT_ERROR_WRITE:
					emsg = "Cannot write item to object";
					break;
				case CRYPT_ERROR_NOTFOUND:
					emsg = "Requested item not found in object";
					break;
				case CRYPT_ERROR_DUPLICATE:
					emsg = "Item already present in object";
					break;

				/* Data enveloping errors */

				case CRYPT_ENVELOPE_RESOURCE:
					emsg = "Need resource to proceed";
					break;
			}
		}
		if (emsg) {
			asprintf(estr, "%s '%s' (%d) %s", crypt_lstr(level), emsg, status, action);
			if (allocated)
				free_crypt_attrstr(emsg);
		}
		else
			asprintf(estr, "%s (%d) %s", crypt_lstr(level), status, action);
	}
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

void lock_ssl_cert(void)
{
	pthread_mutex_lock(&ssl_cert_mutex);
}

void unlock_ssl_cert(void)
{
	pthread_mutex_unlock(&ssl_cert_mutex);
}

#define DO(action, handle, x)	get_crypt_error_string(x, handle, estr, action, level)

CRYPT_CONTEXT DLLCALL get_ssl_cert(scfg_t *cfg, char **estr, int *level)
{
	CRYPT_KEYSET		ssl_keyset;
	CRYPT_CONTEXT		ssl_context = -1;	// MSVC requires this to be initialized
	CRYPT_CERTIFICATE	ssl_cert;
	char				sysop_email[sizeof(cfg->sys_inetaddr)+6];
	char				str[MAX_PATH+1];

	if (estr)
		*estr = NULL;
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
		if(!DO("getting private key", ssl_keyset, cryptGetPrivateKey(ssl_keyset, &ssl_context, CRYPT_KEYID_NAME, "ssl_cert", cfg->sys_pass))) {
			pthread_mutex_unlock(&ssl_cert_mutex);
			return -1;
		}
	}
	else {
		/* Couldn't do that... create a new context and use the cert from there... */
		if(!DO("creating SSL context", CRYPT_UNUSED,cryptCreateContext(&ssl_context, CRYPT_UNUSED, CRYPT_ALGO_RSA))) {
			pthread_mutex_unlock(&ssl_cert_mutex);
			return -1;
		}
		if(!DO("setting label", ssl_context, cryptSetAttributeString(ssl_context, CRYPT_CTXINFO_LABEL, "ssl_cert", 8)))
			goto failure_return_1;
		if(!DO("generating key", ssl_context, cryptGenerateKey(ssl_context)))
			goto failure_return_1;
		if(!DO("opening keyset", CRYPT_UNUSED, cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, str, CRYPT_KEYOPT_CREATE)))
			goto failure_return_1;
		if(!DO("adding private key", ssl_keyset, cryptAddPrivateKey(ssl_keyset, ssl_context, cfg->sys_pass)))
			goto failure_return_2;
		if(!DO("creating certificate", CRYPT_UNUSED, cryptCreateCert(&ssl_cert, CRYPT_UNUSED, CRYPT_CERTTYPE_CERTIFICATE)))
			goto failure_return_2;
		if(!DO("setting public key", ssl_cert, cryptSetAttribute(ssl_cert, CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, ssl_context)))
			goto failure_return_3;
		if(!DO("signing certificate", ssl_cert, cryptSetAttribute(ssl_cert, CRYPT_CERTINFO_SELFSIGNED, 1)))
			goto failure_return_3;
		if(!DO("verifying certificate", ssl_cert, cryptSetAttribute(ssl_cert, CRYPT_OPTION_CERT_VALIDITY, 3650)))
			goto failure_return_3;
		if(!DO("setting country name", ssl_cert, cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_COUNTRYNAME, "ZZ", 2)))
			goto failure_return_3;
		if(!DO("setting organization name", ssl_cert, cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_ORGANIZATIONNAME, cfg->sys_name, strlen(cfg->sys_name))))
			goto failure_return_3;
		if(!DO("setting DNS name", ssl_cert, cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_DNSNAME, cfg->sys_inetaddr, strlen(cfg->sys_inetaddr))))
			goto failure_return_3;
		if(!DO("setting Common Name", ssl_cert, cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_COMMONNAME, cfg->sys_inetaddr, strlen(cfg->sys_inetaddr))))
			goto failure_return_3;
		sprintf(sysop_email, "sysop@%s", cfg->sys_inetaddr);
		if(!DO("setting email", ssl_cert, cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_RFC822NAME, sysop_email, strlen(sysop_email))))
			goto failure_return_3;
		if(!DO("signing certificate", ssl_cert, cryptSignCert(ssl_cert, ssl_context)))
			goto failure_return_3;
		if(!DO("adding public key", ssl_keyset, cryptAddPublicKey(ssl_keyset, ssl_cert)))
			goto failure_return_3;
		cryptDestroyCert(ssl_cert);
		cryptKeysetClose(ssl_keyset);
		cryptDestroyContext(ssl_context);
		// Finally, load it from the file.
		if(cryptStatusOK(cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, str, CRYPT_KEYOPT_READONLY))) {
			if(!DO("getting private key", ssl_keyset, cryptGetPrivateKey(ssl_keyset, &ssl_context, CRYPT_KEYID_NAME, "ssl_cert", cfg->sys_pass))) {
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
