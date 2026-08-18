// es-app/src/guis/GuiThemeBrowser.cpp
#include "guis/GuiThemeBrowser.h"

#include "Log.h"
#include "Window.h"
#include "resources/Font.h"
#include "utils/FileSystemUtil.h"
#include "renderers/Renderer.h"

// Busy overlay (ya existe y ya anima busy_0..3.svg)
#include "components/BusyComponent.h"

// Popup concreto (implementa Window::InfoPopup)
#include "guis/GuiInfoPopup.h"

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>   // popen/pclose

static inline std::string trim(const std::string& s)
{
	size_t b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) return "";
	size_t e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, (e - b) + 1);
}

static inline bool fileExists(const std::string& path)
{
	return Utils::FileSystem::exists(path);
}

static inline std::string quote(const std::string& s)
{
	return std::string("\"") + s + "\"";
}

static inline std::string ellipsize(const std::string& s, size_t maxLen)
{
	if (s.size() <= maxLen) return s;
	if (maxLen <= 3) return s.substr(0, maxLen);
	return s.substr(0, maxLen - 3) + "...";
}

GuiThemeBrowser::GuiThemeBrowser(Window* window)
	: GuiComponent(window)
	, mFrame(window)
	, mList(window)
	, mPreview(window)
	, mRepoLabel(window, "", Font::get(FONT_SIZE_SMALL), 0x777777CC, ALIGN_LEFT)
	, mHeader(window, "Theme Downloader", Font::get(FONT_SIZE_LARGE), 0xFFFFFFFF, ALIGN_CENTER)
	, mFooter(window, "", Font::get(FONT_SIZE_SMALL), 0xFFFFFFFF, ALIGN_CENTER)
	, mCatalogDir("")
	, mPreviewDir("")
	, mThemesDir("")
	, mLastSelectedIndex(0)
	, mInnerX(0.0f)
	, mInnerY(0.0f)
	, mInnerW(0.0f)
	, mInnerH(0.0f)
	, mBusy(nullptr)
	, mBusyVisible(false)
	, mJobRunning(false)
	, mJobDone(false)
	, mJobOk(false)
	, mJobRefreshCatalogAfter(false)
	, mJobDoneMsg("")
	, mUpdateStop(false)
	, mUpdateDirty(false)
{
	const std::string home = Utils::FileSystem::getHomePath();

	// Catálogo local (snapshot ZIP) en ~/.emulationstation/esx/theme-catalog
	mCatalogDir = home + "/.emulationstation/esx/theme-catalog";

	// Dentro del catálogo: theme-previews/themes.ini + previews
	mPreviewDir = mCatalogDir + "/theme-previews";

	// Temas instalados
	mThemesDir  = home + "/.emulationstation/themes";

	const float sw = (float)Renderer::getScreenWidth();
	const float sh = (float)Renderer::getScreenHeight();

	// ============================================================
	// Frame general
	// ============================================================
	mFrame.setImagePath(":/frame_dark.png");

	const float marginX = sw * 0.05f;
	const float marginY = sh * 0.06f;

	mFrame.setPosition(Vector3f(marginX, marginY, 0.0f));
	mFrame.setSize(Vector2f(sw - (marginX * 2.0f), sh - (marginY * 2.0f)));
	mFrame.setCornerSize(Vector2f(16.0f, 16.0f));
	mFrame.setOpacity(0.95f);

	// ============================================================
	// Layout interno
	// ============================================================
	mInnerX = marginX + (sw * 0.02f);
	mInnerY = marginY + (sh * 0.02f);
	mInnerW = (sw - (marginX * 2.0f)) - (sw * 0.04f);
	mInnerH = (sh - (marginY * 2.0f)) - (sh * 0.04f);

	// Header
	mHeader.setPosition(mInnerX, mInnerY);
	mHeader.setSize(mInnerW, 40.0f);

	// Footer
	mFooter.setPosition(mInnerX, mInnerY + mInnerH - 32.0f);
	mFooter.setSize(mInnerW, 28.0f);

	const float topPad = 55.0f;
	const float bottomPad = 40.0f;

	// Lista izquierda
	mList.setPosition(mInnerX, mInnerY + topPad);
	mList.setSize(mInnerW * 0.46f, (mInnerH - topPad - bottomPad));

	// Panel derecho
	const float rightX = mInnerX + (mInnerW * 0.50f);
	const float rightY = mInnerY + topPad + 10.0f;
	const float rightW = (mInnerW * 0.46f);
	const float rightH = (mInnerH - topPad - bottomPad);

	// ============================================================
	// Preview (FICHA / FIT - sin distorsión)
	// ============================================================
	const float previewFrac = 0.65f;
	const float pvW = rightW;
	const float pvH = rightH * previewFrac;

	mPreview.setOrigin(0.5f, 0.5f);
	mPreview.setPosition(rightX + pvW * 0.5f, rightY + pvH * 0.5f);
	mPreview.setMaxSize(pvW, pvH);

	// Repo debajo de la imagen
	const float repoY = rightY + pvH + 10.0f;
	mRepoLabel.setPosition(rightX, repoY);
	mRepoLabel.setSize(rightW, rightH - pvH);

	// ============================================================
	// Busy overlay (como CHILD para que no “duerma” sin input)
	// ============================================================
	mBusy.reset(new BusyComponent(mWindow));
	mBusy->setPosition(Vector3f(0.0f, 0.0f, 0.0f));
	mBusy->setSize(Vector2f(sw, sh));
	addChild(mBusy.get());
	setBusyVisible(false);

	// ============================================================
	// Cargar lo que haya ya en disco (sin bloquear)
	// ============================================================
	loadThemes();
	rebuildList();

	mList.onFocusGained();

	updatePreview();
	updateFooter();
	updateHelpPrompts();

	// ✅ Lanza chequeo de updates (no bloquea)
	startUpdateCheck();

	// ============================================================
	// Actualizar catálogo en background (sin congelar UI)
	// ============================================================
	startJob("Updating theme catalog...",
		[this]() { return updateCatalogZipSilent(); },
		"Catalog ready.",
		"Catalog update failed.",
		true /*refreshCatalogAfter*/);
}

GuiThemeBrowser::~GuiThemeBrowser()
{
	stopUpdateCheck();

	if (mJobThread.joinable())
		mJobThread.join();
}

void GuiThemeBrowser::setBusyVisible(bool v)
{
	mBusyVisible = v;
	if (mBusy)
		mBusy->setVisible(v);
}

void GuiThemeBrowser::showPopup(const std::string& msg, int durationMs)
{
	mWindow->setInfoPopup(new GuiInfoPopup(mWindow, msg, durationMs));
}

int GuiThemeBrowser::runCmd(const std::string& cmd)
{
	LOG(LogInfo) << "GuiThemeBrowser cmd: " << cmd;
	return std::system(cmd.c_str());
}

bool GuiThemeBrowser::hasCommand(const std::string& cmd) const
{
	const std::string c = "sh -c \"command -v " + cmd + " >/dev/null 2>&1\"";
	return (std::system(c.c_str()) == 0);
}

// Captura stdout (para git ls-remote / rev-parse)
bool GuiThemeBrowser::runCmdCapture(const std::string& cmd, std::string& out) const
{
	out.clear();

	FILE* pipe = popen(cmd.c_str(), "r");
	if (!pipe)
		return false;

	char buffer[256];
	std::ostringstream ss;

	while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
		ss << buffer;

	int rc = pclose(pipe);
	if (rc != 0)
		return false;

	out = trim(ss.str());
	return !out.empty();
}

// ============================================================
// Catálogo ZIP (SILENT)
// ============================================================
bool GuiThemeBrowser::updateCatalogZipSilent()
{
	const std::string zipUrl = "https://github.com/Renetrox/esx-theme-catalog/archive/refs/heads/main.zip";

	const bool hasUnzip = hasCommand("unzip");
	const bool hasCurl  = hasCommand("curl");
	const bool hasWget  = hasCommand("wget");

	if (!hasUnzip || (!hasCurl && !hasWget))
	{
		LOG(LogWarning) << "GuiThemeBrowser: missing deps for catalog zip update. unzip=" << hasUnzip
		                << " curl=" << hasCurl << " wget=" << hasWget;
		return false;
	}

	const std::string esxDir = Utils::FileSystem::getParent(mCatalogDir);
	Utils::FileSystem::createDirectory(esxDir);

	const std::string zipPath = esxDir + "/theme-catalog.zip";
	const std::string extracted = esxDir + "/esx-theme-catalog-main";

	std::string dlCmd;
	if (hasCurl)
		dlCmd = "curl -L -o " + quote(zipPath) + " " + quote(zipUrl);
	else
		dlCmd = "wget -O " + quote(zipPath) + " " + quote(zipUrl);

	int rc = runCmd(dlCmd);
	if (rc != 0 || !Utils::FileSystem::exists(zipPath))
		return false;

	runCmd("rm -rf " + quote(extracted));
	runCmd("rm -rf " + quote(mCatalogDir));

	rc = runCmd("unzip -q -o " + quote(zipPath) + " -d " + quote(esxDir));
	runCmd("rm -f " + quote(zipPath));

	if (rc != 0 || !Utils::FileSystem::exists(extracted))
		return false;

	rc = runCmd("mv " + quote(extracted) + " " + quote(mCatalogDir));
	if (rc != 0 || !Utils::FileSystem::exists(mCatalogDir))
		return false;

	return true;
}

void GuiThemeBrowser::loadThemes()
{
	mThemes.clear();

	const std::string iniPath = mPreviewDir + "/themes.ini";
	if (!fileExists(iniPath))
		return;

	std::ifstream f(iniPath);
	if (!f.is_open())
		return;

	ThemeEntry current;
	bool inSection = false;

	auto flushSection = [&]()
	{
		if (!inSection) return;
		if (current.id.empty()) return;

		if (current.title.empty())  current.title  = current.id;
		if (current.folder.empty()) current.folder = current.id;

		mThemes.push_back(current);

		current = ThemeEntry{};
		inSection = false;
	};

	std::string line;
	while (std::getline(f, line))
	{
		line = trim(line);
		if (line.empty()) continue;
		if (line[0] == ';' || line[0] == '#') continue;

		if (line.front() == '[' && line.back() == ']')
		{
			flushSection();
			inSection = true;
			current.id = trim(line.substr(1, line.size() - 2));
			continue;
		}

		const size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		if (!inSection) continue;

		const std::string key = trim(line.substr(0, eq));
		const std::string val = trim(line.substr(eq + 1));

		if (key == "title") current.title = val;
		else if (key == "repo") current.repo = val;
		else if (key == "preview_hint") current.previewHint = val;
		else if (key == "folder") current.folder = val;
		else if (key == "author") current.author = val;
	}

	flushSection();

	if (mLastSelectedIndex < 0) mLastSelectedIndex = 0;
	if (mLastSelectedIndex >= (int)mThemes.size()) mLastSelectedIndex = 0;
}

bool GuiThemeBrowser::isInstalled(const ThemeEntry& e) const
{
	const std::string dst = mThemesDir + "/" + e.folder;
	return Utils::FileSystem::exists(dst);
}

// -----------------------------
// Update states (NEW)
// -----------------------------
GuiThemeBrowser::UpdateState GuiThemeBrowser::getUpdateStateFor(const ThemeEntry& e) const
{
	std::lock_guard<std::mutex> lk(mUpdateMutex);
	auto it = mUpdateStates.find(e.folder);
	if (it == mUpdateStates.end())
		return UpdateState::UNKNOWN;
	return it->second;
}

void GuiThemeBrowser::setUpdateStateFor(const ThemeEntry& e, UpdateState st)
{
	{
		std::lock_guard<std::mutex> lk(mUpdateMutex);
		mUpdateStates[e.folder] = st;
	}
	mUpdateDirty = true;
}

std::string GuiThemeBrowser::getThemeLocalCommitPath(const ThemeEntry& e) const
{
	return mThemesDir + "/" + e.folder + "/.esx_commit";
}

std::string GuiThemeBrowser::getThemeLocalRepoPath(const ThemeEntry& e) const
{
	return mThemesDir + "/" + e.folder + "/.esx_repo";
}

bool GuiThemeBrowser::readTextFile(const std::string& path, std::string& out) const
{
	out.clear();
	std::ifstream f(path);
	if (!f.is_open())
		return false;

	std::ostringstream ss;
	ss << f.rdbuf();
	out = trim(ss.str());
	return !out.empty();
}

bool GuiThemeBrowser::writeTextFile(const std::string& path, const std::string& text) const
{
	std::ofstream f(path, std::ios::trunc);
	if (!f.is_open())
		return false;

	f << text << "\n";
	return true;
}

bool GuiThemeBrowser::getInstalledCommit(const ThemeEntry& e, std::string& outCommit) const
{
	return readTextFile(getThemeLocalCommitPath(e), outCommit);
}

bool GuiThemeBrowser::saveInstalledInfo(const ThemeEntry& e, const std::string& repo, const std::string& commit) const
{
	const std::string dst = mThemesDir + "/" + e.folder;
	if (!Utils::FileSystem::exists(dst))
		return false;

	bool ok1 = writeTextFile(getThemeLocalRepoPath(e), repo);
	bool ok2 = writeTextFile(getThemeLocalCommitPath(e), commit);
	return ok1 && ok2;
}

bool GuiThemeBrowser::getRemoteHeadCommit(const std::string& repoUrl, std::string& outCommit) const
{
	outCommit.clear();
	// HEAD apunta a la rama default del repo (simple y universal)
	// git ls-remote <url> HEAD  -> "<hash>\tHEAD"
	std::string cmd = "sh -c " + quote("git ls-remote " + quote(repoUrl) + " HEAD 2>/dev/null");
	std::string out;
	if (!runCmdCapture(cmd, out))
		return false;

	// parse: primer token antes de tab/espacio
	std::istringstream iss(out);
	std::string hash;
	iss >> hash;
	if (hash.size() < 7)
		return false;

	outCommit = hash;
	return true;
}

void GuiThemeBrowser::startUpdateCheck()
{
	stopUpdateCheck();

	mUpdateStop = false;
	mUpdateDirty = false;

	// Poner UNKNOWN para instalados; no molesta para no instalados
	{
		std::lock_guard<std::mutex> lk(mUpdateMutex);
		mUpdateStates.clear();
		for (const auto& e : mThemes)
		{
			if (isInstalled(e))
				mUpdateStates[e.folder] = UpdateState::UNKNOWN;
		}
	}

	mUpdateThread = std::thread([this]() { updateCheckWorker(); });
}

void GuiThemeBrowser::stopUpdateCheck()
{
	mUpdateStop = true;
	if (mUpdateThread.joinable())
		mUpdateThread.join();
}

void GuiThemeBrowser::updateCheckWorker()
{
	// Chequea solo instalados, y solo una vez por “carga/refresco”
	for (const auto& e : mThemes)
	{
		if (mUpdateStop)
			return;

		if (!isInstalled(e))
			continue;

		// si no hay repo en catalogo, no hay qué chequear
		if (e.repo.empty())
		{
			setUpdateStateFor(e, UpdateState::ERROR);
			continue;
		}

		std::string localCommit;
		if (!getInstalledCommit(e, localCommit))
		{
			// Tema instalado antes de esta feature: no sabemos versión
			// (podés elegir marcar UNKNOWN o asumir “update available”; yo lo dejo UNKNOWN)
			setUpdateStateFor(e, UpdateState::UNKNOWN);
			continue;
		}

		std::string remoteCommit;
		if (!getRemoteHeadCommit(e.repo, remoteCommit))
		{
			setUpdateStateFor(e, UpdateState::ERROR);
			continue;
		}

		if (remoteCommit == localCommit)
			setUpdateStateFor(e, UpdateState::UP_TO_DATE);
		else
			setUpdateStateFor(e, UpdateState::UPDATE_AVAILABLE);
	}
}

void GuiThemeBrowser::rebuildList()
{
	mList.clear();

	if (mThemes.empty())
	{
		ComponentListRow row;
		auto txt = std::make_shared<TextComponent>(
			mWindow, "No themes found (themes.ini empty?)",
			Font::get(FONT_SIZE_MEDIUM), 0xFFFFFFFF, ALIGN_LEFT
		);

		row.addElement(txt, false);
		mList.addRow(row, true);
		mLastSelectedIndex = 0;
		return;
	}

	for (int i = 0; i < (int)mThemes.size(); i++)
	{
		const ThemeEntry& e = mThemes[i];
		const bool installed = isInstalled(e);

		// NEW: icono update
		std::string upd = "";
		if (installed)
		{
			switch (getUpdateStateFor(e))
			{
			case UpdateState::UPDATE_AVAILABLE: upd = "⟳  "; break; // update disponible
			case UpdateState::UP_TO_DATE:       upd = "   "; break;
			case UpdateState::ERROR:            upd = "?  "; break;
			case UpdateState::UNKNOWN:          upd = "   "; break;
			}
		}

		ComponentListRow row;

		std::string label = installed ? "☑  " : "☐  ";
		label += upd;
		label += (e.title.empty() ? e.id : e.title);

		auto title = std::make_shared<TextComponent>(
			mWindow, label,
			Font::get(FONT_SIZE_MEDIUM), 0xFFFFFFFF, ALIGN_LEFT
		);

		row.addElement(title, false);

		row.makeAcceptInputHandler([this, i]()
		{
			mLastSelectedIndex = i;
			updatePreview();
			updateFooter();
			updateHelpPrompts();
		});

		mList.addRow(row, (i == mLastSelectedIndex));
	}

	updatePreview();
	updateFooter();
	updateHelpPrompts();
}

std::string GuiThemeBrowser::getPreviewPath(const ThemeEntry& e) const
{
	std::string base = e.previewHint.empty() ? e.id : e.previewHint;

	const std::string p1 = mPreviewDir + "/" + base + ".png";
	if (fileExists(p1)) return p1;

	const std::string p2 = mPreviewDir + "/" + base + ".jpg";
	if (fileExists(p2)) return p2;

	const std::string p3 = mPreviewDir + "/" + base + ".jpeg";
	if (fileExists(p3)) return p3;

	return "";
}

void GuiThemeBrowser::updatePreview()
{
	if (mThemes.empty())
	{
		mHeader.setText("Theme Downloader");
		mPreview.setImage("");
		mRepoLabel.setText("");
		return;
	}

	if (mLastSelectedIndex < 0) mLastSelectedIndex = 0;
	if (mLastSelectedIndex >= (int)mThemes.size()) mLastSelectedIndex = 0;

	const ThemeEntry& e = mThemes[mLastSelectedIndex];

	mHeader.setText(e.title.empty() ? e.id : e.title);

	const std::string preview = getPreviewPath(e);
	mPreview.setImage(preview.empty() ? "" : preview);

	std::string repoLine = e.repo.empty() ? "" : ellipsize(e.repo, 70);
	std::string authorLine = e.author.empty() ? "" : ("by " + e.author);

	if (!repoLine.empty() && !authorLine.empty())
		mRepoLabel.setText(repoLine + "\n" + authorLine);
	else if (!repoLine.empty())
		mRepoLabel.setText(repoLine);
	else
		mRepoLabel.setText(authorLine);
}

void GuiThemeBrowser::updateFooter()
{
	if (mThemes.empty())
	{
		mFooter.setText("B Back");
		return;
	}

	const ThemeEntry& e = mThemes[mLastSelectedIndex];
	const bool installed = isInstalled(e);

	std::string s = "A ";
	s += installed ? "Update" : "Download";
	if (installed) s += "    X Uninstall";
	s += "    B Back";

	mFooter.setText(s);
}

bool GuiThemeBrowser::downloadOrUpdateSilent(const ThemeEntry& e)
{
	if (e.repo.empty())
		return false;

	Utils::FileSystem::createDirectory(mThemesDir);

	const std::string dst = mThemesDir + "/" + e.folder;
	const bool installed = Utils::FileSystem::exists(dst);

	if (installed)
		runCmd("rm -rf " + quote(dst));

	// clone
	const std::string cmd = "git clone --depth 1 " + quote(e.repo) + " " + quote(dst);
	int rc = runCmd(cmd);

	if (rc == 0)
	{
		// NEW: capturar commit antes de borrar .git
		std::string head;
		{
			// git -C "<dst>" rev-parse HEAD
			std::string out;
			std::string c = "sh -c " + quote("git -C " + quote(dst) + " rev-parse HEAD 2>/dev/null");
			if (runCmdCapture(c, out))
				head = trim(out);
		}

		// limpiar .git como siempre
		runCmd("rm -rf " + quote(dst + "/.git"));

		// NEW: guardar info instalada
		if (!head.empty())
			saveInstalledInfo(e, e.repo, head);

		return true;
	}

	return false;
}

bool GuiThemeBrowser::uninstallThemeSilent(const ThemeEntry& e)
{
	const std::string dst = mThemesDir + "/" + e.folder;

	if (!Utils::FileSystem::exists(dst))
		return false;

	const std::string cmd = "rm -rf " + quote(dst);
	const int rc = runCmd(cmd);

	return (rc == 0 && !Utils::FileSystem::exists(dst));
}

void GuiThemeBrowser::startJob(const std::string& busyMsg, std::function<bool()> fn,
                              const std::string& okMsg, const std::string& failMsg,
                              bool refreshCatalogAfter)
{
	if (mJobRunning)
		return;

	mJobRunning = true;
	mJobDone = false;
	mJobOk = false;
	mJobRefreshCatalogAfter = refreshCatalogAfter;

	setBusyVisible(true);
	showPopup(busyMsg, 1200);

	if (mJobThread.joinable())
		mJobThread.join();

	mJobThread = std::thread([this, fn, okMsg, failMsg]() {
		const bool ok = fn();

		{
			std::lock_guard<std::mutex> lk(mJobMutex);
			mJobOk = ok;
			mJobDoneMsg = ok ? okMsg : failMsg;
		}

		mJobDone = true;
		mJobRunning = false;
	});
}

void GuiThemeBrowser::finishJobIfDone()
{
	if (!mJobDone)
		return;

	if (mJobThread.joinable())
		mJobThread.join();

	setBusyVisible(false);

	std::string msg;
	bool ok = false;
	bool refreshCatalog = false;

	{
		std::lock_guard<std::mutex> lk(mJobMutex);
		msg = mJobDoneMsg;
		ok = mJobOk;
		refreshCatalog = mJobRefreshCatalogAfter;
	}

	showPopup(msg, ok ? 2500 : 4500);

	if (refreshCatalog)
	{
		loadThemes();
		rebuildList();
		// NEW: re-chequear updates después de refrescar catálogo
		startUpdateCheck();
	}
	else
	{
		rebuildList();
		// NEW: si instalaste/actualizaste, re-chequear
		startUpdateCheck();
	}

	mJobDone = false;
}

bool GuiThemeBrowser::input(InputConfig* config, Input input)
{
	// ES-X / inspirado en ES-DE:
	// No ejecutar acciones en release.
	// Los eventos con value == 0 solo deben cortar/terminar acciones, no mover el menú.
	if (input.value == 0)
		return GuiComponent::input(config, input);

	// Mientras hay un trabajo en segundo plano, bloqueamos acciones.
	if (mJobRunning)
		return true;

	if (config->isMappedTo("b", input) || config->isMappedTo("back", input))
	{
		mWindow->removeGui(this);
		return true;
	}

	if (mThemes.empty())
		return GuiComponent::input(config, input);

	// ------------------------------------------------------------
	// Navegación manual.
	//
	// No usar mList.input() acá porque en algunos mandos BT/wireless
	// vuelve a interpretar ejes/hats y genera desplazamiento raro.
	//
	// Usamos isMappedLike() para up/down porque algunos mandos wireless
	// no responden bien con isMappedTo() en esta pantalla.
	// ------------------------------------------------------------
	if (config->isMappedLike("up", input))
	{
		mLastSelectedIndex--;

		if (mLastSelectedIndex < 0)
			mLastSelectedIndex = (int)mThemes.size() - 1;

		rebuildList();
		return true;
	}

	if (config->isMappedLike("down", input))
	{
		mLastSelectedIndex++;

		if (mLastSelectedIndex >= (int)mThemes.size())
			mLastSelectedIndex = 0;

		rebuildList();
		return true;
	}

	if (config->isMappedTo("a", input) || config->isMappedTo("start", input))
	{
		const ThemeEntry cur = mThemes[mLastSelectedIndex];
		const bool installed = isInstalled(cur);

		startJob(installed ? "Updating theme..." : "Downloading theme...",
			[this, cur]() { return downloadOrUpdateSilent(cur); },
			"Theme ready.",
			"Download/Update failed.",
			false);

		return true;
	}

	if (config->isMappedTo("x", input) || config->isMappedTo("delete", input))
	{
		const ThemeEntry cur = mThemes[mLastSelectedIndex];

		if (!isInstalled(cur))
		{
			showPopup("Not installed.", 2500);
			return true;
		}

		startJob("Removing theme...",
			[this, cur]() { return uninstallThemeSilent(cur); },
			"Theme removed.",
			"Remove failed.",
			false);

		return true;
	}

	// Importante:
	// No llamar mList.input(config, input).
	// El listado se renderiza y actualiza, pero la navegación la controla este GUI.
	return GuiComponent::input(config, input);
}

void GuiThemeBrowser::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	mFrame.update(deltaTime);
	mList.update(deltaTime);
	mPreview.update(deltaTime);
	mRepoLabel.update(deltaTime);
	mHeader.update(deltaTime);
	mFooter.update(deltaTime);

	// NEW: si el thread marcó cambios de update state, refrescar lista 1 vez
	if (mUpdateDirty)
	{
		mUpdateDirty = false;
		rebuildList();
	}

	finishJobIfDone();
}

void GuiThemeBrowser::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();
	Renderer::setMatrix(trans);

	mFrame.render(trans);
	mHeader.render(trans);
	mList.render(trans);
	mPreview.render(trans);
	mRepoLabel.render(trans);
	mFooter.render(trans);

	renderChildren(trans);
}

// ============================================================
// Help prompts
// ============================================================
std::vector<HelpPrompt> GuiThemeBrowser::getHelpPrompts()
{
	std::vector<HelpPrompt> p;

	p.push_back(HelpPrompt("up/down", "Move"));

	const bool hasThemes = !mThemes.empty();
	const bool installed = (hasThemes ? isInstalled(mThemes[mLastSelectedIndex]) : false);

	p.push_back(HelpPrompt("a", installed ? "Update" : "Download"));

	if (installed)
		p.push_back(HelpPrompt("x", "Uninstall"));

	p.push_back(HelpPrompt("b", "Back"));

	return p;
}

void GuiThemeBrowser::updateHelpPrompts()
{
	HelpStyle style;
	mWindow->setHelpPrompts(getHelpPrompts(), style);
}
