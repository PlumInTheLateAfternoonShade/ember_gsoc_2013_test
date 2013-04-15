/*
 *  File:       Application.cpp
 *  Summary:    The class which initializes the GUI.
 *  Written by: nikal
 *
 *  Copyright (C) 2001, 2002 nikal.
 *  This code is distributed under the GPL.
 *  See file COPYING for details.
 *

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Application.h"

#ifdef _WIN32
#include "platform/platform_windows.h"
#else
#include <dirent.h>
#endif

#include <Eris/View.h>
#include <Eris/PollDefault.h>
#include <Eris/DeleteLater.h>

#include "services/EmberServices.h"
#include "services/logging/LoggingService.h"
#include "services/server/ServerService.h"
#include "services/config/ConfigService.h"
#include "services/config/ConfigListenerContainer.h"
#include "services/metaserver/MetaserverService.h"
#include "services/sound/SoundService.h"
#include "services/scripting/ScriptingService.h"
#include "services/wfut/WfutService.h"
#include "services/input/InputService.h"
#include "services/input/Input.h"
#include "services/serversettings/ServerSettings.h"

#include "framework/ConsoleBackend.h"
#include "framework/ShutdownException.h"
#include "framework/TimeFrame.h"
#include "framework/FileResourceProvider.h"
#include "framework/osdir.h"

#include "components/lua/LuaScriptingProvider.h"
#include "components/lua/Connectors.h"

#include "components/ogre/EmberOgre.h"

#include "ConfigBoundLogObserver.h"

#include "components/lua/tolua++.h"

TOLUA_API int tolua_Ogre_open(lua_State* tolua_S);
TOLUA_API int tolua_Eris_open(lua_State* tolua_S);
TOLUA_API int tolua_EmberServices_open(lua_State* tolua_S);
TOLUA_API int tolua_EmberOgre_open(lua_State* tolua_S);
TOLUA_API int tolua_Helpers_open(lua_State* tolua_S);
TOLUA_API int tolua_Framework_open(lua_State* tolua_S);
TOLUA_API int tolua_AtlasAdapters_open(lua_State* tolua_S);
TOLUA_API int tolua_Adapters_open(lua_State* tolua_S);
TOLUA_API int tolua_Representations_open(lua_State* tolua_S);
TOLUA_API int tolua_Atlas_open(lua_State* tolua_S);
TOLUA_API int tolua_Varconf_open(lua_State* tolua_S);
TOLUA_API int tolua_Lua_open(lua_State* tolua_S);
TOLUA_API int tolua_ConnectorDefinitions_open(lua_State* tolua_S);
TOLUA_API int tolua_Domain_open(lua_State* tolua_S);

#include <iostream>
#include <sstream>
#include <stdio.h>
#include <list>

#include "framework/osdir.h"
#include <sys/stat.h>

#include <boost/thread.hpp>

#ifndef HAVE_SIGHANDLER_T
typedef void (*sighandler_t)(int);
#endif

extern "C"
{
#include <signal.h>    /* signal name macros, and the signal() prototype */

sighandler_t oldSignals[NSIG];
}

using boost::posix_time::microsec_clock;
using boost::posix_time::ptime;

namespace Ember
{

/**
 * @author Erik Hjortsberg <erik.hjortsberg@gmail.com>
 * @brief A simple listener class for the general:desiredfps config setting, which configures the capped fps.
 */
class DesiredFpsListener: public ConfigListenerContainer
{
protected:

	/**
	 * @brief The desired frames per second.
	 * If set to 0 no capping will occur.
	 */
	long mDesiredFps;

	/**
	 * @brief How long each frame should be in microseconds.
	 * If set to 0 no capping will occur.
	 */
	long mMicrosecondsPerFrame;

	void Config_DesiredFps(const std::string& section, const std::string& key, varconf::Variable& variable)
	{
		//Check for double, but cast to int. That way we'll catch all numbers.
		if (variable.is_double()) {
			mDesiredFps = static_cast<int>(variable);
			if (mDesiredFps != 0) {
				mMicrosecondsPerFrame = 1000000L / mDesiredFps;
			} else {
				mMicrosecondsPerFrame = 0;
			}
		}
	}

public:

	/**
	 * @brief Ctor.
	 * A listener will be set up listening for the general:desiredfps config setting.
	 */
	DesiredFpsListener() :
			mDesiredFps(0), mMicrosecondsPerFrame(0)
	{
		registerConfigListener("general", "desiredfps", sigc::mem_fun(*this, &DesiredFpsListener::Config_DesiredFps));
	}

	/**
	 * @brief Accessor for the desired framed per second.
	 * If 0 no capping should occur.
	 */
	long getDesiredFps() const
	{
		return mDesiredFps;
	}

	/**
	 * @brief Accessor for the minimum length (in milliseconds) that each frame should take in order for the desired fps to be kept.
	 * If 0 no capping will occur.
	 */
	long getMicrosecondsPerFrame() const
	{
		return mMicrosecondsPerFrame;
	}
};

template<> Application *Singleton<Application>::ms_Singleton = 0;

Application::Application(const std::string prefix, const std::string homeDir, const ConfigMap& configSettings) :
		mOgreView(0), mShouldQuit(false), mPollEris(true), mMainLoopController(mShouldQuit, mPollEris), mPrefix(prefix), mHomeDir(homeDir), mLogObserver(0), mServices(0), mWorldView(0), mConfigSettings(configSettings), mConsoleBackend(new ConsoleBackend()), Quit("quit", this, "Quit Ember."), ToggleErisPolling("toggle_erispolling", this, "Switch server polling on and off."), mScriptingResourceProvider(0)

{

}

Application::~Application()
{
	// before shutting down, we write out the user config to user's ember home directory
	ConfigService& configService = mServices->getConfigService();
	configService.saveConfig(configService.getHomeDirectory() + "/ember.conf", varconf::USER);

	mServices->getServerService().stop(0);

	//When shutting down make sure to delete all pending objects from Eris. This is mainly because we want to be able to track memory leaks.
	Eris::execDeleteLaters();

	delete mOgreView;
	delete mServices;
	delete mScriptingResourceProvider;
	S_LOG_INFO("Ember shut down normally.");
	Log::removeObserver(mLogObserver);
	delete mLogObserver;
}

/**
 * Detach the input system, else the mouse will be locked.
 */
extern "C" void shutdownHandler(int sig)
{
	std::cerr << "Crashed with signal " << sig << ", will try to detach the input system gracefully. Please report bugs at https://bugs.launchpad.net/ember" << std::endl << std::flush;
	if (Input::hasInstance()) {
		Input::getSingleton().shutdownInteraction();
	}
	if (oldSignals[sig] != SIG_DFL && oldSignals[sig] != SIG_IGN ) {
		/* Call saved signal handler. */
		oldSignals[sig](sig);
	} else {
		/* Reraise the signal. */
		signal(sig, SIG_DFL );
		raise(sig);
	}
}

void Application::registerComponents()
{
	mOgreView = new OgreView::EmberOgre();
}

void Application::mainLoopStep(long minMicrosecondsPerFrame)
{
	TimeFrame timeFrame = TimeFrame(boost::posix_time::microseconds(minMicrosecondsPerFrame));
	Input& input(Input::getSingleton());
	ptime currentTime;
	unsigned int frameActionMask = 0;
	try {

		if (mPollEris) {
			currentTime = microsec_clock::local_time();
			mMainLoopController.EventStartErisPoll.emit((currentTime - mLastTimeErisPollStart).total_microseconds() / 1000000.0f);
			mLastTimeErisPollStart = currentTime;
			Eris::PollDefault::poll(0);
			if (mWorldView) {
				mWorldView->update();
			}
			currentTime = microsec_clock::local_time();
			mMainLoopController.EventEndErisPoll.emit((currentTime - mLastTimeErisPollEnd).total_microseconds() / 1000000.0f);
			mLastTimeErisPollEnd = currentTime;
			frameActionMask |= MainLoopController::FA_ERIS;
		}

		currentTime = microsec_clock::local_time();
		mMainLoopController.EventBeforeInputProcessing.emit((currentTime - mLastTimeInputProcessingStart).total_microseconds() / 1000000.0f);
		mLastTimeInputProcessingStart = currentTime;
		input.processInput();
		frameActionMask |= MainLoopController::FA_INPUT;

		currentTime = microsec_clock::local_time();
		mMainLoopController.EventAfterInputProcessing.emit((currentTime - mLastTimeInputProcessingEnd).total_microseconds() / 1000000.0f);
		mLastTimeInputProcessingEnd = currentTime;

		bool updatedRendering = mOgreView->renderOneFrame(timeFrame);
		if (updatedRendering) {
			frameActionMask |= MainLoopController::FA_GRAPHICS;
		}
		mServices->getSoundService().cycle();
		frameActionMask |= MainLoopController::FA_SOUND;

		mMainLoopController.EventFrameProcessed(timeFrame, frameActionMask);

		//If we should cap the fps so that each frame should take a minimum amount of time,
		//we need to see if we should sleep a little.
		if (minMicrosecondsPerFrame > 0) {
			if (timeFrame.isTimeLeft()) {
				try {
					boost::this_thread::sleep(timeFrame.getRemainingTime());
				} catch (const boost::thread_interrupted& ex) {
				}
			}
		}
		mLastTimeMainLoopStepEnded = microsec_clock::local_time();
	} catch (const std::exception& ex) {
		S_LOG_CRITICAL("Got exception, shutting down." << ex);
		throw;
	} catch (const std::string& ex) {
		S_LOG_CRITICAL("Got exception, shutting down. " << ex);
		throw;
	} catch (...) {
		S_LOG_CRITICAL("Got unknown exception.");
		throw;
	}
}

void Application::mainLoop()
{
	const ptime currentTime = microsec_clock::local_time();
	mLastTimeErisPollStart = currentTime;
	mLastTimeErisPollEnd = currentTime;
	mLastTimeInputProcessingStart = currentTime;
	mLastTimeInputProcessingEnd = currentTime;
	mLastTimeMainLoopStepEnded = currentTime;
	DesiredFpsListener desiredFpsListener;
	while (mShouldQuit == false) {
		mainLoopStep(desiredFpsListener.getMicrosecondsPerFrame());
	}
}

void Application::prepareComponents()
{
}

void Application::initializeServices()
{
	// Initialize Ember services
	std::cout << "Initializing Ember services" << std::endl;

	mServices = new EmberServices();
	// Initialize the Configuration Service
	ConfigService& configService = mServices->getConfigService();
	configService.start();
	if (mPrefix != "") {
		configService.setPrefix(mPrefix);
	}
	if (mHomeDir != "") {
		configService.setHomeDirectory(mHomeDir);
		std::cout << "Setting home directory to " << mHomeDir << std::endl;
	}

	//output all logging to ember.log
	std::string filename(configService.getHomeDirectory() + "/ember.log");
	mLogOutStream = std::unique_ptr < std::ofstream > (new std::ofstream(filename.c_str()));

	//write to the log the version number
	*mLogOutStream << "Ember version " << VERSION << std::endl;

	mLogObserver = new ConfigBoundLogObserver(configService, *mLogOutStream);
	Log::addObserver(mLogObserver);

	//default to INFO, though this can be changed by the config file
	mLogObserver->setFilter(Log::INFO);

	// Change working directory
	const std::string& dirName = configService.getHomeDirectory();
	oslink::directory osdir(dirName);

	if (!osdir) {
		oslink::directory::mkdir(dirName.c_str());
	}

	int result = chdir(configService.getHomeDirectory().c_str());
	if (result) {
		S_LOG_WARNING("Could not change directory to '"<< configService.getHomeDirectory().c_str() <<"'.");
	}

	//load the config file. Note that this will load the shared config file, and then the user config file if available (~/.ember/ember.conf)
	configService.loadSavedConfig("ember.conf", mConfigSettings);

	//Check if there's a user specific ember.conf file. If not, create an empty template one.
	std::string userConfigFilePath = configService.getHomeDirectory() + "/ember.conf";
	struct stat tagStat;
	int ret = stat(userConfigFilePath.c_str(), &tagStat);
	if (ret == -1) {
		//Create empty template file.
		std::ofstream outstream(userConfigFilePath.c_str());
		outstream << "#This is a user specific settings file. Settings here override those found in the application installed ember.conf file." << std::endl << std::flush;
		S_LOG_INFO("Created empty user specific settings file at '" << userConfigFilePath << "'.");
	}

	S_LOG_INFO("Using media from " << configService.getEmberMediaDirectory());

	// Initialize the Sound Service
	S_LOG_INFO("Initializing sound service");
	mServices->getSoundService().start();

	// Initialize and start the Metaserver Service.
	S_LOG_INFO("Initializing metaserver service");

	mServices->getMetaserverService().start();
	//hoho, we get linking errors if we don't do some calls to the service
	mServices->getMetaserverService().getMetaServer();

	// Initialize the Server Service
	S_LOG_INFO("Initializing server service");
	mServices->getServerService().start();

	S_LOG_INFO("Initializing input service");
	mServices->getInputService().start();
	mServices->getInputService().getInput().setMainLoopController(&mMainLoopController);

	S_LOG_INFO("Initializing scripting service");
	mServices->getScriptingService().start();

	S_LOG_INFO("Initializing wfut service");
	mServices->getWfutService().start();

	S_LOG_INFO("Initializing server settings service");
	mServices->getServerSettingsService().start();

	mServices->getServerService().GotView.connect(sigc::mem_fun(*this, &Application::Server_GotView));
	mServices->getServerService().DestroyedView.connect(sigc::mem_fun(*this, &Application::Server_DestroyedView));

	//register the lua scripting provider. The provider will be owned by the scripting service, so we don't need to keep the pointer reference.
	Lua::LuaScriptingProvider* luaProvider = new Lua::LuaScriptingProvider();

	tolua_Lua_open(luaProvider->getLuaState());
	tolua_Framework_open(luaProvider->getLuaState());
	tolua_EmberOgre_open(luaProvider->getLuaState());
	tolua_Eris_open(luaProvider->getLuaState());
	tolua_EmberServices_open(luaProvider->getLuaState());
	tolua_Helpers_open(luaProvider->getLuaState());
	tolua_Ogre_open(luaProvider->getLuaState());
	tolua_AtlasAdapters_open(luaProvider->getLuaState());
	tolua_Adapters_open(luaProvider->getLuaState());
	tolua_Representations_open(luaProvider->getLuaState());
	tolua_Atlas_open(luaProvider->getLuaState());
	tolua_Varconf_open(luaProvider->getLuaState());
	tolua_ConnectorDefinitions_open(luaProvider->getLuaState());
	tolua_Domain_open(luaProvider->getLuaState());

	mServices->getScriptingService().registerScriptingProvider(luaProvider);
	Lua::ConnectorBase::setState(luaProvider->getLuaState());

	mScriptingResourceProvider = new FileResourceProvider(mServices->getConfigService().getSharedMediaDirectory() + "/scripting/");
	mServices->getScriptingService().setResourceProvider(mScriptingResourceProvider);

	oldSignals[SIGSEGV] = signal(SIGSEGV, shutdownHandler);
	oldSignals[SIGABRT] = signal(SIGABRT, shutdownHandler);
	oldSignals[SIGILL] = signal(SIGILL, shutdownHandler);
#ifndef _WIN32
	oldSignals[SIGBUS] = signal(SIGBUS, shutdownHandler);
#endif

	EventServicesInitialized.emit();
}

void Application::Server_GotView(Eris::View* view)
{
	mWorldView = view;
}

void Application::Server_DestroyedView()
{
	mWorldView = 0;
}

Eris::View* Application::getMainView()
{
	return mWorldView;
}

void Application::startScripting()
{
	//this should be defined in some kind of text file, which should be different depending on what game you're playing (like mason)
	try {
		//load the bootstrap script which will load all other scripts
		mServices->getScriptingService().loadScript("lua/Bootstrap.lua");
	} catch (const std::exception& e) {
		S_LOG_FAILURE("Error when loading bootstrap script." << e);
	}

	//load any user defined scripts
	const std::string userScriptDirectoryPath = mServices->getConfigService().getHomeDirectory() + "/scripts";
	oslink::directory scriptDir(userScriptDirectoryPath);
	if (scriptDir.isExisting()) {
		static const std::string luaSuffix = ".lua";
		std::list<std::string> luaFiles;
		while (scriptDir) {
			std::string fileName = scriptDir.next();
			if (fileName != "." && fileName != "..") {
				std::string lowerCaseFileName = fileName;
				std::transform(lowerCaseFileName.begin(), lowerCaseFileName.end(), lowerCaseFileName.begin(), ::tolower);

				if (lowerCaseFileName.compare(lowerCaseFileName.length() - luaSuffix.length(), luaSuffix.length(), luaSuffix) == 0) {
					luaFiles.push_back(fileName);
				}

			}
		}

		//Sorting, because we want to load the scripts in a deterministic order.
		luaFiles.sort();
		for (auto& fileName : luaFiles) {
			std::ifstream stream(userScriptDirectoryPath + "/" + fileName, std::ios::in);
			if (stream) {
				std::stringstream ss;
				ss << stream.rdbuf();
				stream.close();
				//It's important that we inform the user that we're loading a script (in case it provides any confusing behaviour).
				ConsoleBackend::getSingleton().pushMessage("Loading user Lua script from '" + fileName + "'.", "info");
				mServices->getScriptingService().executeCode(ss.str(), "LuaScriptingProvider");
			}
		}
	} else {
		try {
			//Create the script user script directory
			oslink::directory::mkdir(userScriptDirectoryPath.c_str());
			std::ofstream readme(userScriptDirectoryPath + "/README", std::ios::out);
			readme << "Any script files placed here will be executed as long as they have a supported file suffix.\nScripts are executed in alphabetical order.\nEmber currently supports lua scripts (ending with '.lua').";
			readme.close();
			S_LOG_INFO("Created user user scripting directory (" + userScriptDirectoryPath + ").");
		} catch (const std::exception& e) {
			S_LOG_INFO("Could not create user scripting directory.");
		}
	}
}

void Application::start()
{

	try {
		if (!mOgreView->setup(Input::getSingleton(), mMainLoopController)) {
			//The setup was cancelled, return.
			return;
		}
	} catch (const std::exception& ex) {
		std::cerr << "==== Error during startup ====\n\r\t" << ex.what() << "\n" << std::endl;
		S_LOG_CRITICAL("Error during startup." << ex);
		return;
	} catch (ShutdownException& ex2) {
		//Note that a ShutdownException is not an error. It just means that the user closed the application during startup. We should therefore just exit, as intended.
		S_LOG_INFO("ShutdownException caught: " << ex2.getReason());
		return;
	} catch (...) {
		std::cerr << "==== Error during startup ====\n\r\tUnknown fatal error during startup. Something went wrong which caused a shutdown. Check the log file for more information." << std::endl;
		S_LOG_CRITICAL("Unknown fatal error during startup.");
		return;
	}
	Input::getSingleton().startInteraction();

	startScripting();

	mainLoop();

}

void Application::runCommand(const std::string& command, const std::string& args)
{
	if (command == Quit.getCommand()) {
		mShouldQuit = true;
	} else if (ToggleErisPolling == command) {
		mPollEris = !mPollEris;
	}
}

}
