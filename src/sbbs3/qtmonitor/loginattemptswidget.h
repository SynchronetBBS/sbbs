#ifndef LOGINATTEMPTSWIDGET_H
#define LOGINATTEMPTSWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QToolButton>
#include <QMenu>
#include <QVariantMap>

class LoginAttemptsWidget : public QWidget
{
	Q_OBJECT

public:
	explicit LoginAttemptsWidget(const QStringList &servers,
	                            const QHash<QString, QString> &serverLabels,
	                            bool dark = true, QWidget *parent = nullptr);
	void setDark(bool dark);

public slots:
	void updateAttempt(const QString &ip, const QString &action, const QVariantMap &data);

signals:
	void countChanged(int count);
	void clearAttempt(const QString &ip);
	void clearAllAttempts(const QString &server);

private:
	QTreeWidget *m_tree;
	QHash<QString, QTreeWidgetItem *> m_attempts;
	bool m_dark;
};

#endif
