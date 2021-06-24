#include "CwssRecver.h"

#include "resources.cpp"

int uimain(std::function<int()> run) {
	SciterSetOption(NULL, SCITER_SET_DEBUG_MODE, TRUE);
#ifdef  _DEBUG
	sciter::debug_output_console console; //- uncomment it if you will need console window
#endif //  _DEBUG


	SciterSetOption(NULL, SCITER_SET_SCRIPT_RUNTIME_FEATURES,
        ALLOW_FILE_IO |
        ALLOW_SOCKET_IO |
        ALLOW_EVAL |
        ALLOW_SYSINFO);
	
	sciter::archive::instance().open(aux::elements_of(resources)); // bind resources[] (defined in "resources.cpp") with the archive
	frame *pwin = new frame();
	::pwin = pwin;
	// note: this:://app URL is dedicated to the sciter::archive content associated with the application
	pwin->load(WSTR("this://app/main.htm"));

	pwin->expand(false);

	return run();
}