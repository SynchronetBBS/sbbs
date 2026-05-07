#ifndef CLIENTWIDGET_H
#define CLIENTWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QVariantMap>

class ClientWidget : public QWidget
{
	Q_OBJECT

public:
	explicit ClientWidget(bool dark = true, QWidget *parent = nullptr);
	void setDark(bool dark);

public slots:
	void updateClient(const QString &server, const QString &action, const QVariantMap &data);

private:
	QTreeWidget *m_tree;
	QHash<QString, QTreeWidgetItem *> m_clients;
	bool m_dark;
};

#endif
