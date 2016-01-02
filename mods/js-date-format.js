/**
* js-date-format (js-date-format.js)
* v1.0
* (c) Tony Brix (tonybrix.info) - 2014.
* https://github.com/UziTech/js-date-format
*
* MIT License
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
* associated documentation files (the "Software"), to deal in the Software without restriction,
* including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial
* portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
* NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
Date.prototype.setLocale = function (lang) {
	if (lang && lang in Date.locales) {
		this.locale = lang;
	}
};

Date.prototype.getLocale = function () {
	return this.locale || "en";
};

Date.prototype.getMonthName = function (lang) {
	var locale = "en";
	if (lang && lang in Date.locales) {
		locale = lang;
	} else if (this.locale && this.locale in Date.locales) {
		locale = this.locale;
	}
	return Date.locales[locale].month_names[this.getMonth()];
};

Date.prototype.getMonthNameShort = function (lang) {
	var locale = "en";
	if (lang && lang in Date.locales) {
		locale = lang;
	} else if (this.locale && this.locale in Date.locales) {
		locale = this.locale;
	}
	return Date.locales[locale].month_names_short[this.getMonth()];
};

Date.prototype.getDayName = function (lang) {
	var locale = "en";
	if (lang && lang in Date.locales) {
		locale = lang;
	} else if (this.locale && this.locale in Date.locales) {
		locale = this.locale;
	}
	return Date.locales[locale].day_names[this.getDay()];
};

Date.prototype.getDayNameShort = function (lang) {
	var locale = "en";
	if (lang && lang in Date.locales) {
		locale = lang;
	} else if (this.locale && this.locale in Date.locales) {
		locale = this.locale;
	}
	return Date.locales[locale].day_names_short[this.getDay()];
};

Date.prototype.getDateSuffix = function (lang) {
	var locale = "en";
	if (lang && lang in Date.locales) {
		locale = lang;
	} else if (this.locale && this.locale in Date.locales) {
		locale = this.locale;
	}
	return Date.locales[locale].date_suffix(this.getDate());
};

Date.prototype.getMeridiem = function (isLower, lang) {
	var locale = "en";
	if (lang && lang in Date.locales) {
		locale = lang;
	} else if (this.locale && this.locale in Date.locales) {
		locale = this.locale;
	}
	return Date.locales[locale].meridiem(this.getHours(), this.getMinutes(), isLower);
};

Date.prototype.getLastDate = function () {
	return (new Date(this.getFullYear(), this.getMonth() + 1, 0)).getDate();
};

/* languages from http://momentjs.com */
Date.locales = {
	"en": {
		month_names: ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"],
		month_names_short: ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"],
		day_names: ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"],
		day_names_short: ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"],
		date_suffix: function (date) {
			var day10 = ~~ (date % 100 / 10);
			var day1 = date % 10;
			if (day10 === 1) {
				return "th";
			} else if (day1 === 1) {
				return "st";
			} else if (day1 === 2) {
				return "nd";
			} else if (day1 === 3) {
				return "rd";
			} else {
				return "th";
			}
		},
		meridiem : function (hour, minute, isLower) {
			if (hour < 12) {
				return isLower ? "am" : "AM";
			} else {
				return isLower ? "pm" : "PM";
			}
		}
	}
};

Date.prototype.format = function (formatString) {

	var addPadding = function (value, length) {
		var negative = ((value < 0) ? "-" : "");
		var zeros = "0";
		for (var i = 2; i < length; i++) {
			zeros += "0";
		}
		return negative + (zeros + Math.abs(value).toString()).slice(-length);
	};

	var replacements = {
		date: this,
		YYYY: function () {
			return this.date.getFullYear();
		},
		YY: function () {
			return this.date.getFullYear() % 100;
		},
		MMMM: function () {
			return this.date.getMonthName();
		},
		MMM: function () {
			return this.date.getMonthNameShort();
		},
		MM: function () {
			return addPadding((this.date.getMonth() + 1), 2);
		},
		M: function () {
			return this.date.getMonth() + 1;
		},
		DDDD: function () {
			return this.date.getDayName();
		},
		DDD: function () {
			return this.date.getDayNameShort();
		},
		DD: function () {
			return addPadding(this.date.getDate(), 2);
		},
		D: function () {
			return this.date.getDate();
		},
		S: function () {
			return this.date.getDateSuffix();
		},
		HH: function () {
			return addPadding(this.date.getHours(), 2);
		},
		H: function () {
			return this.date.getHours();
		},
		hh: function () {
			var hour = this.date.getHours();
			if (hour > 12) {
				hour -= 12;
			} else if (hour < 1) {
				hour = 12;
			}
			return addPadding(hour, 2);
		},
		h: function () {
			var hour = this.date.getHours();
			if (hour > 12) {
				hour -= 12;
			} else if (hour < 1) {
				hour = 12;
			}
			return hour;
		},
		mm: function () {
			return addPadding(this.date.getMinutes(), 2);
		},
		m: function () {
			return this.date.getMinutes();
		},
		ss: function () {
			return addPadding(this.date.getSeconds(), 2);
		},
		s: function () {
			return this.date.getSeconds();
		},
		fff: function () {
			return addPadding(this.date.getMilliseconds(), 3);
		},
		ff: function () {
			return addPadding(Math.floor(this.date.getMilliseconds() / 10), 2);
		},
		f: function () {
			return Math.floor(this.date.getMilliseconds() / 100);
		},
		zzzz: function () {
			return addPadding(Math.floor(-this.date.getTimezoneOffset() / 60), 2) + ":" + addPadding(-this.date.getTimezoneOffset() % 60, 2);
		},
		zzz: function () {
			return Math.floor(-this.date.getTimezoneOffset() / 60) + ":" + addPadding(-this.date.getTimezoneOffset() % 60, 2);
		},
		zz: function () {
			return addPadding(Math.floor(-this.date.getTimezoneOffset() / 60), 2);
		},
		z: function () {
			return Math.floor(-this.date.getTimezoneOffset() / 60);
		},
		tt: function () {
			return this.date.getMeridiem(true);
		},
		TT: function () {
			return this.date.getMeridiem(false);
		}
	};


	var formats = new Array();
	while (formatString.length > 0) {
		if (formatString[0] === "\"") {
			var temp = /"[^"]*"/m.exec(formatString);
			if (temp === null) {
				formats.push(formatString.substring(1));
				formatString = "";
			} else {
				temp = temp[0].substring(1, temp[0].length - 1);
				formats.push(temp);
				formatString = formatString.substring(temp.length + 2);
			}
		} else if (formatString[0] === "'") {
			var temp = /'[^']*'/m.exec(formatString);
			if (temp === null) {
				formats.push(formatString.substring(1));
				formatString = "";
			} else {
				temp = temp[0].substring(1, temp[0].length - 1);
				formats.push(temp);
				formatString = formatString.substring(temp.length + 2);
			}
		} else if (formatString[0] === "\\") {
			if (formatString.length > 1) {
				formats.push(formatString.substring(1, 2));
				formatString = formatString.substring(2);
			} else {
				formats.push("\\");
				formatString = "";
			}
		} else {
			var foundMatch = false;
			for (var i = formatString.length; i > 0; i--) {
				if (formatString.substring(0, i) in replacements) {
					formats.push(replacements[formatString.substring(0, i)]());
					formatString = formatString.substring(i);
					foundMatch = true;
					break;
				}
			}
			if (!foundMatch) {
				formats.push(formatString[0]);
				formatString = formatString.substring(1);
			}
		}
	}

	return formats.join("");
};