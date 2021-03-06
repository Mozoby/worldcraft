/*
Minetest
Copyright (C) 2013 sapier

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "guiEngine.h"

#include "scripting_mainmenu.h"
#include "config.h"
#include "version.h"
#include "porting.h"
#include "filesys.h"
#include "main.h"
#include "settings.h"
#include "guiMainMenu.h"
#include "sound.h"
#if USE_SOUND
#include "sound_openal.h"
#endif
#include "clouds.h"

#include <IGUIStaticText.h>
#include <ICameraSceneNode.h>

#if USE_CURL
#include <curl/curl.h>
#endif
#include "irrlichttypes.h"

 
using namespace irr;
using namespace irr::gui;



/******************************************************************************/
/** TextDestGUIMainMenu                                                         */
/******************************************************************************/
TextDestGUIMainMenu::TextDestGUIMainMenu(GUIMainMenu* engine)
{
	m_engine = engine;
}

/******************************************************************************/
void TextDestGUIMainMenu::gotText(std::map<std::string, std::string> fields)
{
	m_engine->getScriptIface()->handleMainMenuButtons(fields);
}

/******************************************************************************/
void TextDestGUIMainMenu::gotText(std::wstring text)
{
	m_engine->getScriptIface()->handleMainMenuEvent(wide_to_narrow(text));
}

/******************************************************************************/
/** MenuTextureSource                                                         */
/******************************************************************************/
MenuTextureSource::MenuTextureSource(video::IVideoDriver *driver)
{
	m_driver = driver;
}

/******************************************************************************/
MenuTextureSource::~MenuTextureSource()
{
	for (std::set<std::string>::iterator it = m_to_delete.begin();
			it != m_to_delete.end(); ++it) {
		const char *tname = (*it).c_str();
		video::ITexture *texture = m_driver->getTexture(tname);
		m_driver->removeTexture(texture);
	}
}

/******************************************************************************/
video::ITexture* MenuTextureSource::getTexture(const std::string &name, u32 *id)
{
	if(id)
		*id = 0;
	if(name.empty())
		return NULL;
	m_to_delete.insert(name);
	return m_driver->getTexture(name.c_str());
}

/******************************************************************************/
/** MenuMusicFetcher                                                          */
/******************************************************************************/
void MenuMusicFetcher::fetchSounds(const std::string &name,
			std::set<std::string> &dst_paths,
			std::set<std::string> &dst_datas)
{
	if(m_fetched.count(name))
		return;
	m_fetched.insert(name);
	std::string base;
	base = porting::path_share + DIR_DELIM + "sounds";
	dst_paths.insert(base + DIR_DELIM + name + ".ogg");
	int i;
	for(i=0; i<10; i++)
		dst_paths.insert(base + DIR_DELIM + name + "."+itos(i)+".ogg");
	base = porting::path_user + DIR_DELIM + "sounds";
	dst_paths.insert(base + DIR_DELIM + name + ".ogg");
	for(i=0; i<10; i++)
		dst_paths.insert(base + DIR_DELIM + name + "."+itos(i)+".ogg");
}

/******************************************************************************/
/** GUIMainMenu                                                                 */
/******************************************************************************/
GUIMainMenu::GUIMainMenu(	irr::IrrlichtDevice* dev,
						gui::IGUIElement* parent,
						IMenuManager *menumgr,
						scene::ISceneManager* smgr,
						MainMenuData* data
						) :
	m_device(dev),
	m_parent(parent),
	m_menumanager(menumgr),
	m_smgr(smgr),
	m_data(data),
	m_texture_source(NULL),
	m_sound_manager(NULL),
	m_formspecgui(0),
	m_buttonhandler(0),
	m_menu(0),
	m_kill(false),
	m_startgame(false),
	m_script(0),
	m_scriptdir(""),
	m_irr_toplefttext(0),
	m_clouds_enabled(true),
	m_cloud()
{
	//initialize texture pointers
	for (unsigned int i = 0; i < TEX_LAYER_MAX; i++) {
		m_textures[i] = 0;
	}
	// is deleted by guiformspec!
	m_buttonhandler = new TextDestGUIMainMenu(this);

	//create texture source
	m_texture_source = new MenuTextureSource(m_device->getVideoDriver());

	//create soundmanager
	MenuMusicFetcher soundfetcher;
#if USE_SOUND
	m_sound_manager = createOpenALSoundManager(&soundfetcher);
#endif
	if(!m_sound_manager)
		m_sound_manager = &dummySoundManager;

	//create topleft header
	core::rect<s32> rect(the_screensize.Width * 0.02, the_screensize.Height * 0.02, the_screensize.Width, the_screensize.Height * 0.2);
	rect += v2s32(4, 0);
	std::string t = minetest_version_hash;

	m_irr_toplefttext =
		m_device->getGUIEnvironment()->addStaticText(narrow_to_wide(t).c_str(),
		rect,false,true,0,-1);

	//create formspecsource
	m_formspecgui = new FormspecFormSource("",&m_formspecgui);

	/* Create menu */
	m_menu =
		new GUIFormSpecMenu(	m_device,
								m_parent,
								-1,
								m_menumanager,
								0 /* &client */,
								0 /* gamedef */,
								m_texture_source);

	m_menu->allowClose(false);
    m_menu->lockSize(true,the_screensize);
	m_menu->setFormSource(m_formspecgui);
	m_menu->setTextDest(m_buttonhandler);

	// Initialize scripting

	infostream<<"GUIMainMenu: Initializing Lua"<<std::endl;

	m_script = new MainMenuScripting(this);

	try {
		if (m_data->errormessage != "")
		{
			m_script->setMainMenuErrorMessage(m_data->errormessage);
			m_data->errormessage = "";
		}

		if (!loadMainMenuScript())
			assert("no future without mainmenu" == 0);

//		run();
        cloudInit();
        
        int margin = 5;
        int w = the_screensize.Height * 0.12;
        int h = w;
        int x = the_screensize.Width - w - margin;
        int y = 0 + margin;

        
        infostream << "init main menu success!";
        
        
           
	}
	catch(LuaError &e) {
		errorstream << "MAINMENU ERROR: " << e.what() << std::endl;
		m_data->errormessage = e.what();
	}
}

/******************************************************************************/
bool GUIMainMenu::loadMainMenuScript()
{
	// Try custom menu script (main_menu_script)

	std::string menuscript = g_settings->get("main_menu_script");
	if(menuscript != "") {
		m_scriptdir = fs::RemoveLastPathComponent(menuscript);

		if(m_script->loadMod(menuscript, "__custommenu")) {
			// custom menu script loaded
			return true;
		}
		else {
			infostream
				<< "GUIMainMenu: execution of custom menu failed!"
				<< std::endl
				<< "\tfalling back to builtin menu"
				<< std::endl;
		}
	}

	// Try builtin menu script (main_menu_script)

	std::string builtin_menuscript =
			porting::path_share + DIR_DELIM + "builtin"
				+ DIR_DELIM + "mainmenu.lua";

	m_scriptdir = fs::RemoveRelativePathComponents(
			fs::RemoveLastPathComponent(builtin_menuscript));

	if(m_script->loadMod(builtin_menuscript, "__builtinmenu")) {
		// builtin menu script loaded
		return true;
	}
	else {
		errorstream
			<< "GUIMainMenu: unable to load builtin menu"
			<< std::endl;
	}

	return false;
}

/******************************************************************************/
void GUIMainMenu::run()
{
	// Always create clouds because they may or may not be
	// needed based on the game selected
	video::IVideoDriver* driver = m_device->getVideoDriver();
	
    driver->beginScene(true, true, video::SColor(255,140,186,250));
    
    if (m_clouds_enabled)
    {
        cloudPreProcess();
        drawOverlay(driver);
    }
    else
        drawBackground(driver);
    
    drawHeader(driver);
    drawFooter(driver);
    
    m_device->getGUIEnvironment()->drawAll();
        
    driver->endScene();
    
    if (m_clouds_enabled)
        cloudPostProcess();
    else
        sleep_ms(25);
}

/******************************************************************************/
GUIMainMenu::~GUIMainMenu()
{
    m_menu->quitMenu();
	m_menu->drop();
	m_menu = 0;
	video::IVideoDriver* driver = m_device->getVideoDriver();
	assert(driver != 0);

	if(m_sound_manager != &dummySoundManager){
		delete m_sound_manager;
		m_sound_manager = NULL;
	}

	//TODO: clean up m_menu here

	infostream<<"GUIMainMenu: Deinitializing scripting"<<std::endl;
	delete m_script;

	m_irr_toplefttext->setText(L"");

	//clean up texture pointers
	for (unsigned int i = 0; i < TEX_LAYER_MAX; i++) {
		if (m_textures[i] != 0)
			driver->removeTexture(m_textures[i]);
	}

	delete m_texture_source;
	
	if (m_cloud.clouds)
		m_cloud.clouds->drop();
    
        
}

/******************************************************************************/
void GUIMainMenu::cloudInit()
{
	m_cloud.clouds = new Clouds(m_smgr->getRootSceneNode(),
			m_smgr, -1, rand(), 100);
	m_cloud.clouds->update(v2f(0, 0), video::SColor(255,200,200,255));

	m_cloud.camera = m_smgr->addCameraSceneNode(0,
				v3f(0,0,0), v3f(0, 60, 100));
	m_cloud.camera->setFarValue(10000);

	m_cloud.lasttime = m_device->getTimer()->getTime();
}

/******************************************************************************/
void GUIMainMenu::cloudPreProcess()
{
	u32 time = m_device->getTimer()->getTime();

	if(time > m_cloud.lasttime)
		m_cloud.dtime = (time - m_cloud.lasttime) / 1000.0;
	else
		m_cloud.dtime = 0;

	m_cloud.lasttime = time;

	m_cloud.clouds->step(m_cloud.dtime*3);
	m_cloud.clouds->render();
	m_smgr->drawAll();
}

/******************************************************************************/
void GUIMainMenu::cloudPostProcess()
{
	float fps_max = g_settings->getFloat("fps_max");
	// Time of frame without fps limit
	float busytime;
	u32 busytime_u32;
	// not using getRealTime is necessary for wine
	u32 time = m_device->getTimer()->getTime();
	if(time > m_cloud.lasttime)
		busytime_u32 = time - m_cloud.lasttime;
	else
		busytime_u32 = 0;
	busytime = busytime_u32 / 1000.0;

	// FPS limiter
	u32 frametime_min = 1000./fps_max;

	if(busytime_u32 < frametime_min) {
		u32 sleeptime = frametime_min - busytime_u32;
		m_device->sleep(sleeptime);
	}
}

/******************************************************************************/
void GUIMainMenu::drawBackground(video::IVideoDriver* driver)
{
	v2u32 screensize = driver->getScreenSize();

	video::ITexture* texture = m_textures[TEX_LAYER_BACKGROUND];

	/* If no texture, draw background of solid color */
	if(!texture){
		video::SColor color(255,80,58,37);
		core::rect<s32> rect(0, 0, screensize.X, screensize.Y);
		driver->draw2DRectangle(color, rect, NULL);
		return;
	}

	/* Draw background texture */
	v2u32 sourcesize = texture->getOriginalSize();
	driver->draw2DImage(texture,
		core::rect<s32>(0, 0, screensize.X, screensize.Y),
		core::rect<s32>(0, 0, sourcesize.X, sourcesize.Y),
		NULL, NULL, true);
}

/******************************************************************************/
void GUIMainMenu::drawOverlay(video::IVideoDriver* driver)
{
	v2u32 screensize = driver->getScreenSize();

	video::ITexture* texture = m_textures[TEX_LAYER_OVERLAY];

	/* If no texture, draw background of solid color */
	if(!texture)
		return;

	/* Draw background texture */
	v2u32 sourcesize = texture->getOriginalSize();
	driver->draw2DImage(texture,
		core::rect<s32>(0, 0, screensize.X, screensize.Y),
		core::rect<s32>(0, 0, sourcesize.X, sourcesize.Y),
		NULL, NULL, true);
}

/******************************************************************************/
void GUIMainMenu::drawHeader(video::IVideoDriver* driver)
{
	core::dimension2d<u32> screensize = driver->getScreenSize();

	video::ITexture* texture = m_textures[TEX_LAYER_HEADER];

	/* If no texture, draw nothing */
	if(!texture)
		return;

	f32 mult = (((f32)screensize.Width / 2.0)) /
			((f32)texture->getOriginalSize().Width);

	v2s32 splashsize(((f32)texture->getOriginalSize().Width) * mult,
			((f32)texture->getOriginalSize().Height) * mult);

	// Don't draw the header is there isn't enough room
	s32 free_space = (((s32)screensize.Height)- screensize.Height * 32.0/60)/2;

	if (free_space > splashsize.Y) {
		core::rect<s32> splashrect(0, 0, splashsize.X, splashsize.Y);
		splashrect += v2s32((screensize.Width/2)-(splashsize.X/2),
				((free_space/2)-splashsize.Y/2) * 0.95);

	video::SColor bgcolor(255,50,50,50);

	driver->draw2DImage(texture, splashrect,
		core::rect<s32>(core::position2d<s32>(0,0),
		core::dimension2di(texture->getOriginalSize())),
		NULL, NULL, true);
	}
}

/******************************************************************************/
void GUIMainMenu::drawFooter(video::IVideoDriver* driver)
{
	core::dimension2d<u32> screensize = driver->getScreenSize();

	video::ITexture* texture = m_textures[TEX_LAYER_FOOTER];

	/* If no texture, draw nothing */
	if(!texture)
		return;

	f32 mult = (((f32)screensize.Width)) /
			((f32)texture->getOriginalSize().Width);

	v2s32 footersize(((f32)texture->getOriginalSize().Width) * mult,
			((f32)texture->getOriginalSize().Height) * mult);

	// Don't draw the footer if there isn't enough room
	s32 free_space = (((s32)screensize.Height)-320)/2;

	if (free_space > footersize.Y) {
		core::rect<s32> rect(0,0,footersize.X,footersize.Y);
		rect += v2s32(screensize.Width/2,screensize.Height-footersize.Y);
		rect -= v2s32(footersize.X/2, 0);

		driver->draw2DImage(texture, rect,
			core::rect<s32>(core::position2d<s32>(0,0),
			core::dimension2di(texture->getOriginalSize())),
			NULL, NULL, true);
	}
}

/******************************************************************************/
bool GUIMainMenu::setTexture(texture_layer layer,std::string texturepath) {

	video::IVideoDriver* driver = m_device->getVideoDriver();
	assert(driver != 0);

	if (m_textures[layer] != 0)
	{
		driver->removeTexture(m_textures[layer]);
		m_textures[layer] = 0;
	}

	if ((texturepath == "") || !fs::PathExists(texturepath))
		return false;

	m_textures[layer] = driver->getTexture(texturepath.c_str());

	if (m_textures[layer] == 0) return false;

	return true;
}

/******************************************************************************/
#if USE_CURL
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	FILE* targetfile = (FILE*) userp;
	fwrite(contents,size,nmemb,targetfile);
	return size * nmemb;
}
#endif
bool GUIMainMenu::downloadFile(std::string url,std::string target) {
#if USE_CURL
	//download file via curl
	CURL *curl;

	curl = curl_easy_init();

	if (curl)
	{
		CURLcode res;
		bool retval = true;

		FILE* targetfile = fopen(target.c_str(),"wb");

		if (targetfile) {
			curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, targetfile);
			curl_easy_setopt(curl, CURLOPT_USERAGENT, (std::string("Minetest ")+minetest_version_hash).c_str());
			res = curl_easy_perform(curl);
			if (res != CURLE_OK) {
				errorstream << "File at url \"" << url
					<<"\" not found (" << curl_easy_strerror(res) << ")" <<std::endl;
				retval = false;
			}
			fclose(targetfile);
		}
		else {
			retval = false;
		}

		curl_easy_cleanup(curl);
		return retval;
	}
#endif
	return false;
}

/******************************************************************************/
void GUIMainMenu::setTopleftText(std::string append) {
	std::string toset = minetest_version_hash;

	if (append != "") {
		toset += "";
		toset += append;
	}

	m_irr_toplefttext->setText(narrow_to_wide(toset).c_str());
}

/******************************************************************************/
s32 GUIMainMenu::playSound(SimpleSoundSpec spec, bool looped)
{
	s32 handle = m_sound_manager->playSound(spec, looped);
	return handle;
}

/******************************************************************************/
void GUIMainMenu::stopSound(s32 handle)
{
	m_sound_manager->stopSound(handle);
}
extern std::string g_settings_configpath;
