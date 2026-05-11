#include "mqttclient.h"
#include <QDateTime>
#include <QMqttSubscription>
#include <QTimer>
#include <QMqttTopicFilter>
#include <QSslConfiguration>
#include <QSslCipher>
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>

MqttClient::MqttClient(QObject *parent)
	: QObject(parent)
	, m_client(nullptr)
	, m_socket(nullptr)
{
}

void MqttClient::initClient()
{
	m_client = new QMqttClient(this);
	m_client->setProtocolVersion(QMqttClient::MQTT_5_0);
	connect(m_client, &QMqttClient::connected, this, &MqttClient::onConnected);
	connect(m_client, &QMqttClient::disconnected, this, &MqttClient::onDisconnected);
	connect(m_client, &QMqttClient::errorChanged, this, &MqttClient::onErrorChanged);
}

void MqttClient::configure(const QString &host, quint16 port,
                           const QString &bbsId,
                           const QString &username, const QString &password,
                           const QByteArray &pskIdentity, const QByteArray &pskKey,
                           const QString &caFile, const QString &certFile,
                           const QString &keyFile)
{
	m_host = host;
	m_port = port;
	m_bbsId = bbsId.isEmpty() ? QString() : bbsId;
	m_username = username;
	m_password = password;
	m_pskIdentity = pskIdentity;
	m_pskKey = pskKey;
	m_caFile = caFile;
	m_certFile = certFile;
	m_keyFile = keyFile;
}

void MqttClient::connectToBroker()
{
	disconnectFromBroker();
	initClient();

	if (!m_username.isEmpty()) {
		m_client->setUsername(m_username);
		m_client->setPassword(m_password);
	} else if (!m_password.isEmpty() && !m_pskIdentity.isEmpty()) {
		m_client->setUsername(QString::fromUtf8(m_pskIdentity));
		m_client->setPassword(m_password);
	}

	bool usePsk = !m_pskIdentity.isEmpty() && !m_pskKey.isEmpty();
	bool useTls = usePsk || !m_caFile.isEmpty();

	if (useTls) {
		m_socket = new QSslSocket(this);
		if (usePsk)
			connect(m_socket, &QSslSocket::preSharedKeyAuthenticationRequired,
			        this, &MqttClient::onPskRequired);
		if (!m_ignoredSslErrors.isEmpty())
			m_socket->ignoreSslErrors(m_ignoredSslErrors);
		connect(m_socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &errors) {
			m_socket->ignoreSslErrors(errors);
			QList<QSslError> newErrors;
			for (const auto &e : errors)
				if (!m_ignoredSslErrors.contains(e))
					newErrors << e;
			if (newErrors.isEmpty())
				return;
			QStringList msgs;
			for (const auto &e : newErrors)
				msgs << e.errorString();
			QMetaObject::invokeMethod(this, [this, newErrors, msgs] {
				emit sslErrorsOccurred(newErrors, msgs);
			}, Qt::QueuedConnection);
		});
		connect(m_socket, &QAbstractSocket::errorOccurred, this, [this] {
			emit errorOccurred("Socket error: " + m_socket->errorString());
		});
		connect(m_socket, &QSslSocket::encrypted, this, [this] {
			m_client->setTransport(m_socket, QMqttClient::IODevice);
			m_client->setHostname(m_host);
			m_client->setPort(m_port);
			m_client->connectToHost();
		});

		QSslConfiguration config = m_socket->sslConfiguration();

		if (!m_caFile.isEmpty()) {
			QList<QSslCertificate> caCerts = QSslCertificate::fromPath(m_caFile);
			if (!caCerts.isEmpty()) {
				config.setCaCertificates(caCerts);
				config.setPeerVerifyMode(QSslSocket::VerifyPeer);
			} else {
				emit errorOccurred("Failed to load CA certificate: " + m_caFile);
				config.setPeerVerifyMode(QSslSocket::VerifyNone);
			}
		} else {
			config.setPeerVerifyMode(QSslSocket::VerifyNone);
		}

		if (!m_certFile.isEmpty() && !m_keyFile.isEmpty()) {
			QList<QSslCertificate> certs = QSslCertificate::fromPath(m_certFile);
			if (!certs.isEmpty())
				config.setLocalCertificate(certs.first());
			else
				emit errorOccurred("Failed to load client certificate: " + m_certFile);

			QFile keyFile(m_keyFile);
			if (keyFile.open(QIODevice::ReadOnly)) {
				QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);
				if (key.isNull()) {
					keyFile.seek(0);
					key = QSslKey(&keyFile, QSsl::Ec, QSsl::Pem);
				}
				if (!key.isNull())
					config.setPrivateKey(key);
				else
					emit errorOccurred("Failed to load private key: " + m_keyFile);
			} else {
				emit errorOccurred("Cannot open key file: " + m_keyFile);
			}
		}

		if (usePsk) {
			config.setProtocol(QSsl::TlsV1_2);
			QList<QSslCipher> pskCiphers;
			for (const auto &c : QSslConfiguration::supportedCiphers())
				if (c.name().contains("PSK"))
					pskCiphers << c;
			if (!pskCiphers.isEmpty())
				config.setCiphers(pskCiphers);
		}

		m_socket->setSslConfiguration(config);
		m_socket->connectToHostEncrypted(m_host, m_port);
	} else {
		m_client->setHostname(m_host);
		m_client->setPort(m_port);
		m_client->connectToHost();
	}
}

void MqttClient::disconnectFromBroker()
{
	if (m_client) {
		if (m_client->state() != QMqttClient::Disconnected)
			m_client->disconnectFromHost();
		delete m_client;
		m_client = nullptr;
	}
	if (m_socket) {
		m_socket->abort();
		delete m_socket;
		m_socket = nullptr;
	}
}

void MqttClient::ignoreSslErrors(const QList<QSslError> &errors)
{
	m_ignoredSslErrors += errors;
}

void MqttClient::onPskRequired(QSslPreSharedKeyAuthenticator *auth)
{
	auth->setIdentity(m_pskIdentity);
	auth->setPreSharedKey(m_pskKey);
}

void MqttClient::onConnected()
{
	QString prefix = topicPrefix();
	const QStringList topics = {
		prefix + "/host/+/server/+/log/#",
		prefix + "/log/#",
		prefix + "/node/#",
		prefix + "/host/+/server/+/client/action/#",
		prefix + "/host/+/server/+/client",
		prefix + "/host/+/server/+",
		prefix + "/host/+/server/+/version",
		prefix + "/host/+/server/+/served",
		prefix + "/host/+/server/+/highwater",
		prefix + "/host/+/server/+/error_count",
		prefix + "/action/#",
		prefix + "/host/+/event/log/#",
		prefix + "/host/+/login_attempts/#",
		prefix + "/host/+/server/+/max_concurrent/#",
		prefix,
	};
	for (const auto &t : topics) {
		auto *sub = m_client->subscribe(QMqttTopicFilter(t));
		if (sub)
			connect(sub, &QMqttSubscription::messageReceived,
			        this, &MqttClient::onSubscriptionMessage);
	}
	auto *sysSub = m_client->subscribe(QMqttTopicFilter("$SYS/#"));
	if (sysSub)
		connect(sysSub, &QMqttSubscription::messageReceived,
		        this, &MqttClient::onSubscriptionMessage);

	QString clientListFilter = prefix + "/host/+/server/+/client/list";
	auto *clientListSub = m_client->subscribe(QMqttTopicFilter(clientListFilter));
	if (clientListSub) {
		connect(clientListSub, &QMqttSubscription::messageReceived,
		        this, &MqttClient::onSubscriptionMessage);
		QTimer::singleShot(2000, this, [this, clientListFilter] {
			if (m_client)
				m_client->unsubscribe(QMqttTopicFilter(clientListFilter));
		});
	}

	emit connected();
}

void MqttClient::onDisconnected()
{
	emit disconnected();
}

void MqttClient::onErrorChanged(QMqttClient::ClientError error)
{
	if (error != QMqttClient::NoError)
		emit errorOccurred(QStringLiteral("MQTT error: %1").arg(static_cast<int>(error)));
}

void MqttClient::onSubscriptionMessage(const QMqttMessage &msg)
{
	QHash<QString, QString> userProps;
	const auto props = msg.publishProperties().userProperties();
	for (const auto &p : props)
		userProps.insert(p.name(), p.value());
	dispatchMessage(msg.topic().name(), QString::fromUtf8(msg.payload()), userProps);
}

bool MqttClient::isConnected() const
{
	return m_client && m_client->state() == QMqttClient::Connected;
}

void MqttClient::publish(const QString &topicSuffix, const QByteArray &payload)
{
	if (!m_bbsId.isEmpty() && isConnected())
		m_client->publish(QMqttTopicName("sbbs/" + m_bbsId + "/" + topicSuffix), payload);
}

QString MqttClient::topicPrefix() const
{
	return m_bbsId.isEmpty() ? QStringLiteral("sbbs/+") : QStringLiteral("sbbs/") + m_bbsId;
}

void MqttClient::recycleHost(const QString &host)                          { publish("host/" + host + "/recycle", {}); }
void MqttClient::pauseHost(const QString &host)                            { publish("host/" + host + "/pause", {}); }
void MqttClient::resumeHost(const QString &host)                           { publish("host/" + host + "/resume", {}); }
void MqttClient::clearHost(const QString &host)                            { publish("host/" + host + "/clear", {}); }
void MqttClient::recycleServer(const QString &host, const QString &server) { publish("host/" + host + "/server/" + server + "/recycle", {}); }
void MqttClient::pauseServer(const QString &host, const QString &server)   { publish("host/" + host + "/server/" + server + "/pause", {}); }
void MqttClient::resumeServer(const QString &host, const QString &server)  { publish("host/" + host + "/server/" + server + "/resume", {}); }
void MqttClient::clearServer(const QString &host, const QString &server)    { publish("host/" + host + "/server/" + server + "/clear", {}); }
void MqttClient::clearLoginAttempt(const QString &host, const QString &ip)  { publish("host/" + host + "/clear", ip.toUtf8()); }
void MqttClient::triggerEvent(const QString &code)     { publish("exec", code.toUtf8()); }
void MqttClient::triggerCallout(const QString &hubId)  { publish("call", hubId.toUtf8()); }
void MqttClient::setNode(int n, const QString &prop, const QString &val) { publish(QStringLiteral("node/%1/set/%2").arg(n).arg(prop), val.toUtf8()); }
void MqttClient::sendNodeMessage(int n, const QString &msg) { publish(QStringLiteral("node/%1/msg").arg(n), msg.toUtf8()); }

static QString formatTimestamp(const QString &raw)
{
	if (raw.isEmpty())
		return raw;
	QDateTime dt = QDateTime::fromString(raw, Qt::ISODate);
	if (!dt.isValid())
		dt = QDateTime::fromString(raw.left(15), "yyyyMMdd'T'HHmmss");
	return dt.isValid() ? dt.toString("MMM dd hh:mm:ss") : raw;
}

void MqttClient::dispatchMessage(const QString &topic, const QString &text,
                                  const QHash<QString, QString> &userProps)
{
	QStringList parts = topic.split('/');

	// $SYS/broker/version
	if (topic == "$SYS/broker/version") {
		emit brokerVersion(text);
		return;
	}
	// $SYS/broker/log/{level}
	if (parts.size() == 4 && parts[0] == "$SYS" && parts[1] == "broker" && parts[2] == "log") {
		auto [timestamp, msg] = splitTsvPayload(text, userProps);
		emit brokerLog(parseLogLevel(parts[3]), timestamp, msg);
		return;
	}

	if (parts.size() >= 2 && parts[0] == "sbbs" && m_bbsId.isEmpty())
		m_bbsId = parts[1];

	// sbbs/{id} — BBS system name
	if (parts.size() == 2 && parts[0] == "sbbs" && !text.isEmpty()) {
		emit bbsName(text);
		return;
	}

	if (parts.size() >= 4 && parts[2] == "host" && !m_hosts.contains(parts[3])) {
		m_hosts.append(parts[3]);
		emit hostDiscovered(parts[3]);
	}

	QString host = (parts.size() >= 4 && parts[2] == "host") ? parts[3] : QString();

	// sbbs/{id}/host/{h}/server/{srv}/log/{level}
	if (parts.size() >= 8 && parts[4] == "server" && parts[6] == "log") {
		auto [timestamp, msg] = splitTsvPayload(text, userProps);
		emit logMessage(host, parts[5], parseLogLevel(parts.last()), timestamp, msg);
		return;
	}

	// sbbs/{id}/action/{type}/{detail...} — payload is timestamp\tdata
	if (parts.size() >= 4 && parts[2] == "action") {
		QString timestamp, payload;
		auto it = userProps.constFind(QStringLiteral("time"));
		if (it != userProps.constEnd()) {
			timestamp = formatTimestamp(it.value());
			payload = text;
		} else {
			int tab = text.indexOf('\t');
			if (tab >= 0) {
				timestamp = formatTimestamp(text.left(tab));
				payload = text.mid(tab + 1);
			} else {
				payload = text;
			}
		}
		QString action = parts[3];
		QString detail = QStringList(parts.mid(4)).join('/');
		emit bbsAction(action, detail, timestamp, payload);
		return;
	}

	// sbbs/{id}/host/{h}/event/log/{level}
	if (parts.size() >= 7 && parts[4] == "event" && parts[5] == "log") {
		auto [timestamp, msg] = splitTsvPayload(text, userProps);
		emit eventLogMessage(host, parseLogLevel(parts.last()), timestamp, msg);
		return;
	}

	// sbbs/{id}/log/{level}
	if (parts.size() >= 4 && parts[2] == "log") {
		auto [timestamp, msg] = splitTsvPayload(text, userProps);
		emit logMessage(host, "bbs", parseLogLevel(parts.last()), timestamp, msg);
		return;
	}

	// sbbs/{id}/node/{n}/status (structured)
	if (parts.size() >= 5 && parts[2] == "node" && parts[4] == "status") {
		bool ok;
		int nodeNum = parts[3].toInt(&ok);
		if (!ok) return;
		QStringList fields = text.split('\t');
		QStringList names = {"status", "action", "user", "connection", "misc", "aux", "extaux", "errors"};
		QVariantMap data;
		for (int i = 0; i < qMin(fields.size(), names.size()); ++i)
			data[names[i]] = fields[i];
		emit nodeStatus(host, nodeNum, data);
		return;
	}

	// sbbs/{id}/node/{n} (verbose)
	if (parts.size() == 4 && parts[2] == "node") {
		bool ok;
		int nodeNum = parts[3].toInt(&ok);
		if (!ok) return;
		emit nodeVerbose(host, nodeNum, text);
		return;
	}

	// sbbs/{id}/host/{h}/server/{srv}/client/list
	if (parts.size() == 8 && parts[4] == "server" && parts[6] == "client" && parts[7] == "list") {
		QStringList names = {"timestamp", "protocol", "usernum", "username", "ip", "hostname", "port", "socket"};
		for (const auto &line : text.split('\n', Qt::SkipEmptyParts)) {
			QStringList fields = line.split('\t');
			QVariantMap data;
			for (int i = 0; i < qMin(fields.size(), names.size()); ++i)
				data[names[i]] = fields[i];
			emit clientUpdate(host, parts[5], "connect", data);
		}
		return;
	}

	// sbbs/{id}/host/{h}/server/{srv}/client/action/{act}
	if (parts.size() >= 9 && parts[4] == "server" && parts[6] == "client" && parts[7] == "action") {
		QStringList fields = text.split('\t');
		QStringList names = {"timestamp", "protocol", "usernum", "username", "ip", "hostname", "port", "socket"};
		QVariantMap data;
		for (int i = 0; i < qMin(fields.size(), names.size()); ++i)
			data[names[i]] = fields[i];
		emit clientUpdate(host, parts[5], parts[8], data);
		return;
	}

	// sbbs/{id}/host/{h}/server/{srv}/state/{state} — ignored, use server-level topic
	if (parts.size() >= 8 && parts[4] == "server" && parts[6] == "state")
		return;

	// sbbs/{id}/host/{h}/login_attempts/{ip}
	if (parts.size() >= 6 && parts[4] == "login_attempts") {
		QString ip = QStringList(parts.mid(5)).join('/');
		if (text.isEmpty()) {
			emit loginAttempt(host, ip, "clear", {});
		} else {
			QStringList fields = text.split('\t');
			QStringList names = {"first", "last", "count", "dupes", "protocol", "username"};
			QVariantMap data;
			for (int i = 0; i < qMin(fields.size(), names.size()); ++i)
				data[names[i]] = fields[i];
			emit loginAttempt(host, ip, "update", data);
		}
		return;
	}

	// sbbs/{id}/host/{h}/server/{srv}/max_concurrent/{ip}
	if (parts.size() >= 8 && parts[4] == "server" && parts[6] == "max_concurrent") {
		QString ip = QStringList(parts.mid(7)).join('/');
		if (text.isEmpty())
			emit maxConcurrent(host, parts[5], ip, "clear", 0);
		else
			emit maxConcurrent(host, parts[5], ip, "update", text.toInt());
		return;
	}

	// sbbs/{id}/host/{h}/server/{srv}/client (count)
	if (parts.size() == 7 && parts[4] == "server" && parts[6] == "client") {
		emit serverStat(host, parts[5], "clients", text);
		return;
	}

	// sbbs/{id}/host/{h}/server/{srv} (server-level status)
	if (parts.size() == 6 && parts[4] == "server") {
		emit serverState(host, parts[5], text.split('\t').first());
		return;
	}

	// sbbs/{id}/host/{h}/server/{srv}/version
	if (parts.size() == 7 && parts[4] == "server" && parts[6] == "version") {
		emit serverVersion(host, parts[5], text);
		return;
	}

	// sbbs/{id}/host/{h}/server/{srv}/{stat}
	if (parts.size() == 7 && parts[4] == "server"
	    && (parts[6] == "served" || parts[6] == "highwater" || parts[6] == "error_count")) {
		emit serverStat(host, parts[5], parts[6], text);
		return;
	}
}

int MqttClient::parseLogLevel(const QString &level) const
{
	bool ok;
	int n = level.toInt(&ok);
	if (ok) return n;
	static const QHash<QString, int> map = {
		{"emergency", 0}, {"alert", 1}, {"critical", 2}, {"error", 3},
		{"warn", 4}, {"warning", 4}, {"notice", 5}, {"info", 6}, {"debug", 7},
	};
	return map.value(level, 6);
}

QPair<QString, QString> MqttClient::splitTsvPayload(const QString &text,
                                                     const QHash<QString, QString> &userProps) const
{
	auto it = userProps.constFind(QStringLiteral("time"));
	if (it != userProps.constEnd())
		return {formatTimestamp(it.value()), text};
	return {{}, text};
}
