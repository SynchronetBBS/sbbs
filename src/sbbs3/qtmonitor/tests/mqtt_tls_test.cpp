#include "mqttclient.h"

#include <QHostAddress>
#include <QSignalSpy>
#include <QSslCipher>
#include <QSslDiffieHellmanParameters>
#include <QSslKey>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

#include <cstring>
#include <memory>

// RFC 3526 group 5, matching cryptlib's default PSK DH size. Public
// parameters only; test identities and keys never come from user settings.
static const QByteArray DhParameters = QByteArrayLiteral(
	"-----BEGIN DH PARAMETERS-----\n"
	"MIHHAoHBAP//////////yQ/aoiFowjTExmKLgNwc0SkCTgiKZ8x0Agu+pjsTmyJR\n"
	"Sgh5jjQE3e+VGbPNOkMbMCsKbfJfFDdP4TVtbVHCReSFtXZiXn7G9ExC6aY37WsL\n"
	"/1y29Aa37e44a/taiZ+lrp8kEXxLH+ZJKGZR7ORbPcIAfLihY78FmNpINhxV05pp\n"
	"Fj+o/STPX4NlXSPco62WHGLzViCFUrue1SkHcJaWbWcMNU5KvJgE8XRsCMojcyf/\n"
	"/////////wIBAg==\n"
	"-----END DH PARAMETERS-----\n");

static const QByteArray Identity = QByteArrayLiteral("qtmonitor-test");
static const QByteArray Key = QByteArrayLiteral("qtmonitor-test-password");
static const QString Cipher = QStringLiteral("DHE-PSK-AES128-CBC-SHA256");

class PskServer : public QTcpServer
{
public:
	PskServer()
	{
		connect(this, &QTcpServer::newConnection, this, [this] {
			auto *socket = nextPendingConnection();
			auto ssl = std::shared_ptr<SSL>(SSL_new(m_context.get()), SSL_free);
			BIO *input = BIO_new(BIO_s_mem());
			BIO *output = BIO_new(BIO_s_mem());
			if (!ssl || !input || !output) {
				BIO_free(input);
				BIO_free(output);
				socket->abort();
				return;
			}
			// Memory BIOs keep this fixture independent of native socket
			// descriptor types and let Qt drive the server without a thread.
			SSL_set_bio(ssl.get(), input, output);
			SSL_set_accept_state(ssl.get());
			connect(socket, &QTcpSocket::readyRead, socket, [socket, ssl] {
				const auto data = socket->readAll();
				if (SSL_is_init_finished(ssl.get()))
					return; // This fixture only needs the TLS handshake.
				BIO *input = SSL_get_rbio(ssl.get());
				if (BIO_write(input, data.constData(), int(data.size())) != data.size()) {
					socket->abort();
					return;
				}
				ERR_clear_error();
				const int result = SSL_do_handshake(ssl.get());
				const int error = result == 1 ? SSL_ERROR_NONE : SSL_get_error(ssl.get(), result);
				BIO *output = SSL_get_wbio(ssl.get());
				char buffer[4096];
				int count;
				while ((count = BIO_read(output, buffer, sizeof(buffer))) > 0)
					socket->write(buffer, count);
				if (error != SSL_ERROR_NONE && error != SSL_ERROR_WANT_READ
				    && error != SSL_ERROR_WANT_WRITE)
					socket->disconnectFromHost(); // Flush any fatal TLS alert.
			});
		});
	}

	bool start()
	{
		m_context.reset(SSL_CTX_new(TLS_server_method()));
		if (!m_context)
			return false;
		SSL_CTX_set_security_level(m_context.get(), 1);
		if (SSL_CTX_set_min_proto_version(m_context.get(), TLS1_2_VERSION) != 1
		    || SSL_CTX_set_max_proto_version(m_context.get(), TLS1_2_VERSION) != 1
		    || SSL_CTX_set_cipher_list(m_context.get(), Cipher.toLatin1().constData()) != 1)
			return false;
		auto bio = std::unique_ptr<BIO, decltype(&BIO_free)>(
			BIO_new_mem_buf(DhParameters.constData(), int(DhParameters.size())), BIO_free);
		if (!bio)
			return false;
		EVP_PKEY *parameters = PEM_read_bio_Parameters(bio.get(), nullptr);
		if (!parameters)
			return false;
		if (SSL_CTX_set0_tmp_dh_pkey(m_context.get(), parameters) != 1) {
			EVP_PKEY_free(parameters);
			return false;
		}
		SSL_CTX_set_psk_server_callback(m_context.get(),
			[](SSL *, const char *identity, unsigned char *key, unsigned int maxKeySize) -> unsigned int {
				if (!identity || Identity != identity || maxKeySize < Key.size())
					return 0;
				std::memcpy(key, Key.constData(), size_t(Key.size()));
				return unsigned(Key.size());
			});
		return listen(QHostAddress::LocalHost);
	}

private:
	std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> m_context{nullptr, SSL_CTX_free};
};

class MqttTlsTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		if (!QSslSocket::supportsSsl() || QSslSocket::activeBackend() != "openssl")
			QSKIP("OpenSSL TLS backend required for DH security level tests");
		QVERIFY(!QSslCipher(Cipher).isNull());
		QVERIFY(QSslDiffieHellmanParameters::fromEncoded(DhParameters).isValid());
	}

	void levelTwoRejectsCryptlibDhSize()
	{
		PskServer server;
		QVERIFY2(server.start(), ERR_error_string(ERR_peek_last_error(), nullptr));
		QSslSocket socket;
		auto config = socket.sslConfiguration();
		config.setProtocol(QSsl::TlsV1_2);
		config.setPeerVerifyMode(QSslSocket::VerifyNone);
		config.setBackendConfigurationOption("CipherString", (Cipher + ":@SECLEVEL=2").toLatin1());
		socket.setSslConfiguration(config);
		connect(&socket, &QSslSocket::preSharedKeyAuthenticationRequired, &socket,
			[](QSslPreSharedKeyAuthenticator *auth) {
				auth->setIdentity(Identity);
				auth->setPreSharedKey(Key);
			});
		QSignalSpy errors(&socket, &QAbstractSocket::errorOccurred);
		socket.connectToHostEncrypted("127.0.0.1", server.serverPort());
		QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(), 5000);
		QVERIFY2(socket.errorString().contains("dh key too small"), qPrintable(socket.errorString()));
		QVERIFY(!socket.isEncrypted());
	}

	void pskHandshake_data()
	{
		QTest::addColumn<QByteArray>("key");
		QTest::addColumn<bool>("accepted");
		QTest::newRow("correct-key") << Key << true;
		QTest::newRow("wrong-key") << QByteArray("wrong-password") << false;
	}

	void pskHandshake()
	{
		QFETCH(QByteArray, key);
		QFETCH(bool, accepted);
		const auto defaults = QSslConfiguration::defaultConfiguration();
		PskServer server;
		QVERIFY2(server.start(), ERR_error_string(ERR_peek_last_error(), nullptr));
		MqttClient client;
		client.configure("127.0.0.1", server.serverPort(), {}, {}, {}, Identity, key);
		client.connectToBroker();
		auto *socket = client.findChild<QSslSocket *>();
		QVERIFY(socket);
		QSignalSpy errors(&client, &MqttClient::errorOccurred);
		QTRY_VERIFY_WITH_TIMEOUT(socket->isEncrypted() || !errors.isEmpty(), 5000);
		QVERIFY2(socket->isEncrypted() == accepted, qPrintable(socket->errorString()));
		if (accepted) {
			QCOMPARE(socket->sslConfiguration().sessionCipher().name(), Cipher);
			QCOMPARE(socket->sslConfiguration().ephemeralServerKey().length(), 1536);
		} else {
			QVERIFY(!errors.isEmpty());
		}
		client.disconnectFromBroker();
		QCOMPARE(QSslConfiguration::defaultConfiguration(), defaults);
	}
};

QTEST_GUILESS_MAIN(MqttTlsTest)
#include "mqtt_tls_test.moc"
