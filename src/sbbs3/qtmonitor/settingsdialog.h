#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QSettings>

class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SettingsDialog(QSettings *settings, QWidget *parent = nullptr);

private:
	void saveAndAccept();

	QSettings *m_settings;
	QLineEdit *m_host;
	QSpinBox *m_port;
	QLineEdit *m_bbsId;
	QLineEdit *m_username;
	QLineEdit *m_password;
	QLineEdit *m_pskIdentity;
	QLineEdit *m_pskKey;
};

#endif
