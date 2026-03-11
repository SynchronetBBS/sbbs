/*
 * mocks_news.h
 *
 * Shared mocks for news.h functionality.  Provides news-related stubs
 * used by 5+ test files.
 */

#ifndef MOCKS_NEWS_H
#define MOCKS_NEWS_H

void News_AddNews(char *s)
{
	(void)s;
}

void News_ReadNews(bool t)
{
	(void)t;
}

void News_CreateTodayNews(void)
{
}

#endif /* MOCKS_NEWS_H */
