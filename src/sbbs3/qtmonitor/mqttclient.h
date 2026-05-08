#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QObject>
#include <QMqttClient>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QSslPreSharedKeyAuthenticator>

class MqttClient : public QObject
{
	Q_OBJECT

public:
	explicit MqttClient(QObject *parent = nullptr);

	void configure(const QString &host, quint16 port,
	               const QString &bbsId = {},
	               const QString &username = {}, const QString &password = {},
	               const QByteArray &pskIdentity = {}, const QByteArray &pskKey = {},
	               const QString &caFile = {}, const QString &certFile = {},
	               const QString &keyFile = {});
	void connectToBroker();
	void disconnectFromBroker();
	void ignoreSslErrors(const QList<QSslError> &errors);

	void recycleServer(const QString &server);
	void pauseServer(const QString &server);
	void resumeServer(const QString &server);
	void clearServer(const QString &server);
	void triggerEvent(const QString &code);
	void triggerCallout(const QString &hubId);
	void setNode(int nodeNum, const QString &prop, const QString &value);
	void sendNodeMessage(int nodeNum, const QString &message);

	bool isConnected() const;
	QString bbsId() const { return m_bbsId; }

signals:
	void connected();
	void disconnected();
	void errorOccurred(const QString &message);
	void sslErrorsOccurred(const QList<QSslError> &errors, const QStringList &descriptions);

	void logMessage(const QString &server, int level, const QString &timestamp, const QString &text);
	void eventLogMessage(int level, const QString &timestamp, const QString &text);
	void nodeStatus(int nodeNum, const QVariantMap &fields);
	void nodeVerbose(int nodeNum, const QString &description);
	void clientUpdate(const QString &server, const QString &action, const QVariantMap &fields);
	void serverState(const QString &server, const QString &state);
	void serverVersion(const QString &server, const QString &version);
	void serverStat(const QString &server, const QString &statName, const QString &value);
	void loginAttempt(const QString &ip, const QString &action, const QVariantMap &fields);

private slots:
	void onConnected();
	void onDisconnected();
	void onErrorChanged(QMqttClient::ClientError error);
	void onMessageReceived(const QByteArray &payload, const QMqttTopicName &topic);
	void onPskRequired(QSslPreSharedKeyAuthenticator *auth);

private:
	void initClient();
	void publish(const QString &topicSuffix, const QByteArray &payload);
	QString topicPrefix() const;
	void dispatchMessage(const QString &topic, const QString &text);
	int parseLogLevel(const QString &level) const;
	QPair<QString, QString> splitTsvPayload(const QString &text) const;

	QMqttClient *m_client;
	QSslSocket *m_socket;
	QString m_host;
	quint16 m_port = 0;
	QString m_bbsId;
	QString m_username;
	QString m_password;
	QByteArray m_pskIdentity;
	QByteArray m_pskKey;
	QString m_caFile;
	QString m_certFile;
	QString m_keyFile;
	QList<QSslError> m_ignoredSslErrors;
};

#endif
