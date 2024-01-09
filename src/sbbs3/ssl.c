#include <stdbool.h>
#include <stdio.h>

#include <threadwrap.h>
#include "xpprintf.h"
#include "eventwrap.h"
#include "ini_file.h" // For INI_MAX_VALUE_LEN

#include "ssl.h"

void free_crypt_attrstr(char *attr)
{
	free(attr);
}

char* get_crypt_attribute(CRYPT_HANDLE sess, C_IN CRYPT_ATTRIBUTE_TYPE attr)
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

char* get_crypt_error(CRYPT_HANDLE sess)
{
	return get_crypt_attribute(sess, CRYPT_ATTRIBUTE_ERRORMESSAGE);
}

static int crypt_ll(int error)
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

bool get_crypt_error_string(int status, CRYPT_HANDLE sess, char **estr, const char *action, int *lvl)
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
			if(asprintf(estr, "%s '%s' (%d) %s", crypt_lstr(level), emsg, status, action) < 0)
				*estr = NULL;
			if (allocated)
				free_crypt_attrstr(emsg);
		}
		else {
			if(asprintf(estr, "%s (%d) %s", crypt_lstr(level), status, action) < 0)
				*estr = NULL;
		}
	}
	return false;
}

static pthread_once_t crypt_init_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t ssl_cert_mutex;
static pthread_mutex_t ssl_cert_list_mutex;
static xpevent_t ssl_cert_read_available;
static xpevent_t ssl_cert_write_available;
static bool cryptlib_initialized;
static int cryptInit_error;

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
		cryptInit_error = ret; 
	}
	pthread_mutex_init(&ssl_cert_mutex, NULL);
	pthread_mutex_init(&ssl_cert_list_mutex, NULL);
	ssl_cert_read_available = CreateEvent(NULL, TRUE, TRUE, "ssl_cert_read_available");
	ssl_cert_write_available = CreateEvent(NULL, TRUE, TRUE, "ssl_cert_write_available");
	return;
}

bool do_cryptInit(int (*lprintf)(int level, const char* fmt, ...))
{
	int ret;
	if ((ret = pthread_once(&crypt_init_once, internal_do_cryptInit)) != 0) {
		lprintf(LOG_ERR, "%s call to pthread_once failed with error %d", __FUNCTION__, ret);
		return false;
	}
	if (!cryptlib_initialized)
		lprintf(LOG_ERR,"cryptInit() returned %d", cryptInit_error);
	return cryptlib_initialized;
}

bool is_crypt_initialized(void)
{
	return cryptlib_initialized;
}

static uint32_t readers;
static uint32_t writers;
static uint32_t writers_waiting;

static void lock_ssl_cert(void)
{
	int done = 0;

	pthread_mutex_lock(&ssl_cert_mutex);
	do {
		done = (writers == 0 && writers_waiting == 0);
		if (!done) {
			pthread_mutex_unlock(&ssl_cert_mutex);
			WaitForEvent(ssl_cert_read_available, INFINITE);
			pthread_mutex_lock(&ssl_cert_mutex);
		}
	} while (!done);
	ResetEvent(ssl_cert_write_available);
	readers++;
	pthread_mutex_unlock(&ssl_cert_mutex);
}

static void lock_ssl_cert_write(void)
{
	int done;

	ResetEvent(ssl_cert_read_available);
	pthread_mutex_lock(&ssl_cert_mutex);
	writers_waiting++;
	do {
		done = (readers == 0 && writers == 0);
		if (!done) {
			pthread_mutex_unlock(&ssl_cert_mutex);
			WaitForEvent(ssl_cert_write_available, INFINITE);
			pthread_mutex_lock(&ssl_cert_mutex);
		}
	} while(!done);
	ResetEvent(ssl_cert_write_available);
	writers_waiting--;
	writers++;
	pthread_mutex_unlock(&ssl_cert_mutex);
}

static void unlock_ssl_cert(int (*lprintf)(int level, const char* fmt, ...))
{
	pthread_mutex_lock(&ssl_cert_mutex);
	if (readers == 0) {
		lprintf(LOG_ERR, "Unlocking ssl cert for read when it's not locked.");
	}
	else {
		readers--;
		if (readers == 0) {
			SetEvent(ssl_cert_write_available);
		}
	}
	pthread_mutex_unlock(&ssl_cert_mutex);
}

static void unlock_ssl_cert_write(int (*lprintf)(int level, const char* fmt, ...))
{
	pthread_mutex_lock(&ssl_cert_mutex);
	if (writers == 0) {
		lprintf(LOG_ERR, "Unlocking ssl cert for write when it's not locked.");
	}
	else {
		writers--;
		SetEvent(ssl_cert_write_available);
		if (writers_waiting == 0) {
			SetEvent(ssl_cert_read_available);
		}
	}
	pthread_mutex_unlock(&ssl_cert_mutex);
}

// TODO: Failures in get_ssl_cert() aren't logged.
#define DO(action, handle, x)	cryptStatusOK(x)

static char cert_path[INI_MAX_VALUE_LEN + 8];
static unsigned cert_epoch;
struct cert_list {
	struct cert_list *next;
	CRYPT_SESSION sess;
	CRYPT_CONTEXT cert;
	unsigned epoch;
};
static struct cert_list *sess_list;
static struct cert_list *cert_list;
static time_t tls_cert_file_date;

bool ssl_sync(scfg_t *scfg, int (*lprintf)(int level, const char* fmt, ...))
{
	if (!do_cryptInit(lprintf))
		return false;
	lock_ssl_cert_write();
	if (!cert_path[0])
		SAFEPRINTF2(cert_path,"%s%s",scfg->ctrl_dir,"ssl.cert");
	time_t fd = fdate(cert_path);
	if (tls_cert_file_date != 0) {
		if (fd != tls_cert_file_date) {
			tls_cert_file_date = fd;
			lprintf(LOG_DEBUG, "Destroying TLS private keys");
			cert_epoch++;
			if (cert_epoch == 0)
				cert_epoch = 1;
			pthread_mutex_lock(&ssl_cert_list_mutex);
			while (cert_list) {
				struct cert_list *old;
				old = cert_list;
				cert_list = cert_list->next;
				cryptDestroyContext(old->cert);
				free(old);
			}
			pthread_mutex_unlock(&ssl_cert_list_mutex);
		}
	}
	tls_cert_file_date = fd;
	unlock_ssl_cert_write(lprintf);
	return true;
}

static CRYPT_CONTEXT get_ssl_cert(scfg_t *cfg, int (*lprintf)(int level, const char* fmt, ...))
{
	CRYPT_KEYSET		ssl_keyset;
	CRYPT_CERTIFICATE	ssl_cert;
	char			sysop_email[sizeof(cfg->sys_inetaddr)+6];
	struct cert_list *cert_entry;

	if(!do_cryptInit(lprintf))
		return -1;
	ssl_sync(cfg, lprintf);
	lock_ssl_cert_write();
	cert_entry = malloc(sizeof(*cert_entry));
	if(cert_entry == NULL) {
		unlock_ssl_cert_write(lprintf);
		free(cert_entry);
		lprintf(LOG_CRIT, "%s line %d: FAILED TO ALLOCATE %u bytes of memory", __FUNCTION__, __LINE__, sizeof *cert_entry);
		return -1;
	}
	cert_entry->sess = -1;
	cert_entry->epoch = cert_epoch;
	cert_entry->next = NULL;

	/* Get the certificate... first try loading it from a file... */
	if(cryptStatusOK(cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, cert_path, CRYPT_KEYOPT_READONLY))) {
		if(!DO("getting private key", ssl_keyset, cryptGetPrivateKey(ssl_keyset, &cert_entry->cert, CRYPT_KEYID_NAME, "ssl_cert", cfg->sys_pass))) {
			unlock_ssl_cert_write(lprintf);
			free(cert_entry);
			return -1;
		}
	}
	else {
		/* Couldn't do that... create a new context and use the cert from there... */
		if(!DO("creating SSL context", CRYPT_UNUSED,cryptCreateContext(&cert_entry->cert, CRYPT_UNUSED, CRYPT_ALGO_RSA))) {
			unlock_ssl_cert_write(lprintf);
			free(cert_entry);
			return -1;
		}
		if(!DO("setting label", cert_entry->cert, cryptSetAttributeString(cert_entry->cert, CRYPT_CTXINFO_LABEL, "ssl_cert", 8)))
			goto failure_return_1;
		if(!DO("generating key", cert_entry->cert, cryptGenerateKey(cert_entry->cert)))
			goto failure_return_1;
		if(!DO("opening keyset", CRYPT_UNUSED, cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, cert_path, CRYPT_KEYOPT_CREATE)))
			goto failure_return_1;
		if(!DO("adding private key", ssl_keyset, cryptAddPrivateKey(ssl_keyset, cert_entry->cert, cfg->sys_pass)))
			goto failure_return_2;
		if(!DO("creating certificate", CRYPT_UNUSED, cryptCreateCert(&ssl_cert, CRYPT_UNUSED, CRYPT_CERTTYPE_CERTIFICATE)))
			goto failure_return_2;
		if(!DO("setting public key", ssl_cert, cryptSetAttribute(ssl_cert, CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, cert_entry->cert)))
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
		if(!DO("signing certificate", ssl_cert, cryptSignCert(ssl_cert, cert_entry->cert)))
			goto failure_return_3;
		if(!DO("adding public key", ssl_keyset, cryptAddPublicKey(ssl_keyset, ssl_cert)))
			goto failure_return_3;
		cryptDestroyCert(ssl_cert);
		cryptKeysetClose(ssl_keyset);
		cryptDestroyContext(cert_entry->cert);
		cert_entry->cert = -1;
		// Finally, load it from the file.
		if(cryptStatusOK(cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, cert_path, CRYPT_KEYOPT_READONLY))) {
			if(!DO("getting private key", ssl_keyset, cryptGetPrivateKey(ssl_keyset, &cert_entry->cert, CRYPT_KEYID_NAME, "ssl_cert", cfg->sys_pass))) {
				cert_entry->cert = -1;
			}
			else {
				tls_cert_file_date = fdate(cert_path);
			}
		}
	}

	cryptKeysetClose(ssl_keyset);
	if (cert_entry->cert == -1) {
		free(cert_entry);
		cert_path[0] = 0;
	}
	else {
		pthread_mutex_lock(&ssl_cert_list_mutex);
		cert_entry->next = cert_list;
		cert_list = cert_entry;
		lprintf(LOG_DEBUG, "Created TLS private key and certificate %d", cert_entry->cert);
		pthread_mutex_unlock(&ssl_cert_list_mutex);
	}
	unlock_ssl_cert_write(lprintf);
	return 0;

failure_return_3:
	cryptDestroyCert(ssl_cert);
failure_return_2:
	cryptKeysetClose(ssl_keyset);
failure_return_1:
	cryptDestroyContext(cert_entry->cert);
	cert_path[0] = 0;
	unlock_ssl_cert_write(lprintf);
	free(cert_entry);
	return -1;
}

static struct cert_list *get_sess_list_entry(scfg_t *cfg, int (*lprintf)(int level, const char* fmt, ...), CRYPT_SESSION csess)
{
	struct cert_list *ret;

	if (cert_list == NULL) {
		pthread_mutex_unlock(&ssl_cert_list_mutex);
		get_ssl_cert(cfg, lprintf);
		pthread_mutex_lock(&ssl_cert_list_mutex);
	}
	ret = cert_list;
	if (ret) {
		cert_list = ret->next;
	}
	return ret;
}

int add_private_key(scfg_t *cfg, int (*lprintf)(int level, const char* fmt, ...), CRYPT_SESSION csess)
{
	struct cert_list *sess;
	int ret;

	pthread_mutex_lock(&ssl_cert_list_mutex);
	sess = get_sess_list_entry(cfg, lprintf, csess);
	if (sess == NULL) {
		pthread_mutex_unlock(&ssl_cert_list_mutex);
		return CRYPT_ERROR_NOTINITED;
	}
	ret = cryptSetAttribute(csess, CRYPT_SESSINFO_PRIVATEKEY, sess->cert);
	if (cryptStatusOK(ret)) {
		sess->next = sess_list;
		sess->sess = csess;
		sess_list = sess;
	}
	else {
		sess->next = cert_list;
		cert_list = sess;
	}
	pthread_mutex_unlock(&ssl_cert_list_mutex);
	return ret;
}

int destroy_session(int (*lprintf)(int level, const char* fmt, ...), CRYPT_SESSION csess)
{
	struct cert_list *sess;
	struct cert_list *psess = NULL;
	int ret = CRYPT_ERROR_NOTFOUND;

	lock_ssl_cert();
	pthread_mutex_lock(&ssl_cert_list_mutex);
	sess = sess_list;
	while (sess != NULL) {
		if (sess->sess == csess) {
			if (psess == NULL) {
				sess_list = sess->next;
			}
			else {
				psess->next = sess->next;
			}
			if (sess->epoch == cert_epoch) {
				sess->sess = -1;
				sess->next = cert_list;
				cert_list = sess;
				ret = cryptDestroySession(csess);
			}
			else {
				// TODO: Failure here isn't logged
				cryptDestroyContext(sess->cert);
				free(sess);
				ret = cryptDestroySession(csess);
			}
			break;
		}
		psess = sess;
		sess = sess->next;
	}
	pthread_mutex_unlock(&ssl_cert_list_mutex);
	unlock_ssl_cert(lprintf);
	if (ret == CRYPT_ERROR_NOTFOUND)
		ret = cryptDestroySession(csess);
	return ret;
}
