/*
 * mocks_video.h
 *
 * Shared mocks for video.h functionality.  Provides ScreenWidth, ScreenLines,
 * and video output stubs used by 14+ test files.
 */

#ifndef MOCKS_VIDEO_H
#define MOCKS_VIDEO_H

int ScreenWidth = 80;
int ScreenLines = 24;

void DisplayStr(const char *s)
{
	(void)s;
}

void zputs(const char *s)
{
	(void)s;
}

#endif /* MOCKS_VIDEO_H */
