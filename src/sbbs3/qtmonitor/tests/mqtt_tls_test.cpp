#include "mqttclient.h"

#include <QHostAddress>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QSslCipher>
#include <QSslDiffieHellmanParameters>
#include <QSslKey>
#include <QTcpServer>
#include <QTemporaryFile>
#include <QTest>

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

class PskServer
{
public:
	bool start()
	{
		if (!m_dhFile.open() || m_dhFile.write(DhParameters) != DhParameters.size()
		    || !m_dhFile.flush())
			return false;
		QTcpServer reservePort;
		if (!reservePort.listen(QHostAddress::LocalHost))
			return false;
		m_port = reservePort.serverPort();
		reservePort.close();
		m_process.setProcessChannelMode(QProcess::MergedChannels);
		m_process.start(QStandardPaths::findExecutable("openssl"), {
			"s_server", "-accept", QStringLiteral("127.0.0.1:%1").arg(m_port),
			"-nocert", "-tls1_2", "-cipher", Cipher + ":@SECLEVEL=1",
			"-dhparam", m_dhFile.fileName(), "-psk", QString::fromLatin1(Key.toHex()),
			"-psk_identity", QString::fromLatin1(Identity)
		});
		return m_process.waitForStarted(1000);
	}
	~PskServer()
	{
		if (m_process.state() != QProcess::NotRunning) {
			m_process.kill();
			m_process.waitForFinished(1000);
		}
	}
	bool ready()
	{
		m_output += m_process.readAll();
		return m_output.contains("ACCEPT");
	}
	quint16 serverPort() const { return m_port; }

private:
	QTemporaryFile m_dhFile;
	QProcess m_process;
	QByteArray m_output;
	quint16 m_port = 0;
};

class MqttTlsTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		if (!QSslSocket::supportsSsl() || QSslSocket::activeBackend() != "openssl")
			QSKIP("OpenSSL TLS backend required for DH security level tests");
		if (QStandardPaths::findExecutable("openssl").isEmpty())
			QSKIP("OpenSSL command line tool required for the local TLS server");
		QVERIFY(!QSslCipher(Cipher).isNull());
		QVERIFY(QSslDiffieHellmanParameters::fromEncoded(DhParameters).isValid());
	}

	void levelTwoRejectsCryptlibDhSize()
	{
		PskServer server;
		QVERIFY(server.start());
		QTRY_VERIFY_WITH_TIMEOUT(server.ready(), 5000);
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
		QVERIFY(server.start());
		QTRY_VERIFY_WITH_TIMEOUT(server.ready(), 5000);
		MqttClient client;
		client.configure("127.0.0.1", server.serverPort(), {}, {}, {}, Identity, key);
		client.connectToBroker();
		auto *socket = client.findChild<QSslSocket *>();
		QVERIFY(socket);
		QSignalSpy errors(&client, &MqttClient::errorOccurred);
		QTRY_VERIFY_WITH_TIMEOUT(socket->isEncrypted() || !errors.isEmpty(), 5000);
		QCOMPARE(socket->isEncrypted(), accepted);
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
