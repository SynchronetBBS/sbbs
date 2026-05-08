#ifndef TEXTUTIL_H
#define TEXTUTIL_H

#include <QString>
#include <QTreeWidgetItem>

static const char *CtrlNames[] = {
	"NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
	"BS",  "HT",  "LF",  "VT",  "FF",  "CR",  "SO",  "SI",
	"DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
	"CAN", "EM",  "SUB", "ESC", "FS",  "GS",  "RS",  "US",
};

static inline const char *ctrlCEscape(ushort c)
{
	switch (c) {
	case 0x00: return "\\0";
	case 0x07: return "\\a";
	case 0x08: return "\\b";
	case 0x09: return "\\t";
	case 0x0A: return "\\n";
	case 0x0B: return "\\v";
	case 0x0C: return "\\f";
	case 0x0D: return "\\r";
	case 0x1B: return "\\e";
	default:   return nullptr;
	}
}

static inline bool hasControlChars(const QString &s)
{
	for (QChar ch : s) {
		ushort c = ch.unicode();
		if (c <= 0x1F || c == 0x7F)
			return true;
	}
	return false;
}

static inline QString sanitizeControls(const QString &s)
{
	QString out;
	out.reserve(s.size());
	for (QChar ch : s) {
		ushort c = ch.unicode();
		if (c <= 0x1F)
			out += QChar(0x2400 + c);
		else if (c == 0x7F)
			out += QChar(0x2421);
		else
			out += ch;
	}
	return out;
}

static inline QString controlCharTooltip(const QString &s)
{
	QStringList found;
	QSet<ushort> seen;
	for (QChar ch : s) {
		ushort c = ch.unicode();
		if ((c <= 0x1F || c == 0x7F) && !seen.contains(c)) {
			seen.insert(c);
			QChar caret = (c == 0x7F) ? '?' : QChar('@' + c);
			const char *name = (c == 0x7F) ? "DEL" : CtrlNames[c];
			const char *esc = ctrlCEscape(c);
			if (esc)
				found << QString("%1, ^%2 (0x%3 \u2014 %4)")
					.arg(esc).arg(caret).arg(c, 2, 16, QChar('0')).arg(name);
			else
				found << QString("^%1 (0x%2 \u2014 %3)")
					.arg(caret).arg(c, 2, 16, QChar('0')).arg(name);
		}
	}
	return found.join("\n");
}

static inline void setItemText(QTreeWidgetItem *item, int col, const QString &text)
{
	if (hasControlChars(text)) {
		item->setText(col, sanitizeControls(text));
		item->setToolTip(col, controlCharTooltip(text));
	} else {
		item->setText(col, text);
	}
}

#endif
