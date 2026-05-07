#ifndef LOGINATTEMPTSWIDGET_H
#define LOGINATTEMPTSWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QVariantMap>

class LoginAttemptsWidget : public QWidget
{
	Q_OBJECT

public:
	explicit LoginAttemptsWidget(bool dark = true, QWidget *parent = nullptr);
	void setDark(bool dark);

public slots:
	void updateAttempt(const QString &ip, const QString &action, const QVariantMap &data);

signals:
	void countChanged(int count);

private:
	QTreeWidget *m_tree;
	QHash<QString, QTreeWidgetItem *> m_attempts;
	bool m_dark;
};

#endif
