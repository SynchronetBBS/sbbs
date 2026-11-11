/* Standalone Unix regression tests for the certificate cache in ssl.c.
 * Uses real cryptlib keysets, contexts, sessions, and pthread locks.
 * From src/, with built cryptlib and xpdev libraries:
 *   cc -std=gnu11 -Ixpdev -Ismblib -Ihash -Iencode -Iconio -I"$CRYPTDIR" \
 *     sbbs3/ssl_test.c "$XPLIB" "$CRYPTDIR/libcl.a" -pthread -lm -o /tmp/ssl_test
 *   /tmp/ssl_test
 * CRYPTDIR is the cryptlib build directory; XPLIB is libxpdev_mt.a.
 * Do not also compile ssl.c or add this test to the server's objects.
 */

#include <stdarg.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ssl.c"

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
		exit(1); \
	} \
} while (0)

static scfg_t cfg;
static char replacement[sizeof(cert_path)];

static int test_log(int level, const char *fmt, ...)
{
	if (level <= LOG_ERR) {
		va_list args;
		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
		fputc('\n', stderr);
	}
	return 0;
}

static void *try_epoch_write(void *arg)
{
	bool *unlocked = arg;
	*unlocked = rwlock_trywrlock(&cert_epoch_lock);
	if (*unlocked)
		CHECK(rwlock_unlock(&cert_epoch_lock));
	return NULL;
}

static void check_epoch_unlocked(void)
{
	pthread_t thread;
	bool unlocked = false;
	CHECK(pthread_create(&thread, NULL, try_epoch_write, &unlocked) == 0);
	CHECK(pthread_join(thread, NULL) == 0);
	CHECK(unlocked);
}

static CRYPT_SESSION new_session(void)
{
	CRYPT_SESSION session;
	CHECK(do_cryptInit(test_log));
	CHECK(cryptCreateSession(&session, CRYPT_UNUSED, CRYPT_SESSION_TLS_SERVER) == CRYPT_OK);
	CHECK(add_private_key(&cfg, test_log, session) == CRYPT_OK);
	return session;
}

static void fingerprint(CRYPT_CONTEXT cert, unsigned char *hash)
{
	int len;
	CHECK(cryptGetAttributeString(cert, CRYPT_CERTINFO_FINGERPRINT_SHA1, hash, &len) == CRYPT_OK);
	CHECK(len == 20);
}

static void make_fixture(void)
{
	CHECK(ssl_sync(&cfg, test_log));
	CHECK(create_self_signed_cert(&cfg, test_log));
}

static void initial_date(void)
{
	CHECK(ssl_sync(&cfg, test_log));
	CHECK(tls_cert_file_date == fdate(cert_path));
	unsigned epoch = cert_epoch;
	CHECK(ssl_sync(&cfg, test_log));
	CHECK(cert_epoch == epoch);
}

static void cache_lifecycle(void)
{
	CHECK(ssl_sync(&cfg, test_log));
	CRYPT_SESSION first = new_session();
	check_epoch_unlocked(); /* Empty-cache path. */
	struct cert_list *entry = sess_list;
	CHECK(entry != NULL);
	CHECK(entry->epoch == cert_epoch);
	CHECK(destroy_session(test_log, first) == CRYPT_OK);
	CHECK(cert_list == entry);
	CRYPT_SESSION second = new_session();
	CHECK(sess_list == entry); /* Reused without reloading the keyset. */
	CHECK(cert_list == NULL);
	check_epoch_unlocked(); /* Cached path. */
	CHECK(destroy_session(test_log, second) == CRYPT_OK);
	CHECK(ssl_new_epoch(&cfg, test_log));
	CHECK(cert_list == NULL);
}

static void self_signed(void)
{
	char fresh[sizeof(cfg.ctrl_dir)];
	SAFEPRINTF(fresh, "%sfresh/", cfg.ctrl_dir);
	CHECK(mkdir(fresh, 0700) == 0);
	SAFECOPY(cfg.ctrl_dir, fresh);
	CHECK(ssl_sync(&cfg, test_log));
	CRYPT_SESSION session = new_session();
	CHECK(fdate(cert_path) != (time_t)-1);
	CHECK(sess_list->epoch == cert_epoch);
	check_epoch_unlocked();
	CHECK(destroy_session(test_log, session) == CRYPT_OK);
	CHECK(cert_list != NULL);
	CHECK(ssl_new_epoch(&cfg, test_log));
	CHECK(unlink(cert_path) == 0);
	CHECK(rmdir(fresh) == 0);
}

static void missing_client_cert(void)
{
	char empty[sizeof(cfg.ctrl_dir)];
	SAFEPRINTF(empty, "%sempty/", cfg.ctrl_dir);
	CHECK(mkdir(empty, 0700) == 0);
	SAFECOPY(cfg.ctrl_dir, empty);
	cfg.create_self_signed_cert = false;
	CHECK(ssl_sync(&cfg, test_log));
	CRYPT_SESSION session;
	CHECK(cryptCreateSession(&session, CRYPT_UNUSED, CRYPT_SESSION_TLS) == CRYPT_OK);
	CHECK(cryptDestroySession(session) == CRYPT_OK);
	CHECK(cert_list == NULL);
	CHECK(fdate(cert_path) == (time_t)-1);
	CHECK(rmdir(empty) == 0);
}

static void renewal(void)
{
	unsigned char old_hash[20], hash[20];
	CHECK(ssl_sync(&cfg, test_log));
	CRYPT_SESSION active = new_session();
	CRYPT_CONTEXT old_cert = sess_list->cert;
	fingerprint(old_cert, old_hash);
	CRYPT_SESSION idle = new_session();
	CHECK(destroy_session(test_log, idle) == CRYPT_OK);
	CHECK(cert_list != NULL);
	/* Simulate letsyncrypt's replacement followed by server recycle.
	 * Force a different mtime without relying on wall-clock sleeps. */
	CHECK(setfdate(replacement, fdate(cert_path) + 10) == 0);
	CHECK(rename(replacement, cert_path) == 0);
	CHECK(ssl_sync(&cfg, test_log));
	CHECK(cert_list == NULL);
	fingerprint(old_cert, hash);
	CHECK(memcmp(old_hash, hash, sizeof(hash)) == 0); /* Active session survives. */
	CRYPT_SESSION renewed = new_session();
	fingerprint(sess_list->cert, hash);
	CHECK(memcmp(old_hash, hash, sizeof(hash)) != 0);
	CHECK(destroy_session(test_log, active) == CRYPT_OK);
	CHECK(cert_list == NULL); /* Old active certificate wasn't cached. */
	CHECK(destroy_session(test_log, renewed) == CRYPT_OK);
	CHECK(cert_list != NULL);
	CHECK(ssl_new_epoch(&cfg, test_log));
}

static bool run(const char *name, void (*test)(void))
{
	fflush(NULL);
	pid_t child = fork();
	CHECK(child >= 0);
	if (child == 0) {
		alarm(30); /* Report a deadlock as a failed test. */
		test();
		exit(0);
	}
	int status;
	CHECK(waitpid(child, &status, 0) == child);
	bool passed = WIFEXITED(status) && WEXITSTATUS(status) == 0;
	printf("%s: %s", name, passed ? "PASS" : "FAIL");
	if (WIFSIGNALED(status))
		printf(" (signal %d)", WTERMSIG(status));
	putchar('\n');
	return passed;
}

int main(void)
{
	char dir[] = "/tmp/sbbs-ssl-test.XXXXXX";
	CHECK(mkdtemp(dir) != NULL);
	SAFEPRINTF(cfg.ctrl_dir, "%s/", dir);
	SAFECOPY(cfg.sys_pass, "ssl-regression-test");
	SAFECOPY(cfg.sys_name, "SSL regression test");
	SAFECOPY(cfg.sys_inetaddr, "ssl-test.invalid");
	cfg.create_self_signed_cert = true;
	CHECK(run("create existing fixture", make_fixture));
	SAFEPRINTF(replacement, "%sreplacement.cert", cfg.ctrl_dir);
	SAFECOPY(cert_path, replacement);
	CHECK(run("create replacement fixture", make_fixture));
	cert_path[0] = 0;
	bool passed = run("initial date and unchanged sync", initial_date);
	passed = run("cache lifecycle and balanced locks", cache_lifecycle) && passed;
	passed = run("self-signed first session", self_signed) && passed;
	passed = run("TLS client without local certificate", missing_client_cert) && passed;
	passed = run("replacement with active and cached sessions", renewal) && passed;
	char path[sizeof(cert_path)];
	SAFEPRINTF(path, "%sssl.cert", cfg.ctrl_dir);
	CHECK(unlink(path) == 0);
	/* Failed children may leave their fixture files behind. */
	unlink(replacement);
	SAFEPRINTF(path, "%sfresh/ssl.cert", cfg.ctrl_dir);
	unlink(path);
	SAFEPRINTF(path, "%sfresh/", cfg.ctrl_dir);
	rmdir(path);
	SAFEPRINTF(path, "%sempty/", cfg.ctrl_dir);
	rmdir(path);
	CHECK(rmdir(dir) == 0);
	return passed ? 0 : 1;
}
