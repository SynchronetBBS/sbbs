#include "settingsdialog.h"
#include "logwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QFileDialog>

SettingsDialog::SettingsDialog(QSettings *settings, QWidget *parent)
	: QDialog(parent), m_settings(settings)
{
	setWindowTitle("Settings");
	setMinimumWidth(350);

	auto *layout = new QVBoxLayout(this);

	auto *mqttGroup = new QGroupBox("MQTT Broker");
	auto *form = new QFormLayout(mqttGroup);

	m_host = new QLineEdit(m_settings->value("mqtt/host").toString());
	m_host->setPlaceholderText("(required)");
	form->addRow("Host:", m_host);

	m_port = new QSpinBox;
	m_port->setRange(0, 65535);
	m_port->setSpecialValueText("(required)");
	m_port->setValue(m_settings->value("mqtt/port", 0).toInt());
	form->addRow("Port:", m_port);

	m_bbsId = new QLineEdit(m_settings->value("mqtt/bbs_id").toString());
	m_bbsId->setPlaceholderText("(auto-detect)");
	form->addRow("BBS ID:", m_bbsId);

	m_username = new QLineEdit(m_settings->value("mqtt/username").toString());
	m_username->setPlaceholderText("(optional)");
	form->addRow("Username:", m_username);

	m_password = new QLineEdit(m_settings->value("mqtt/password").toString());
	m_password->setPlaceholderText("(optional)");
	m_password->setEchoMode(QLineEdit::Password);
	form->addRow("Password:", m_password);

	m_publishQos = new QComboBox;
	m_publishQos->addItem("0 — At most once", 0);
	m_publishQos->addItem("2 — Exactly once", 2);
	int savedQos = m_settings->value("mqtt/publish_qos", 0).toInt();
	m_publishQos->setCurrentIndex(savedQos == 2 ? 1 : 0);
	form->addRow("Publish QoS:", m_publishQos);

	layout->addWidget(mqttGroup);

	auto *pskGroup = new QGroupBox("TLS-PSK (for broker.js)");
	auto *pskForm = new QFormLayout(pskGroup);

	m_pskIdentity = new QLineEdit(m_settings->value("mqtt/psk_identity").toString());
	m_pskIdentity->setPlaceholderText("sysop alias (lowercased)");
	pskForm->addRow("PSK Identity:", m_pskIdentity);

	m_pskKey = new QLineEdit(m_settings->value("mqtt/psk_key").toString());
	m_pskKey->setPlaceholderText("sysop password (lowercased)");
	m_pskKey->setEchoMode(QLineEdit::Password);
	pskForm->addRow("PSK Key:", m_pskKey);

	layout->addWidget(pskGroup);

	auto addFileRow = [&](QFormLayout *form, const QString &label, QLineEdit *&edit, const QString &settingsKey) {
		edit = new QLineEdit(m_settings->value(settingsKey).toString());
		edit->setPlaceholderText("(optional)");
		auto *row = new QHBoxLayout;
		row->addWidget(edit);
		auto *btn = new QPushButton("Browse...");
		connect(btn, &QPushButton::clicked, this, [this, edit] {
			QString path = QFileDialog::getOpenFileName(this, "Select File", {},
				"Certificates and Keys (*.pem *.crt *.key *.cer);;All Files (*)");
			if (!path.isEmpty())
				edit->setText(path);
		});
		row->addWidget(btn);
		form->addRow(label, row);
	};

	auto *tlsGroup = new QGroupBox("TLS");
	auto *tlsForm = new QFormLayout(tlsGroup);
	addFileRow(tlsForm, "CA File:", m_caFile, "mqtt/ca_file");
	layout->addWidget(tlsGroup);

	auto *certGroup = new QGroupBox("Client Certificate");
	auto *certForm = new QFormLayout(certGroup);
	addFileRow(certForm, "Certificate:", m_certFile, "mqtt/cert_file");
	addFileRow(certForm, "Private Key:", m_keyFile, "mqtt/key_file");
	layout->addWidget(certGroup);

	auto *notifyGroup = new QGroupBox("Notifications");
	auto *notifyForm = new QFormLayout(notifyGroup);
	m_pageAlert = new QCheckBox("Alert on sysop page");
	m_pageAlert->setChecked(m_settings->value("notify/page_alert", true).toBool());
	notifyForm->addRow(m_pageAlert);
	layout->addWidget(notifyGroup);

	auto *logGroup = new QGroupBox("Log");
	auto *logForm = new QFormLayout(logGroup);
	m_maxLogLines = new QSpinBox;
	m_maxLogLines->setRange(1000, 100000000);
	m_maxLogLines->setSingleStep(100000);
	m_maxLogLines->setValue(m_settings->value("log/max_lines", DefaultMaxLogLines).toInt());
	m_maxLogLines->setSuffix(" lines");
	logForm->addRow("Max lines per pane:", m_maxLogLines);
	layout->addWidget(logGroup);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::saveAndAccept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);
}

void SettingsDialog::saveAndAccept()
{
	m_settings->setValue("mqtt/host", m_host->text());
	m_settings->setValue("mqtt/port", m_port->value());
	m_settings->setValue("mqtt/bbs_id", m_bbsId->text());
	m_settings->setValue("mqtt/username", m_username->text());
	m_settings->setValue("mqtt/password", m_password->text());
	m_settings->setValue("mqtt/psk_identity", m_pskIdentity->text());
	m_settings->setValue("mqtt/psk_key", m_pskKey->text());
	m_settings->setValue("mqtt/ca_file", m_caFile->text());
	m_settings->setValue("mqtt/cert_file", m_certFile->text());
	m_settings->setValue("mqtt/key_file", m_keyFile->text());
	m_settings->setValue("mqtt/publish_qos", m_publishQos->currentData().toInt());
	m_settings->setValue("notify/page_alert", m_pageAlert->isChecked());
	m_settings->setValue("log/max_lines", m_maxLogLines->value());
	accept();
}
