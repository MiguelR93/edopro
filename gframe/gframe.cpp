#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Tchar.h> //_tmain
#else
#define _tmain main
#include <unistd.h>
#endif
#include <curl/curl.h>
#include <event2/thread.h>
#include <IrrlichtDevice.h>
#include <IGUIButton.h>
#include <IGUICheckBox.h>
#include <IGUIEditBox.h>
#include <IGUIWindow.h>
#include <IGUIEnvironment.h>
#include <ISceneManager.h>
#include "client_updater.h"
#include "config.h"
#include "data_handler.h"
#include "logging.h"
#include "game.h"
#include "log.h"
#include "joystick_wrapper.h"
#include "utils_gui.h"
#ifdef EDOPRO_MACOS
#include "osx_menu.h"
#endif

bool exit_on_return = false;
bool is_from_discord = false;
bool open_file = false;
epro::path_string open_file_name = EPRO_TEXT("");
bool show_changelog = false;
ygo::Game* ygo::mainGame = nullptr;
ygo::ImageDownloader* ygo::gImageDownloader = nullptr;
ygo::DataManager* ygo::gDataManager = nullptr;
ygo::SoundManager* ygo::gSoundManager = nullptr;
ygo::GameConfig* ygo::gGameConfig = nullptr;
ygo::RepoManager* ygo::gRepoManager = nullptr;
ygo::DeckManager* ygo::gdeckManager = nullptr;
ygo::ClientUpdater* ygo::gClientUpdater = nullptr;
JWrapper* gJWrapper = nullptr;

inline void TriggerEvent(irr::gui::IGUIElement* target, irr::gui::EGUI_EVENT_TYPE type) {
	irr::SEvent event;
	event.EventType = irr::EET_GUI_EVENT;
	event.GUIEvent.EventType = type;
	event.GUIEvent.Caller = target;
	ygo::mainGame->device->postEventFromUser(event);
}

inline void ClickButton(irr::gui::IGUIElement* btn) {
	TriggerEvent(btn, irr::gui::EGET_BUTTON_CLICKED);
}

inline void SetCheckbox(irr::gui::IGUICheckBox* chk, bool state) {
	chk->setChecked(state);
	TriggerEvent(chk, irr::gui::EGET_CHECKBOX_CHANGED);
}

#define PARAM_CHECK(x) (parameter[1] == EPRO_TEXT(x))
#define RUN_IF(x,expr) (PARAM_CHECK(x)) {i++; if(i < argc) {expr;} continue;}
#define SET_TXT(elem) ygo::mainGame->elem->setText(ygo::Utils::ToUnicodeIfNeeded(parameter).data())

void CheckArguments(int argc, epro::path_char* argv[]) {
	bool keep_on_return = false;
	for(int i = 1; i < argc; ++i) {
		epro::path_stringview parameter = argv[i];
		if(parameter.size() < 2)
			continue;
		if(parameter[0] == EPRO_TEXT('-')) {
			// Extra database
			if RUN_IF('e', if(ygo::gDataManager->LoadDB(parameter)) ygo::WindBot::AddDatabase(parameter) )
				// Nickname
			else if RUN_IF('n', SET_TXT(ebNickName))
				// Host address
			else if RUN_IF('h', SET_TXT(ebJoinHost))
				// Host Port
			else if RUN_IF('p', SET_TXT(ebJoinPort))
				// Host password
			else if RUN_IF('w', SET_TXT(ebJoinPass))
			else if(PARAM_CHECK('k')) { // Keep on return
				exit_on_return = false;
				keep_on_return = true;
			} else if(PARAM_CHECK('m')) { // Mute
				SetCheckbox(ygo::mainGame->tabSettings.chkEnableSound, false);
				SetCheckbox(ygo::mainGame->tabSettings.chkEnableMusic, false);
			} else if(PARAM_CHECK('d')) { // Deck
				++i;
				if(i + 1 < argc) { // select deck
					ygo::gGameConfig->lastdeck = ygo::Utils::ToUnicodeIfNeeded(argv[i]);
					continue;
				} else { // open deck
					exit_on_return = !keep_on_return;
					if(i < argc) {
						open_file = true;
						open_file_name = argv[i];
					}
					ClickButton(ygo::mainGame->btnDeckEdit);
					break;
				}
			} else if(PARAM_CHECK('c')) { // Create host
				exit_on_return = !keep_on_return;
				ygo::mainGame->HideElement(ygo::mainGame->wMainMenu);
				ClickButton(ygo::mainGame->btnHostConfirm);
				break;
			} else if(PARAM_CHECK('j')) { // Join host
				exit_on_return = !keep_on_return;
				ygo::mainGame->HideElement(ygo::mainGame->wMainMenu);
				ClickButton(ygo::mainGame->btnJoinHost);
				break;
			} else if(PARAM_CHECK('r')) { // Replay
				exit_on_return = !keep_on_return;
				++i;
				if(i < argc) {
					open_file = true;
					open_file_name = argv[i];
				}
				ClickButton(ygo::mainGame->btnReplayMode);
				if(open_file)
					ClickButton(ygo::mainGame->btnLoadReplay);
				break;
			} else if(PARAM_CHECK('s')) { // Single
				exit_on_return = !keep_on_return;
				++i;
				if(i < argc) {
					open_file = true;
					open_file_name = argv[i];
				}
				ClickButton(ygo::mainGame->btnSingleMode);
				if(open_file)
					ClickButton(ygo::mainGame->btnLoadSinglePlay);
				break;
			}
		} else if(argc == 2 && parameter.size() >= 4) {
			const auto extension = ygo::Utils::GetFileExtension(parameter);
			if(extension == EPRO_TEXT("ydk")) {
				open_file = true;
				open_file_name = epro::path_string{ parameter };
				keep_on_return = true;
				exit_on_return = false;
				ClickButton(ygo::mainGame->btnDeckEdit);
				break;
			}
			if(extension == EPRO_TEXT("yrp") || extension == EPRO_TEXT("yrpx")) {
				open_file = true;
				open_file_name = epro::path_string{ parameter };
				keep_on_return = true;
				exit_on_return = false;
				ClickButton(ygo::mainGame->btnReplayMode);
				ClickButton(ygo::mainGame->btnLoadReplay);
				break;
			}
			if(extension == EPRO_TEXT("lua")) {
				open_file = true;
				open_file_name = epro::path_string{ parameter };
				keep_on_return = true;
				exit_on_return = false;
				ClickButton(ygo::mainGame->btnSingleMode);
				ClickButton(ygo::mainGame->btnLoadSinglePlay);
				break;
			}
		}
	}
}
#undef RUN_IF
#undef SET_TXT
#undef PARAM_CHECK

inline void ThreadsStartup() {
	curl_global_init(CURL_GLOBAL_SSL);
#ifdef _WIN32
	const WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;
	WSAStartup(wVersionRequested, &wsaData);
	evthread_use_windows_threads();
#else
	evthread_use_pthreads();
#endif
}
inline void ThreadsCleanup() {
	curl_global_cleanup();
	libevent_global_shutdown();
#ifdef _WIN32
	WSACleanup();
#endif
}

//clang below version 11 (llvm version 8) has a bug with brace class initialization
//where it can't properly deduce the destructors of its members
//https://reviews.llvm.org/D45898
//https://bugs.llvm.org/show_bug.cgi?id=28280
//add a workaround to construct the game object manually still on the stack by using
//a buffer and using in place new
#if defined(__clang_major__) && __clang_major__ <= 10
class StackGame {
	std::aligned_storage_t<sizeof(ygo::Game), alignof(ygo::Game)> game_buf[1];
	ygo::Game* get() { return reinterpret_cast<ygo::Game*>(&game_buf[0]); }
public:
	StackGame() { new (&game_buf[0]) ygo::Game(); }
	~StackGame() { get()->~Game(); }
	ygo::Game* operator&() { return get(); }
};
#else
using StackGame = ygo::Game;
#endif

int _tmain(int argc, epro::path_char* argv[]) {
	epro::path_stringview dest;
	int skipped = 0;
	if(argc > 2 && (argv[1] == EPRO_TEXT("from_discord"_sv) || argv[1] == EPRO_TEXT("-C"_sv))) {
		dest = argv[2];
		skipped = 2;
	} else
		dest = ygo::Utils::GetExeFolder();
	if(!ygo::Utils::ChangeDirectory(dest)) {
		const auto err = fmt::format("failed to change directory to: {}", ygo::Utils::ToUTF8IfNeeded(dest));
		ygo::ErrorLog(err);
		fmt::print("{}\n", err);
		ygo::GUIUtils::ShowErrorWindow("Initialization fail", err);
		return EXIT_FAILURE;
	}
	if(argc >= (2 + skipped) && argv[1 + skipped] == EPRO_TEXT("show_changelog"_sv))
		show_changelog = true;
	ThreadsStartup();
#ifndef _WIN32
	setlocale(LC_CTYPE, "UTF-8");
#endif //_WIN32
	ygo::ClientUpdater updater;
	ygo::gClientUpdater = &updater;
	std::shared_ptr<ygo::DataHandler> data{ nullptr };
	try {
		data = std::make_shared<ygo::DataHandler>(dest);
		ygo::gImageDownloader = data->imageDownloader.get();
		ygo::gDataManager = data->dataManager.get();
		ygo::gSoundManager = data->sounds.get();
		ygo::gGameConfig = data->configs.get();
		ygo::gRepoManager = data->gitManager.get();
		ygo::gdeckManager = data->deckManager.get();
	}
	catch(const std::exception& e) {
		epro::stringview text(e.what());
		ygo::ErrorLog(text);
		fmt::print("{}\n", text);
		ygo::GUIUtils::ShowErrorWindow("Initialization fail", text);
		ThreadsCleanup();
		return EXIT_FAILURE;
	}
	if (!data->configs->noClientUpdates)
		updater.CheckUpdates();
#ifdef _WIN32
	if(!data->configs->showConsole)
		FreeConsole();
#endif
#ifdef EDOPRO_MACOS
	EDOPRO_SetupMenuBar([]() {
		ygo::gGameConfig->fullscreen = !ygo::gGameConfig->fullscreen;
		ygo::mainGame->gSettings.chkFullscreen->setChecked(ygo::gGameConfig->fullscreen);
	});
#endif
	srand(time(0));
	std::unique_ptr<JWrapper> joystick{ nullptr };
	bool firstlaunch = true;
	bool reset = false;
	do {
		StackGame _game{};
		ygo::mainGame = &_game;
		if(data->tmp_device) {
			ygo::mainGame->device = data->tmp_device;
			data->tmp_device = nullptr;
		}
		try {
			ygo::mainGame->Initialize();
		}
		catch(const std::exception& e) {
			epro::stringview text(e.what());
			ygo::ErrorLog(text);
			fmt::print("{}\n", text);
			ygo::GUIUtils::ShowErrorWindow("Assets load fail", text);
			ThreadsCleanup();
			return EXIT_FAILURE;
		}
		if(firstlaunch) {
			joystick = std::unique_ptr<JWrapper>(new JWrapper(ygo::mainGame->device));
			gJWrapper = joystick.get();
			firstlaunch = false;
			CheckArguments(argc - skipped, argv + skipped);
		}
		reset = ygo::mainGame->MainLoop();
		data->tmp_device = ygo::mainGame->device;
		if(reset) {
			data->tmp_device->setEventReceiver(nullptr);
			/*the gles drivers have an additional cache, that isn't cleared when the textures are removed,
			since it's not a big deal clearing them, as they'll be reused, they aren't cleared*/
			/*data->tmp_device->getVideoDriver()->removeAllTextures();*/
			data->tmp_device->getVideoDriver()->removeAllHardwareBuffers();
			data->tmp_device->getVideoDriver()->removeAllOcclusionQueries();
			data->tmp_device->getSceneManager()->clear();
			data->tmp_device->getGUIEnvironment()->clear();
		}
	} while(reset);
	data->tmp_device->drop();
	ThreadsCleanup();
	return EXIT_SUCCESS;
}
