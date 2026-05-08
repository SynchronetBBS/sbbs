#ifndef ACTIONWIDGET_H
#define ACTIONWIDGET_H

#include <QWidget>
#include <QTreeWidget>

class ActionWidget : public QWidget
{
	Q_OBJECT

public:
	explicit ActionWidget(bool dark = true, QWidget *parent = nullptr);
	void setDark(bool dark);

public slots:
	void addAction(const QString &action, const QString &detail,
	               const QString &timestamp, const QString &payload);

private:
	QTreeWidget *m_tree;
	bool m_dark;
	static const int MaxRows = 1000;
};

#endif
