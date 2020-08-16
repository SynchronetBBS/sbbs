// $Id: age.js,v 1.1 2020/03/02 18:38:49 rswindell Exp $

require('text.js', 'Years');

// Ported from src/sbbs3/str.cpp, sbbs_t::age_of_posted_item()
function seconds(t, adjust_for_zone)
{
	var	now = time();
	if(adjust_for_zone)
		now += new Date().getTimezoneOffset() * 60
	
	var diff = now - Number(t);
	if(diff < 0) {
		past = bbs.text(InTheFuture);
		diff = -diff;
	}
	return diff;
}

function minutes(t, adjust_for_zone)
{
	return seconds(t, adjust_for_zone) / 60.0;
}

function hours(t, adjust_for_zone)
{
	return minutes(t, adjust_for_zone) / 60.0;
}

function days(t, adjust_for_zone)
{
	return hours(t, adjust_for_zone) / 24.0;
}

function months(t, adjust_for_zone)
{
	return days(t, adjust_for_zone) / 30.0;
}

function years(t, adjust_for_zone)
{
	return days(t, adjust_for_zone) / 365.25;
}

function string(t, adjust_for_zone)
{
	var	past = bbs.text(InThePast);
	var	units = bbs.text(Years);
	var value;
	var diff = seconds(t, adjust_for_zone);
	
	if(diff < 60) {
		value = format("%.0f", diff);
		units = bbs.text(Seconds);
	} else if(diff < 60*60) {
		value = format("%.0f", diff / 60.0);
		units = bbs.text(Minutes);
	} else if(diff < 60*60*24) {
		value = format("%.1f", diff / (60.0 * 60.0));
		units = bbs.text(Hours);
	} else if(diff < 60*60*24*30) {
		value = format("%.1f", diff / (60.0 * 60.0 * 24.0));
		units = bbs.text(Days);
	} else if(diff < 60*60*24*365) {
		value = format("%.1f", diff / (60.0 * 60.0 * 24.0 * 30.0));
		units = bbs.text(Months);
	} else
		value = format("%.1f", diff / (60.0 * 60.0 * 24.0 * 365.25));
	
	return(format(bbs.text(AgeOfPostedItem), value, units, past));
}

this;