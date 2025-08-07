#include <stdbool.h>
#include <stdio.h>

#include "rwlockwrap.h"
#include <threadwrap.h>
#include "xpprintf.h"
#include "eventwrap.h"
#include "ini_file.h" // For INI_MAX_VALUE_LEN

#include "ssl.h"

static pthread_once_t crypt_init_once = PTHREAD_ONCE_INIT;
static bool           cryptlib_initialized;
static int            cryptInit_error;
static char           cert_path[INI_MAX_VALUE_LEN + 8];

static unsigned       cert_epoch; // Current epoch for valid certs in cert_list
static rwlock_t       cert_epoch_lock; // Protects all access to cert_epoch, ssl_cert_list_mutex is locked inside it

struct cert_list {
	struct cert_list *next;
	CRYPT_SESSION sess;
	CRYPT_CONTEXT cert;
	unsigned epoch;
};
static struct cert_list *sess_list;         // List of certificates currently in use
static pthread_mutex_t   ssl_sess_list_mutex; // Protects all accesses to sess_list
static struct cert_list *cert_list;         // List of unused certficiates
static pthread_mutex_t   ssl_cert_list_mutex; // Protects all accesses to cert_list, is locked inside a write cert_epoch_lock
static time_t            tls_cert_file_date; // Last modified date of certificate file
static rwlock_t          tls_cert_file_date_lock; // Protects all access to tls_cert_file_date
static pthread_mutex_t   get_ssl_cert_mutex; // Prevents multiple access to cryptlib in get_ssl_cert()

void free_crypt_attrstr(char *attr)
{
	free(attr);
}

char* get_binary_crypt_attribute(CRYPT_HANDLE sess, C_IN CRYPT_ATTRIBUTE_TYPE attr, size_t *sz)
{
	int   len = 0;
	char *estr = NULL;
	int   status;

	status = cryptGetAttributeString(sess, attr, NULL, &len);
	if (cryptStatusOK(status)) {
		estr = malloc(len + 1);
		if (estr) {
			if (cryptStatusOK(cryptGetAttributeString(sess, attr, estr, &len))) {
				if (len >= 0) {
					estr[len] = 0;
					if (sz)
						*sz = len;
					return estr;
				}
			}
			free(estr);
		}
	}
	return NULL;
}

char* get_crypt_attribute(CRYPT_HANDLE sess, C_IN CRYPT_ATTRIBUTE_TYPE attr)
{
	return get_binary_crypt_attribute(sess, attr, NULL);
}

char* get_crypt_error(CRYPT_HANDLE sess)
{
	return get_crypt_attribute(sess, CRYPT_ATTRIBUTE_ERRORMESSAGE);
}

static int crypt_ll(int error)
{
	switch (error) {
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
	switch (level) {
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
	char *emsg = NULL;
	bool  allocated = false;
	int   level;

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
			switch (status) {
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
			if (asprintf(estr, "%s '%s' (%d) %s", crypt_lstr(level), emsg, status, action) < 0)
				*estr = NULL;
			if (allocated)
				free_crypt_attrstr(emsg);
		}
		else {
			if (asprintf(estr, "%s (%d) %s", crypt_lstr(level), status, action) < 0)
				*estr = NULL;
		}
	}
	return false;
}

static void do_cryptEnd(void)
{
	cryptEnd();
}

static char *cryptfail = NULL;
static void internal_do_cryptInit(void)
{
	int  ret;
	int  maj;
	int  min;
	int  stp;
	int  tmp;
	char patches[32];

	cryptInit_error = CRYPT_ERROR_NOTINITED;

	if (!rwlock_init(&cert_epoch_lock))
		return;
	if (!rwlock_init(&tls_cert_file_date_lock)) {
		rwlock_destroy_ign(&cert_epoch_lock);
		return;
	}
	if (pthread_mutex_init(&ssl_cert_list_mutex, NULL) != 0) {
		rwlock_destroy_ign(&tls_cert_file_date_lock);
		rwlock_destroy_ign(&cert_epoch_lock);
		return;
	}
	if (pthread_mutex_init(&ssl_sess_list_mutex, NULL) != 0) {
		pthread_mutex_destroy(&ssl_cert_list_mutex);
		rwlock_destroy_ign(&tls_cert_file_date_lock);
		rwlock_destroy_ign(&cert_epoch_lock);
		return;
	}
	if (pthread_mutex_init(&get_ssl_cert_mutex, NULL) != 0) {
		pthread_mutex_destroy(&ssl_sess_list_mutex);
		pthread_mutex_destroy(&ssl_cert_list_mutex);
		rwlock_destroy_ign(&tls_cert_file_date_lock);
		rwlock_destroy_ign(&cert_epoch_lock);
		return;
	}

	if ((ret = cryptInit()) == CRYPT_OK) {
		cryptAddRandom(NULL, CRYPT_RANDOM_SLOWPOLL);
		cryptInit_error = CRYPT_OK;
	}
	else {
		cryptInit_error = ret;
	}
	ret = cryptGetAttribute(CRYPT_UNUSED, CRYPT_OPTION_INFO_MAJORVERSION, &maj);
	if (cryptStatusError(ret)) {
		cryptInit_error = ret;
		cryptEnd();
		return;
	}
	ret = cryptGetAttribute(CRYPT_UNUSED, CRYPT_OPTION_INFO_MINORVERSION, &min);
	if (cryptStatusError(ret)) {
		cryptInit_error = ret;
		cryptEnd();
		return;
	}
	ret = cryptGetAttribute(CRYPT_UNUSED, CRYPT_OPTION_INFO_STEPPING, &stp);
	if (cryptStatusError(ret)) {
		cryptInit_error = ret;
		cryptEnd();
		return;
	}
	tmp = (maj * 100) + (min * 10) + stp;
	if (tmp != CRYPTLIB_VERSION) {
		cryptInit_error = CRYPT_ERROR_INVALID;
		cryptEnd();
		if (asprintf(&cryptfail, "Incorrect cryptlib version %d (expected %d)", tmp, CRYPTLIB_VERSION) == -1)
			cryptfail = NULL;
		return;
	}
	ret = cryptGetAttributeString(CRYPT_UNUSED, CRYPT_OPTION_INFO_PATCHES, patches, &stp);
	if (cryptStatusError(ret) || stp != 32 || memcmp(patches, CRYPTLIB_PATCHES, 32) != 0) {
		cryptInit_error = ret;
		cryptEnd();
		if (asprintf(&cryptfail, "Incorrect cryptlib patch set %.32s (expected %s)", patches, CRYPTLIB_PATCHES) == -1)
			cryptfail = NULL;
		return;
	}
	atexit(do_cryptEnd);
	cryptlib_initialized = true;
	return;
}

bool do_cryptInit(int (*lprintf)(int level, const char* fmt, ...))
{
	int ret;
	if ((ret = pthread_once(&crypt_init_once, internal_do_cryptInit)) != 0) {
		lprintf(LOG_ERR, "%s call to pthread_once failed with error %d", __FUNCTION__, ret);
		return false;
	}
	if (!cryptlib_initialized) {
		if (cryptfail) {
			lprintf(LOG_ERR, "cryptInit() returned %d: %s", cryptInit_error, cryptfail);
		}
		else
			lprintf(LOG_ERR, "cryptInit() returned %d", cryptInit_error);
	}
	return cryptlib_initialized;
}

bool is_crypt_initialized(void)
{
	return cryptlib_initialized;
}

static bool
ssl_new_epoch(scfg_t *scfg, int (*lprintf)(int level, const char* fmt, ...))
{
	lprintf(LOG_DEBUG, "New SSL Cert Epoch");
	if (!rwlock_wrlock(&tls_cert_file_date_lock)) {
		lprintf(LOG_ERR, "Unable to lock tls_cert_file_date_lock for write at %d", __LINE__);
		return false;
	}
	tls_cert_file_date = fdate(cert_path);
	if (!rwlock_unlock(&tls_cert_file_date_lock))
		lprintf(LOG_ERR, "Unable to unlock tls_cert_file_date_lock for write at %d", __LINE__);

	if (!rwlock_wrlock(&cert_epoch_lock)) {
		lprintf(LOG_ERR, "Unable to lock cert_epoch_lock for write at %d", __LINE__);
		return false;
	}
	cert_epoch++;
	// Paranoia... keep zero as initial value only.
	if (cert_epoch == 0)
		cert_epoch = 1;
	assert_pthread_mutex_lock(&ssl_cert_list_mutex);
	while (cert_list) {
		struct cert_list *old;
		old = cert_list;
		cert_list = cert_list->next;
		cryptDestroyContext(old->cert);
		free(old);
	}
	assert_pthread_mutex_unlock(&ssl_cert_list_mutex);
	if (!rwlock_unlock(&cert_epoch_lock))
		lprintf(LOG_ERR, "Unable to unlock cert_epoch_lock for write at %d", __LINE__);
	return true;
}

bool ssl_sync(scfg_t *scfg, int (*lprintf)(int level, const char* fmt, ...))
{
	time_t epoch_date;

	if (!do_cryptInit(lprintf))
		return false;
	if (!cert_path[0])
		SAFEPRINTF2(cert_path, "%s%s", scfg->ctrl_dir, "ssl.cert");
	time_t fd = fdate(cert_path);
	if (!rwlock_rdlock(&tls_cert_file_date_lock)) {
		lprintf(LOG_ERR, "Unable to lock tls_cert_file_date_lock for read at %d", __LINE__);
		return false;
	}
	epoch_date = tls_cert_file_date;
	if (!rwlock_unlock(&tls_cert_file_date_lock)) {
		lprintf(LOG_ERR, "Unable to unlock tls_cert_file_date_lock for read at %d", __LINE__);
		return false;
	}
	if (epoch_date != 0) {
		if (fd != epoch_date)
			return ssl_new_epoch(scfg, lprintf);
	}
	return true;
}

static bool
log_cryptlib_error(int status, int line, CRYPT_HANDLE handle, const char *action, int (*lprintf)(int level, const char* fmt, ...))
{
	char *estr;
	int   level;

	get_crypt_error_string(status, handle, &estr, action, &level);
	lprintf(level, "---- TLS %s", estr);
	free_crypt_attrstr(estr);
	return false;
}

#define DO(action, handle, x) (cryptStatusOK((DOtmp = x)) ? true : log_cryptlib_error(DOtmp, __LINE__, handle, action, lprintf))

static bool
create_self_signed_cert(scfg_t *cfg, int (*lprintf)(int level, const char* fmt, ...))
{
	CRYPT_CONTEXT     cert;
	CRYPT_KEYSET      ssl_keyset;
	CRYPT_CERTIFICATE ssl_cert;
	int               DOtmp;
	char              sysop_email[sizeof(cfg->sys_inetaddr) + 6];

	lprintf(LOG_NOTICE, "Creating self-signed TLS certificate");
	if (!cert_path[0]) {
		lprintf(LOG_INFO, "cert_path not set");
		return false;
	}
	/* Create a new context and cert... */
	if (!DO("creating TLS context", CRYPT_UNUSED, cryptCreateContext(&cert, CRYPT_UNUSED, CRYPT_ALGO_RSA)))
		return false;
	if (!DO("setting label", cert, cryptSetAttributeString(cert, CRYPT_CTXINFO_LABEL, "ssl_cert", 8)))
		goto failure_return_1;
	if (!DO("generating key", cert, cryptGenerateKey(cert)))
		goto failure_return_1;
	if (!DO("opening keyset", CRYPT_UNUSED, cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, cert_path, CRYPT_KEYOPT_CREATE)))
		goto failure_return_1;
	if (!DO("adding private key", ssl_keyset, cryptAddPrivateKey(ssl_keyset, cert, cfg->sys_pass)))
		goto failure_return_2;
	if (!DO("creating certificate", CRYPT_UNUSED, cryptCreateCert(&ssl_cert, CRYPT_UNUSED, CRYPT_CERTTYPE_CERTIFICATE)))
		goto failure_return_2;
	if (!DO("setting public key", ssl_cert, cryptSetAttribute(ssl_cert, CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, cert)))
		goto failure_return_3;
	if (!DO("signing certificate", ssl_cert, cryptSetAttribute(ssl_cert, CRYPT_CERTINFO_SELFSIGNED, 1)))
		goto failure_return_3;
	if (!DO("verifying certificate", ssl_cert, cryptSetAttribute(ssl_cert, CRYPT_OPTION_CERT_VALIDITY, 3650)))
		goto failure_return_3;
	if (!DO("setting country name", ssl_cert, cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_COUNTRYNAME, "ZZ", 2)))
		goto failure_return_3;
	if (!DO("setting organization name", ssl_cert, cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_ORGANIZATIONNAME, cfg->sys_name, strlen(cfg->sys_name))))
		goto failure_return_3;
	if (!DO("setting DNS name", ssl_cert, cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_DNSNAME, cfg->sys_inetaddr, strlen(cfg->sys_inetaddr))))
		goto failure_return_3;
	if (!DO("setting Common Name", ssl_cert, cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_COMMONNAME, cfg->sys_inetaddr, strlen(cfg->sys_inetaddr))))
		goto failure_return_3;
	sprintf(sysop_email, "sysop@%s", cfg->sys_inetaddr);
	if (!DO("setting email", ssl_cert, cryptSetAttributeString(ssl_cert, CRYPT_CERTINFO_RFC822NAME, sysop_email, strlen(sysop_email))))
		goto failure_return_3;
	if (!DO("signing certificate", ssl_cert, cryptSignCert(ssl_cert, cert)))
		goto failure_return_3;
	if (!DO("adding public key", ssl_keyset, cryptAddPublicKey(ssl_keyset, ssl_cert)))
		goto failure_return_3;
	cryptDestroyCert(ssl_cert);
	cryptKeysetClose(ssl_keyset);
	cryptDestroyContext(cert);
	ssl_new_epoch(cfg, lprintf);

	return true;

failure_return_3:
	cryptDestroyCert(ssl_cert);
failure_return_2:
	cryptKeysetClose(ssl_keyset);
failure_return_1:
	cryptDestroyContext(cert);
	cert_path[0] = 0;
	return false;
}

static struct cert_list * get_ssl_cert(scfg_t *cfg, int (*lprintf)(int level, const char* fmt, ...))
{
	CRYPT_KEYSET      ssl_keyset;
	struct cert_list *cert_entry;
	int               DOtmp;

	if (!do_cryptInit(lprintf))
		return NULL;
	if (!ssl_sync(cfg, lprintf))
		return NULL;
	cert_entry = malloc(sizeof(*cert_entry));
	if (cert_entry == NULL) {
		lprintf(LOG_CRIT, "%s line %d: FAILED TO ALLOCATE %u bytes of memory", __FUNCTION__, __LINE__, sizeof *cert_entry);
		return NULL;
	}
	cert_entry->sess = -1;
	cert_entry->next = NULL;
	cert_entry->cert = -1;

	size_t backoff_ms = 1;
	unsigned loops = 0;
	while (cert_entry->cert == -1) {
		assert_pthread_mutex_lock(&get_ssl_cert_mutex);
		/* Get the certificate... first try loading it from a file... */
		if (cryptStatusOK(cryptKeysetOpen(&ssl_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, cert_path, CRYPT_KEYOPT_READONLY))) {
			DO("getting private key", ssl_keyset, cryptGetPrivateKey(ssl_keyset, &cert_entry->cert, CRYPT_KEYID_NAME, "ssl_cert", cfg->sys_pass));
			cryptKeysetClose(ssl_keyset);
		}
		if (cert_entry->cert == -1) {
			lprintf(LOG_WARNING, "%s: Failed to open/read TLS certificate: %s", __FUNCTION__, cert_path);
			if (cfg->create_self_signed_cert) {
				// Only try to create cert first time through the loop.
				if (loops == 0) {
					if (create_self_signed_cert(cfg, lprintf)) {
						loops++;
						assert_pthread_mutex_unlock(&get_ssl_cert_mutex);
						continue;
					}
				}
			}
		}
		assert_pthread_mutex_unlock(&get_ssl_cert_mutex);
		// Backoff...
		loops++;
		// Total wait time is (1 << (total loops)) - 1 ms
		// ie: 14 loops is 16.383s
		if (loops > 14)
			break;
		SLEEP(backoff_ms);
		backoff_ms *= 2;
	}
	lprintf(LOG_DEBUG, "Total times through cert loop: %u (%ums)", loops, (1 << loops) - 1);

	if (cert_entry->cert == -1) {
		free(cert_entry);
		cert_path[0] = 0;
		cert_entry = NULL;
	}
	else {
		lprintf(LOG_DEBUG, "Created TLS private key and certificate %d", cert_entry->cert);
	}
	return cert_entry;
}
#undef DO

static struct cert_list *get_sess_list_entry(scfg_t *cfg, int (*lprintf)(int level, const char* fmt, ...), CRYPT_SESSION csess)
{
	struct cert_list *ret;

	if (!rwlock_rdlock(&cert_epoch_lock)) {
		lprintf(LOG_ERR, "Failed to lock cert_epoch_lock for read at %d", __LINE__);
		return NULL;
	}
	assert_pthread_mutex_lock(&ssl_cert_list_mutex);
	while (1) {
		if (cert_list == NULL) {
			assert_pthread_mutex_unlock(&ssl_cert_list_mutex);
			if (!rwlock_rdlock(&cert_epoch_lock)) {
				lprintf(LOG_ERR, "Failed to unlock cert_epoch_lock for read at %d", __LINE__);
			}
			return get_ssl_cert(cfg, lprintf);
		}
		ret = cert_list;
		cert_list = ret->next;
		if (ret->epoch == cert_epoch)
			break;
		cryptDestroyContext(ret->cert);
		free(ret);
	}
	assert_pthread_mutex_unlock(&ssl_cert_list_mutex);
	if (!rwlock_rdlock(&cert_epoch_lock)) {
		lprintf(LOG_ERR, "Failed to unlock cert_epoch_lock for read at %d", __LINE__);
	}
	return ret;
}

int add_private_key(scfg_t *cfg, int (*lprintf)(int level, const char* fmt, ...), CRYPT_SESSION csess)
{
	struct cert_list *sess;
	int               ret;

	sess = get_sess_list_entry(cfg, lprintf, csess);
	if (sess == NULL) {
		return CRYPT_ERROR_NOTINITED;
	}
	ret = cryptSetAttribute(csess, CRYPT_SESSINFO_PRIVATEKEY, sess->cert);
	if (cryptStatusOK(ret)) {
		assert_pthread_mutex_lock(&ssl_sess_list_mutex);
		sess->next = sess_list;
		sess_list = sess;
		assert_pthread_mutex_unlock(&ssl_sess_list_mutex);
		sess->sess = csess;
	}
	else {
		assert_pthread_mutex_lock(&ssl_cert_list_mutex);
		sess->next = cert_list;
		cert_list = sess;
		assert_pthread_mutex_unlock(&ssl_cert_list_mutex);
	}
	return ret;
}

int destroy_session(int (*lprintf)(int level, const char* fmt, ...), CRYPT_SESSION csess)
{
	struct cert_list *sess;
	struct cert_list *psess = NULL;
	int               ret = CRYPT_ERROR_NOTFOUND;

	assert_pthread_mutex_lock(&ssl_sess_list_mutex);
	sess = sess_list;
	while (sess != NULL) {
		if (sess->sess == csess) {
			if (psess == NULL) {
				sess_list = sess->next;
			}
			else {
				psess->next = sess->next;
			}
			break;
		}
		psess = sess;
		sess = sess->next;
	}
	assert_pthread_mutex_unlock(&ssl_sess_list_mutex);
	if (sess != NULL) {
		if (!rwlock_rdlock(&cert_epoch_lock)) {
			lprintf(LOG_ERR, "Unable to unlock cert_epoch_lock for write at %d", __LINE__);
			return CRYPT_ERROR_INTERNAL;
		}
		if (sess->epoch == cert_epoch) {
			if (!rwlock_unlock(&cert_epoch_lock)) {
				lprintf(LOG_ERR, "Unable to unlock cert_epoch_lock for write at %d", __LINE__);
				return CRYPT_ERROR_INTERNAL;
			}
			sess->sess = -1;
			assert_pthread_mutex_lock(&ssl_cert_list_mutex);
			sess->next = cert_list;
			cert_list = sess;
			assert_pthread_mutex_unlock(&ssl_cert_list_mutex);
			ret = cryptDestroySession(csess);
		}
		else {
			if (!rwlock_unlock(&cert_epoch_lock)) {
				lprintf(LOG_ERR, "Unable to unlock cert_epoch_lock for write at %d", __LINE__);
				return CRYPT_ERROR_INTERNAL;
			}
			// TODO: Failure here isn't logged
			cryptDestroyContext(sess->cert);
			free(sess);
			ret = cryptDestroySession(csess);
		}
	}
	else {
		lprintf(LOG_ERR, "Destroying a session (%d) that's not in sess_list at %d", csess, __LINE__);
	}
	if (ret == CRYPT_ERROR_NOTFOUND)
		ret = cryptDestroySession(csess);
	return ret;
}
