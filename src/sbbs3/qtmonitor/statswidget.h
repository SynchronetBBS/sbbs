#ifndef STATSWIDGET_H
#define STATSWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QHash>

class StatsWidget : public QWidget
{
	Q_OBJECT

public:
	explicit StatsWidget(bool dark = true, QWidget *parent = nullptr);
	void setStat(const QString &key, const QString &value);
	void setDark(bool dark);

private:
	QHash<QString, QLabel *> m_labels;
	bool m_dark;
};

#endif
