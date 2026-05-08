#ifndef ACTIONWIDGET_H
#define ACTIONWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QToolButton>
#include <QMenu>
#include <QSet>

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
	void applyFilters();

	QTreeWidget *m_tree;
	QToolButton *m_showBtn;
	QMenu *m_showMenu;
	QHash<QString, QAction *> m_filterActions;
	QSet<QString> m_hiddenActions;
	bool m_dark;
	static const int MaxRows = 1000;
};

#endif
