#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setApplicationName("Synchronet Monitor");
	app.setOrganizationName("Synchronet");
	app.setWindowIcon(QIcon(":/sync.ico"));

	QCommandLineParser parser;
	parser.setApplicationDescription("Synchronet BBS Monitor");
	parser.addHelpOption();
	parser.addOption({{"m", "mqtt-host"}, "MQTT broker hostname", "HOST"});
	parser.addOption({{"p", "mqtt-port"}, "MQTT broker port", "PORT"});
	parser.addOption({{"i", "bbs-id"}, "BBS system ID for MQTT topics", "ID"});
	parser.addOption({{"u", "mqtt-user"}, "MQTT broker username", "USER"});
	parser.addOption({{"P", "mqtt-pass"}, "MQTT password (system password for broker.js)", "PASS"});
	parser.addOption({"psk-id", "TLS-PSK identity (sysop alias, lowercased)", "IDENTITY"});
	parser.addOption({"psk-key", "TLS-PSK key (sysop password, lowercased)", "KEY"});
	parser.addOption({"ca-file", "CA certificate file for server verification", "FILE"});
	parser.addOption({"cert-file", "Client certificate file", "FILE"});
	parser.addOption({"key-file", "Client private key file", "FILE"});
	parser.process(app);

	MainWindow w(
		parser.value("mqtt-host"),
		parser.value("mqtt-port").toUShort(),
		parser.value("bbs-id"),
		parser.value("mqtt-user"),
		parser.value("mqtt-pass"),
		parser.value("psk-id"),
		parser.value("psk-key"),
		parser.value("ca-file"),
		parser.value("cert-file"),
		parser.value("key-file")
	);
	w.show();

	return app.exec();
}
