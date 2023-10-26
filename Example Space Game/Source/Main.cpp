// handles everything
#include "Application.h"
// program entry point
int main()
{
	Application app;
	if (app.Init()) {
		if (app.Run()) {
			return app.Shutdown() ? 0 : 1;
		}
	}
	return 1;
}