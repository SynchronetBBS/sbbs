#include "settingsdialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>

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
	accept();
}
