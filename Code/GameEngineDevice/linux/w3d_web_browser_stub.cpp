#include "W3DDevice/GameClient/W3DWebBrowser.h"
#include "GameClient/GameWindow.h"

W3DWebBrowser::W3DWebBrowser() : WebBrowser()
{
}

Bool W3DWebBrowser::createBrowserWindow(char *url, GameWindow *win)
{
	(void)url;
	(void)win;
	return FALSE;
}

void W3DWebBrowser::closeBrowserWindow(GameWindow *win)
{
	(void)win;
}
