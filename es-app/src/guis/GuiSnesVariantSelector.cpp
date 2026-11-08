#include "guis/GuiSnesVariantSelector.h"

#include "FileData.h"
#include "LocaleES.h"
#include "renderers/Renderer.h"
#include "Settings.h"
#include "SystemData.h"
#include "ThemeData.h"
#include "utils/FileSystemUtil.h"
#include "utils/Randomizer.h"
#include "utils/StringUtil.h"
#include "utils/TimeUtil.h"
#include "views/ViewController.h"
#include "views/gamelist/ISimpleGameListView.h"
#include "components/MultiLineMenuEntry.h"
#include "Window.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace
{
	const std::string TRANSLATION_ROW_ID = "__snes_translation_lane__";
	const std::string ACCESSORY_ROW_ID = "__snes_accessory_lane__";
	const int FOCUS_PULSE_DURATION_MS = 190;

	std::string getSnesThemeAsset(const std::string& themeRoot,
		const std::initializer_list<std::string>& activeThemeCandidates,
		const std::string& animaticsRelativePath,
		const std::string& fallback = std::string())
	{
		for (const auto& relativePath : activeThemeCandidates)
		{
			const std::string path = themeRoot + "/" + relativePath;
			if (Utils::FileSystem::exists(path))
				return path;
		}

		if (!themeRoot.empty() && !animaticsRelativePath.empty())
		{
			const std::string activeThemePath = themeRoot + "/" + animaticsRelativePath;
			if (Utils::FileSystem::exists(activeThemePath))
				return activeThemePath;
		}

		if (!themeRoot.empty() && !animaticsRelativePath.empty())
		{
			const std::string animaticsRoot = Utils::FileSystem::getParent(themeRoot) + "/es-theme-Animatics-DX";
			const std::string animaticsPath = animaticsRoot + "/" + animaticsRelativePath;
			if (Utils::FileSystem::exists(animaticsPath))
				return animaticsPath;
		}

		return fallback;
	}

	std::string getSnesKeyedThemeAsset(const std::string& themeRoot,
		const std::string& mediaType,
		const std::string& mediaKey,
		const std::initializer_list<std::string>& extensions)
	{
		if (themeRoot.empty() || mediaType.empty() || mediaKey.empty())
			return std::string();

		for (const auto& extension : extensions)
		{
			const std::string path = themeRoot + "/_inc/retrobat-snes/media/by-system/" +
				mediaType + "/" + mediaKey + "." + extension;
			if (Utils::FileSystem::exists(path))
				return path;
		}
		return std::string();
	}

	std::string getSnesMediaKey(FolderData* folder)
	{
		if (folder == nullptr)
			return "snes";

		if (folder->getSystem() != nullptr)
		{
			const std::string systemId = Utils::String::toLower(folder->getSystem()->getName());
			if (systemId == "sgb" || systemId == "sgb-msu1" || systemId == "snes-msu1" ||
				systemId == "satellaview" || systemId == "sufami")
				return systemId;
		}

		const std::string folderName = Utils::String::toLower(folder->getFileName());
		const std::string parentName = folder->getParent() != nullptr
			? Utils::String::toLower(folder->getParent()->getFileName()) : std::string();
		const std::string folderKey = Utils::String::toLower(folder->getName() + " " + folder->getPath() + " " +
			folder->getMetadata().get("name") + " " + folder->getMetadata().get("desc"));
		if ((folderKey.find("sgb") != std::string::npos || folderKey.find("super game boy") != std::string::npos ||
			folderKey.find("supergameboy") != std::string::npos) && folderKey.find("msu") != std::string::npos)
			return "sgb-msu1";
		if (folderKey.find("snes-msu") != std::string::npos || folderKey.find("snes msu") != std::string::npos ||
			folderKey.find("super nintendo msu") != std::string::npos)
			return "snes-msu1";
		if (folderKey.find("satellaview") != std::string::npos || folderKey.find("satelliteview") != std::string::npos ||
			folderKey.find("bs-x") != std::string::npos || folderKey.find("bsx") != std::string::npos)
			return "satellaview";
		if (folderKey.find("sufami") != std::string::npos)
			return "sufami";
		if (folderKey.find("super game boy") != std::string::npos || folderKey.find("supergameboy") != std::string::npos ||
			folderKey.find("/sgb") != std::string::npos || folderName == "sgb")
			return "sgb";
		if (parentName.find("translat") != std::string::npos)
		{
			if (folderName.find("english") != std::string::npos || folderName.find("ingles") != std::string::npos)
				return "snes-translation-en";
			if (folderName.find("portug") != std::string::npos || folderName.find("brazil") != std::string::npos || folderName.find("brasil") != std::string::npos)
				return "snes-translation-pt";
			if (folderName.find("spanish") != std::string::npos || folderName.find("espanol") != std::string::npos || folderName.find("castellano") != std::string::npos)
				return "snes-translation-es";
			return "snes-translation";
		}
		if (folderName.find("hack") != std::string::npos || parentName.find("hack") != std::string::npos)
			return "snes-hacks";
		if (folderName == "usa" || folderName == "us") return "snes-usa";
		if (folderName == "europe" || folderName == "europa") return "snes-europe";
		if (folderName == "japan" || folderName == "japon") return "snes-japan";
		return "snes";
	}

	std::string getSnesFolderKey(FolderData* folder)
	{
		if (folder == nullptr)
			return std::string();

		return Utils::String::toLower(folder->getName() + " " + folder->getPath() + " " + folder->getMetadata().get("name") + " " + folder->getMetadata().get("desc"));
	}

	std::string getSnesFolderDisplayName(FolderData* folder)
	{
		if (folder == nullptr)
			return std::string();
		if (folder->getSystem() != nullptr)
		{
			const std::string systemName = folder->getSystem()->getName();
			if (systemName == "sgb") return "SUPER GAME BOY";
			if (systemName == "snes-msu1") return "SNES MSU-1";
			if (systemName == "sgb-msu1") return "SGB MSU-1";
			if (systemName == "satellaview") return "SATELLAVIEW";
			if (systemName == "sufami") return "SUFAMI TURBO";
		}

		const std::string folderName = Utils::String::toLower(folder->getFileName());
		FileData* parent = folder->getParent();
		const bool isTranslationLanguage = parent != nullptr &&
			Utils::String::toLower(parent->getFileName()).find("translat") != std::string::npos;
		if (isTranslationLanguage)
		{
			if (folderName == "english") return "TRADUCCIÓN · INGLÉS";
			if (folderName == "portugues" || folderName == "portuguese") return "BRASIL";
			if (folderName == "spanish" || folderName == "espanol") return "TRADUCCIÓN · ESPAÑOL";
			return "TRADUCCIÓN · " + Utils::String::toUpper(folder->getName());
		}
		if (folderName == "usa") return "REGIÓN USA";
		if (folderName == "europe" || folderName == "europa") return "REGIÓN EUROPA";
		if (folderName == "japan" || folderName == "japon") return "REGIÓN JAPÓN";
		if (folderName == "hacks") return "HACKS";
		if (folderName == "translates") return "TRADUCCIONES";
		return folder->getMetadata().get("name").empty() ? folder->getName() : folder->getMetadata().get("name");
	}

	std::string getSnesTranslationFlagPath(FolderData* folder, const std::string& themeRoot)
	{
		if (folder == nullptr || folder->getParent() == nullptr ||
			Utils::String::toLower(folder->getParent()->getFileName()).find("translat") == std::string::npos)
			return std::string();

		const std::string language = Utils::String::toLower(folder->getFileName());
		std::string flag;
		if (language.find("english") != std::string::npos || language.find("ingles") != std::string::npos ||
			language == "usa" || language == "us" || language.find("united states") != std::string::npos || language.find("estados unidos") != std::string::npos) flag = "us";
		else if (language.find("portug") != std::string::npos || language.find("brazil") != std::string::npos || language.find("brasil") != std::string::npos) flag = "br";
		else if (language.find("spanish") != std::string::npos || language.find("espanol") != std::string::npos ||
			language.find("español") != std::string::npos || language.find("castellano") != std::string::npos) flag = "es";
		else return std::string();

		const std::string brushFlag = themeRoot + "/_inc/retrobat-snes/media/flags/" + flag + "-brush.png";
		if (Utils::FileSystem::exists(brushFlag))
			return brushFlag;
		const std::string themeOverride = themeRoot + "/_inc/images/flags/" + flag + ".png";
		return Utils::FileSystem::exists(themeOverride) ? themeOverride : ":/flags/" + flag + ".png";
	}

	std::string getSnesThemeRoot(SystemData* system)
	{
		const std::string activeThemeRoot = ThemeData::getCurrentThemeRootPath();
		if (!activeThemeRoot.empty())
			return activeThemeRoot;

		if (system == nullptr)
			return std::string();

		const std::string systemThemeRoot = Utils::FileSystem::getParent(system->getThemePath());
		const std::string themeSetRoot = Utils::FileSystem::getParent(systemThemeRoot);
		if (Utils::FileSystem::exists(themeSetRoot + "/_inc/images/flags/us.png"))
			return themeSetRoot;
		return systemThemeRoot;
	}

	std::string getSnesOfficialFlagPath(FolderData* folder, const std::string& themeRoot)
	{
		const std::string key = getSnesFolderKey(folder);
		std::string flag;
		if (key.find("usa") != std::string::npos || key.find("america") != std::string::npos)
			flag = "us";
		else if (key.find("europ") != std::string::npos)
			flag = "eu";
		else if (key.find("jap") != std::string::npos)
			flag = "jp";
		else
			return std::string();

		const std::string brushFlag = themeRoot + "/_inc/retrobat-snes/media/flags/" + flag + "-brush.png";
		if (Utils::FileSystem::exists(brushFlag))
			return brushFlag;
		const std::string themeOverride = themeRoot + "/_inc/images/flags/" + flag + ".png";
		return Utils::FileSystem::exists(themeOverride) ? themeOverride : ":/flags/" + flag + ".png";
	}

	std::string getGameCountLabel(FolderData* folder)
	{
		const size_t count = folder == nullptr ? 0 : folder->getFilesRecursive(GAME).size();
		return std::to_string(count) + (count == 1 ? " JUEGO" : " JUEGOS");
	}

	std::string getRegionalGameImagePath(FileData* game, FolderData* regionFolder)
	{
		if (game == nullptr || regionFolder == nullptr)
			return std::string();

		auto isImageAsset = [](const std::string& path)
		{
			const std::string stem = Utils::String::toLower(Utils::FileSystem::getStem(path));
			return Utils::String::endsWith(stem, "-image");
		};

		// Gamelists can point at a valid image outside the region's ROM tree.
		// Accept it only when it is the requested game-image asset, never fanart
		// or a thumbnail.
		const std::string metadataImage = game->getMetadata(MetaDataId::Image);
		if (!metadataImage.empty() && isImageAsset(metadataImage) && Utils::FileSystem::exists(metadataImage))
			return metadataImage;

		const std::string imageDirectory = regionFolder->getPath() + "/images/";
		if (!metadataImage.empty() && isImageAsset(metadataImage))
		{
			const std::string metadataFilename = Utils::FileSystem::getFileName(metadataImage);
			const std::string regionalMetadataImage = imageDirectory + metadataFilename;
			if (Utils::FileSystem::exists(regionalMetadataImage))
				return regionalMetadataImage;
		}

		// Local art lookup normally starts at the whole SNES system root. The
		// regional ROMs and their media live one level deeper (USA/images,
		// Europe/images, Japan/images), so resolve the conventional -image asset
		// beside the selected region instead of accidentally using general art.
		for (const auto& extension : { ".png", ".jpg", ".jpeg" })
		{
			const std::string regionalImage = imageDirectory + game->getDisplayName() + "-image" + extension;
			if (Utils::FileSystem::exists(regionalImage))
				return regionalImage;
		}

		return std::string();
	}

	void collectTranslationFolders(FolderData* parent, std::vector<FolderData*>& variants)
	{
		if (parent == nullptr)
			return;

		for (auto child : parent->getChildren())
		{
			if (child->getType() != FOLDER)
				continue;

			auto folder = static_cast<FolderData*>(child);
			if (folder->getFilesRecursive(GAME).empty())
				continue;

			bool hasPlayableChildFolder = false;
			for (auto nested : folder->getChildren())
			{
				if (nested->getType() == FOLDER &&
					!static_cast<FolderData*>(nested)->getFilesRecursive(GAME).empty())
				{
					hasPlayableChildFolder = true;
					break;
				}
			}

			if (hasPlayableChildFolder)
				collectTranslationFolders(folder, variants);
			else
				variants.push_back(folder);
		}
	}

	std::vector<FolderData*> getPlayableSnesVariants(SystemData* system)
	{
		std::vector<FolderData*> variants;
		if (system == nullptr || system->getRootFolder() == nullptr)
			return variants;

		for (auto child : system->getRootFolder()->getChildren())
		{
			if (child->getType() != FOLDER)
				continue;

			auto folder = static_cast<FolderData*>(child);
			const std::string name = Utils::String::toLower(folder->getFileName());
			if (name.find("translat") != std::string::npos || name.find("traducc") != std::string::npos)
				collectTranslationFolders(folder, variants);
			else if (!folder->getFilesRecursive(GAME).empty())
				variants.push_back(folder);
		}

		return variants;
	}

	bool isSnesAccessory(FolderData* folder)
	{
		if (folder != nullptr && folder->getSystem() != nullptr)
		{
			const std::string systemName = folder->getSystem()->getName();
			if (systemName == "sgb" || systemName == "satellaview" || systemName == "sufami" ||
				systemName == "snes-msu1" || systemName == "sgb-msu1")
				return true;
		}

		const std::string key = getSnesFolderKey(folder);
		return key.find("super-game") != std::string::npos ||
			key.find("super game") != std::string::npos ||
			key.find("supergameboy") != std::string::npos ||
			key.find("satell") != std::string::npos ||
			key.find("sufami") != std::string::npos ||
			key.find("msu") != std::string::npos ||
			key.find("bs-x") != std::string::npos ||
			key.find("bsx") != std::string::npos ||
			key.find("acces") != std::string::npos ||
			key.find("expansi") != std::string::npos;
	}

	std::string getSnesVariantId(SystemData* system, FolderData* folder)
	{
		if (system == nullptr || folder == nullptr)
			return std::string();

		std::string id = Utils::FileSystem::createRelativePath(folder->getPath(), system->getRootFolder()->getPath(), false);
		if (Utils::String::startsWith(id, "./"))
			id.erase(0, 2);
		if (Utils::String::startsWith(id, ".\\"))
			id.erase(0, 2);
		return id;
	}

	std::string formatPlayedDate(const std::string& value)
	{
		if (value.empty())
			return "NUNCA";

		Utils::Time::DateTime date = value;
		if (!date.isValid())
			return "NUNCA";

		return Utils::Time::timeToString(date.getTime(), "%x");
	}

	class SectionLedRunner : public GuiComponent
	{
	public:
		SectionLedRunner(Window* window, float x, float y, float width, float height,
			float borderWidth, unsigned int accentColor, float phase)
			: GuiComponent(window), mWidth(width), mHeight(height), mBorderWidth(borderWidth),
			  mAccentColor(accentColor), mPhase(phase)
		{
			setPosition(x, y);
			setSize(width, height);
			setZIndex(53.0f);
			const float segmentLength = std::min(42.0f, std::max(18.0f, 2.0f * (width + height) * 0.018f));
			const float thickness = std::max(2.0f, borderWidth * 0.72f);
			auto makeSegment = [this, window, segmentLength, thickness](float lengthFactor, float thicknessFactor, unsigned int alpha)
			{
				auto segment = std::make_shared<ImageComponent>(window, true, false);
				segment->setImage(std::string(":/white.png"), true);
				segment->setOrigin(0.5f, 0.5f);
				segment->setColorShift((mAccentColor & 0xFFFFFF00) | alpha);
				segment->setZIndex(0.0f);
				segment->setResize(lengthFactor * segmentLength, thicknessFactor * thickness);
				addChild(segment.get());
				mSegments.push_back(segment);
			};
			makeSegment(1.30f, 3.2f, 0x38);
			makeSegment(1.00f, 2.0f, 0x98);
			makeSegment(0.52f, 0.9f, 0xFF);
			updateLed(0);
		}

		void update(int deltaTime) override
		{
			GuiComponent::update(deltaTime);
			if (!isVisible())
				return;
			mElapsed += std::max(0, deltaTime);
			updateLed(mElapsed);
		}

	private:
		void updateLed(int elapsed)
		{
			const float perimeter = 2.0f * (mWidth + mHeight);
			if (perimeter <= 0.0f)
				return;
			const float distance = std::fmod(static_cast<float>(elapsed) * 0.20f + mPhase * perimeter, perimeter);
			float centerX = 0.0f;
			float centerY = 0.0f;
			bool vertical = false;
			if (distance < mWidth)
			{
				centerX = distance;
				centerY = mBorderWidth * 0.5f;
			}
			else if (distance < mWidth + mHeight)
			{
				vertical = true;
				centerX = mWidth - mBorderWidth * 0.5f;
				centerY = distance - mWidth;
			}
			else if (distance < mWidth * 2.0f + mHeight)
			{
				centerX = mWidth - (distance - mWidth - mHeight);
				centerY = mHeight - mBorderWidth * 0.5f;
			}
			else
			{
				vertical = true;
				centerX = mBorderWidth * 0.5f;
				centerY = mHeight - (distance - mWidth * 2.0f - mHeight);
			}

			for (const auto& segment : mSegments)
			{
				segment->setRotationDegrees(vertical ? 90.0f : 0.0f);
				segment->setPosition(centerX, centerY);
			}
		}
		float mWidth;
		float mHeight;
		float mBorderWidth;
		unsigned int mAccentColor;
		float mPhase;
		int mElapsed = 0;
		std::vector<std::shared_ptr<ImageComponent>> mSegments;
	};
}

bool GuiSnesVariantSelector::hasVariants(SystemData* system)
{
	return !getPlayableSnesVariants(system).empty();
}

FolderData* GuiSnesVariantSelector::getDefaultVariant(SystemData* system)
{
	if (system == nullptr)
		return nullptr;

	FolderData* firstEligible = nullptr;
	FolderData* usa = nullptr;
	const std::string stored = Settings::getInstance()->getString("snes.defaultVariant");
	for (auto folder : getPlayableSnesVariants(system))
	{
		if (!isDefaultEligible(folder))
			continue;

		if (firstEligible == nullptr)
			firstEligible = folder;

		if (!stored.empty() && (stored == getSnesVariantId(system, folder) || stored == folder->getName()))
			return folder;

		if (usa == nullptr)
		{
			const std::string key = getSnesFolderKey(folder);
			if (key.find("usa") != std::string::npos || key.find("america") != std::string::npos)
				usa = folder;
		}
	}

	FolderData* fallback = usa != nullptr ? usa : firstEligible;
	if (fallback != nullptr && !stored.empty() &&
		stored != getSnesVariantId(system, fallback) && stored != fallback->getName())
	{
		Settings::getInstance()->setString("snes.defaultVariant", getSnesVariantId(system, fallback));
		Settings::getInstance()->saveFile();
	}
	return fallback;
}

bool GuiSnesVariantSelector::isDefaultEligible(FolderData* folder)
{
	if (folder == nullptr || isSnesAccessory(folder))
		return false;

	const std::string key = getSnesFolderKey(folder);
	FileData* parent = folder->getParent();
	if ((parent != nullptr && Utils::String::toLower(parent->getFileName()).find("translat") != std::string::npos) ||
		key.find("hack") != std::string::npos || key.find("homebrew") != std::string::npos ||
		key.find("modific") != std::string::npos || key.find("traducc") != std::string::npos ||
		key.find("translat") != std::string::npos)
		return false;

	return key.find("usa") != std::string::npos || key.find("america") != std::string::npos ||
		key.find("europa") != std::string::npos || key.find("europe") != std::string::npos ||
		key.find("jap") != std::string::npos || key.find("japon") != std::string::npos ||
		key.find("japan") != std::string::npos;
}

FolderData* GuiSnesVariantSelector::getSelectedFolder()
{
	if (mMenu.getList() == nullptr)
		return nullptr;

	const std::string selectedId = mMenu.getList()->getSelectedUserData();
	if (selectedId == TRANSLATION_ROW_ID)
		return mTranslationCursor >= 0 && mTranslationCursor < static_cast<int>(mTranslationFolders.size())
			? mTranslationFolders[mTranslationCursor] : nullptr;
	if (selectedId == ACCESSORY_ROW_ID)
		return mAccessoryCursor >= 0 && mAccessoryCursor < static_cast<int>(mAccessoryFolders.size())
			? mAccessoryFolders[mAccessoryCursor] : nullptr;

	for (auto folder : mVariants)
		if (folder != nullptr && folder->getPath() == selectedId)
			return folder;

	return nullptr;
}

bool GuiSnesVariantSelector::setDefaultVariant(SystemData* system, FolderData* folder)
{
	if (system == nullptr || !isDefaultEligible(folder))
		return false;

	const std::string id = getSnesVariantId(system, folder);
	if (!id.empty())
	{
		Settings::getInstance()->setString("snes.defaultVariant", id);
		Settings::getInstance()->saveFile();
		// Do not reload while this modal is still in the GUI stack. The complete
		// view refresh is deferred until closeSelector(), after the modal has
		// been removed, otherwise the TV/dossier can be left half-rendered.
		return true;
	}

	return false;
}

void GuiSnesVariantSelector::closeSelector()
{
	Window* window = mWindow;
	const bool reloadView = mDefaultChanged;

	// Queue the rebuild before deleting this GUI, but let it run on the next UI
	// turn. At that point the selector is no longer covering the SystemView and
	// reloadAll() can recreate the roulette, hardware animation and dossier as
	// one consistent scene.
	if (reloadView && window != nullptr)
	{
		window->postToUiThread([window]
		{
			ViewController::get()->reloadAll(window, true);
		});
	}

	delete this;
}

void GuiSnesVariantSelector::openVariant(SystemData* system, FolderData* folder)
{
	if (system == nullptr || folder == nullptr)
		return;

    // Change the active view first.  goToGameList() can rebuild the list while
    // completing the transition; opening the folder before it is selected
    // gets overwritten and leaves the user at the root/empty intermediate list.
    ViewController::get()->goToGameList(system, true);

    auto gameList = ViewController::get()->getGameListView(system);
    auto simpleGameList = std::dynamic_pointer_cast<ISimpleGameListView>(gameList);
    if (simpleGameList == nullptr)
        return;

    simpleGameList->openVariantFolder(folder);
}

GuiSnesVariantSelector::GuiSnesVariantSelector(Window* window, SystemData* system)
	: GuiComponent(window), mMenu(window, "", ThemeData::getMenuTheme()->Title.font), mSystem(system),
	  mModalBackdrop(window, true, false), mCard(window), mCardInnerFrame(window),
	  mPreviewBackdrop(window, true, false), mCenterFanart(window, true, false), mCenterFigurine(window, true, false),
	  mPreviewPanel(window), mPreviewScreenFrame(window, true, false), mPreviewScreenSurface(window, true, false),
	  mPreviewLogo(window, true, false), mPreviewGameArt(window, true, false), mPreviewRegionFlag(window, true, false),
	  mPreviewScreenImage(window, true, false), mPreviewEmptyArt(window, true, false),
	  mPreviewLibraryIcon(window, true, false), mPreviewFavoritesIcon(window, true, false),
	  mPreviewPlayedIcon(window, true, false), mPreviewLastPlayedIcon(window, true, false), mPreviewEmptyMessage(window),
	  mPreviewKind(window), mPreviewTitle(window), mPreviewDescription(window),
	  mPreviewLibraryLabel(window), mPreviewLibraryValue(window),
	  mPreviewFolderLabel(window), mPreviewFolderValue(window),
	  mPreviewStatusLabel(window), mPreviewStatusValue(window),
	  mPreviewLastLabel(window), mPreviewLastValue(window), mPreviewHint(window),
	  mPreviewGoldRule(window), mPreviewCyanRule(window)
{
	const float screenWidth = (float)Renderer::getScreenWidth();
	const float screenHeight = (float)Renderer::getScreenHeight();
	// Give the selection surface room to breathe while retaining a visible frame
	// of the system carousel around it.
	setSize(screenWidth * 0.84f, screenHeight * 0.97f);
	setPosition((screenWidth - mSize.x()) / 2.0f, (screenHeight - mSize.y()) / 2.0f);
	// Give the category panels room to read as a single selection surface while
	// leaving the existing live preview and its multimedia in a separate column.
	const float selectionSurfaceWidth = mSize.x() * 0.355f;
	// Expand only the accessories lane into the open center space, stopping
	// before the technical preview column at 0.695 of the modal width.
	const float accessorySurfaceWidth = mSize.x() * 0.660f;
	mMenu.setMaxWidth(selectionSurfaceWidth);
	// The selector is a full-height stack of category cards; omit the old title
	// and reclaim its header space for the actual choices.
	mMenu.setTitleHeightOverride(screenHeight * 0.008f);
	mMenu.setTitle("");
	// MenuComponent normally paints an opaque themed rectangle behind its rows.
	// The selector panels provide their own translucent surfaces, so keep the
	// native menu background transparent and let the selected system fanart show.
	if (mMenu.getChildCount() > 0)
	{
		if (auto* menuBackground = dynamic_cast<NinePatchComponent*>(mMenu.getChild(0)))
		{
			menuBackground->setCenterColor(0x00000000);
			menuBackground->setEdgeColor(0x00000000);
			menuBackground->setVisible(false);
		}
	}
	// Style the existing title component in place so no duplicate heading is added.
	if (mMenu.getChildCount() > 1)
	{
		auto* menuGrid = dynamic_cast<ComponentGrid*>(mMenu.getChild(1));
		if (menuGrid != nullptr && menuGrid->getChildCount() > 0)
		{
			auto* headerGrid = dynamic_cast<ComponentGrid*>(menuGrid->getChild(0));
			if (headerGrid != nullptr && headerGrid->getChildCount() > 0)
			{
				if (auto* title = dynamic_cast<TextComponent*>(headerGrid->getChild(0)))
				{
					title->setText("");
					title->setVisible(false);
				}
			}
		}
	}

	// The selector is its own opaque surface: do not show the main carousel
	// fanart through the modal or duplicate it behind the selection rows.
	mModalBackdrop.setImage(std::string(":/white.png"), true);
	mModalBackdrop.setResize(screenWidth, screenHeight);
	mModalBackdrop.setColorShift(0x080D17FF);
	mModalBackdrop.setPosition(-getPosition().x(), -getPosition().y());

	// The native menu remains the navigation surface. The card is a live
	// presentation of the highlighted variant, not a second navigation layer.
	const std::string themeRoot = getSnesThemeRoot(mSystem);
	// These are flat color surfaces, not nine-patch artwork. Using the 1x1
	// white texture through NinePatchComponent gives it 16px corner slices and
	// invalid UV ranges, which made the intended selector card disappear.
	mCard.setImage(std::string(":/white.png"), true);
	mCard.setColorShift(0x0B1424FF);
	mCard.setPosition(0, 0);
	mCard.setResize(mSize.x(), mSize.y());
	mCardInnerFrame.setImagePath(":/white.png");
	mCardInnerFrame.setCenterColor(0x00000000);
	mCardInnerFrame.setEdgeColor(0x6AC7D420);
	mCardInnerFrame.setPosition(mSize.x() * 0.004f, mSize.y() * 0.006f);
	mCardInnerFrame.setSize(mSize.x() * 0.992f, mSize.y() * 0.988f);

	// Each variant owns the visual identity of the whole card. The selected
	// fanart is therefore a cropped panel background, not a small thumbnail.
	mPreviewBackdrop.setPosition(mSize.x() * 0.010f, mSize.y() * 0.010f);
	mPreviewBackdrop.setMinSize(mSize.x() * 0.980f, mSize.y() * 0.980f);
	mPreviewBackdrop.setColorShift(0xFFFFFF45);
	mPreviewBackdrop.setAllowFading(false);
	mPreviewBackdrop.setVisible(false);
	// Cover the complete modal with translucent system fanart. The 16:9 source
	// is center-cropped to the modal bounds, preserving its proportions; panels,
	// text, figurines, and the technical preview remain separate foreground layers.
	mCenterFanart.setOrigin(0.5f, 0.5f);
	mCenterFanart.setPosition(mSize.x() * 0.5f, mSize.y() * 0.5f);
	mCenterFanart.setMinSize(mSize.x(), mSize.y());
	mCenterFanart.setAllowFading(false);
	mCenterFanart.setOpacity(54);
	// Keep the Animatics DX figurine at its original, normal visibility.
	mCenterFigurine.setOrigin(0.5f, 0.5f);
	mCenterFigurine.setPosition(mSize.x() * 0.525f, mSize.y() * 0.390f);
	mCenterFigurine.setMaxSize(mSize.x() * 0.280f, mSize.y() * 0.660f);
	mCenterFigurine.setAllowFading(false);
	mCenterFigurine.setOpacity(238);

	mPreviewPanel.setImage(std::string(":/white.png"), true);
	// Keep the technical-sheet surface dark enough for legibility while letting
	// the full-modal fanart subtly show through; its text/art are separate layers.
	mPreviewPanel.setColorShift(0x111820B0);
	mPreviewPanel.setPosition(mSize.x() * 0.695f, mSize.y() * 0.055f);
	mPreviewPanel.setResize(mSize.x() * 0.285f, mSize.y() * 0.910f);

	// The TV is a standalone cutout of the active theme's Sony Trinitron art.
	// Its transparent screen aperture reveals the live game image rendered below it.
	const float tvCenterX = mSize.x() * 0.8375f;
	const float tvCenterY = mSize.y() * 0.535f;
	const float tvMaxWidth = mSize.x() * 0.280f;
	const float tvMaxHeight = mSize.y() * 0.320f;
	const float tvAspect = 1.5f;
	const float tvCanvasWidth = std::min(tvMaxWidth, tvMaxHeight * tvAspect);
	const float tvCanvasHeight = tvCanvasWidth / tvAspect;
	const float tvScreenCenterX = tvCenterX;
	const float tvScreenCenterY = tvCenterY - tvCanvasHeight * 0.069f;
	mPreviewScreenFrame.setImage(themeRoot + "/_inc/retrobat-snes/media/snes-trinitron-tv-overlay.png");
	mPreviewScreenFrame.setOrigin(0.5f, 0.5f);
	mPreviewScreenFrame.setPosition(tvCenterX, tvCenterY);
	mPreviewScreenFrame.setMaxSize(tvCanvasWidth, tvCanvasHeight);
	mPreviewScreenFrame.setAllowFading(false);
	mPreviewScreenFrame.setVisible(true);
	// Derive the display opening from the final rendered frame, not the monitor
	// aspect ratio; otherwise a wide dark strip remains behind the CRT.
	const Vector2f tvRenderedSize = mPreviewScreenFrame.getSize();
	const float tvScreenWidth = tvRenderedSize.x() * 0.543f;
	const float tvScreenHeight = tvRenderedSize.y() * 0.578f;
	const float miniScreenInset = std::max(2.0f, screenHeight * 0.0015f);
	mPreviewScreenSurface.setImage(std::string(":/white.png"), true);
	mPreviewScreenSurface.setColorShift(0x07101AFF);
	mPreviewScreenSurface.setOrigin(0.5f, 0.5f);
	mPreviewScreenSurface.setPosition(tvScreenCenterX, tvScreenCenterY);
	mPreviewScreenSurface.setResize(tvScreenWidth - miniScreenInset * 2.0f, tvScreenHeight - miniScreenInset * 2.0f);
	mPreviewScreenSurface.setVisible(true);

	mPreviewLogo.setOrigin(0.5f, 0.5f);
	mPreviewLogo.setPosition(mSize.x() * 0.8375f, mSize.y() * 0.1225f);
	mPreviewLogo.setMaxSize(mSize.x() * 0.245f, mSize.y() * 0.075f);
	mPreviewLogo.setAllowFading(false);
	mPreviewGameArt.setOrigin(0.5f, 0.5f);
	// Reuse the original transparent console/accessory art at a larger scale;
	// fit it proportionally without a frame or a generated replacement.
	mPreviewGameArt.setPosition(mSize.x() * 0.8375f, mSize.y() * 0.755f);
	mPreviewGameArt.setMaxSize(mSize.x() * 0.245f, mSize.y() * 0.180f);
	mPreviewGameArt.setAllowFading(false);
	mPreviewRegionFlag.setOrigin(0.5f, 0.5f);
	mPreviewRegionFlag.setMaxSize(mSize.x() * 0.045f, mSize.y() * 0.042f);
	mPreviewRegionFlag.setAllowFading(false);
	mPreviewRegionFlag.setVisible(false);
	mPreviewScreenImage.setOrigin(0.5f, 0.5f);
	mPreviewScreenImage.setPosition(tvScreenCenterX, tvScreenCenterY);
	// Fill the CRT aperture while preserving the game's aspect ratio and
	// center-cropping only the small excess around the edges.
	mPreviewScreenImage.setMinSize(tvScreenWidth - miniScreenInset * 2.0f, tvScreenHeight - miniScreenInset * 2.0f);
	mPreviewScreenImage.setAllowFading(false);
	mPreviewScreenImage.setVisible(false);
	mPreviewEmptyArt.setOrigin(0.5f, 0.5f);
	mPreviewEmptyArt.setPosition(tvScreenCenterX, tvScreenCenterY - tvScreenHeight * 0.12f);
	mPreviewEmptyArt.setMaxSize(tvScreenWidth * 0.70f, tvScreenHeight * 0.58f);
	mPreviewEmptyArt.setAllowFading(false);
	mPreviewEmptyMessage.setFont(Font::get(static_cast<unsigned int>(screenHeight * 0.010f)));
	mPreviewEmptyMessage.setColor(0xF1F7FFFF);
	mPreviewEmptyMessage.setText("TODAVÍA NO HAS JUGADO\nEN ESTA CONSOLA");
	mPreviewEmptyMessage.setHorizontalAlignment(ALIGN_CENTER);
	mPreviewEmptyMessage.setVerticalAlignment(ALIGN_CENTER);
	mPreviewEmptyMessage.setLineSpacing(1.0f);
	mPreviewEmptyMessage.setBackgroundColor(0x08111AE8);
	mPreviewEmptyMessage.setRenderBackground(true);
	mPreviewEmptyMessage.setPosition(tvScreenCenterX - tvScreenWidth * 0.47f,
		tvScreenCenterY + tvScreenHeight * 0.22f);
	mPreviewEmptyMessage.setSize(tvScreenWidth * 0.94f, tvScreenHeight * 0.16f);

	const float libraryCountCenterY = tvScreenCenterY;
	const float libraryIconSize = screenHeight * 0.030f;
	mPreviewLibraryIcon.setVisible(false);
	mPreviewLibraryIcon.setOrigin(0.5f, 0.5f);
	mPreviewLibraryIcon.setPosition(mSize.x() * 0.730f, libraryCountCenterY);
	mPreviewLibraryIcon.setMaxSize(libraryIconSize, libraryIconSize);
	mPreviewLibraryIcon.setColorShift(0x6AC7D4FF);
	mPreviewLibraryIcon.setAllowFading(false);
	const std::string statsIconRoot = themeRoot + "/_inc/retrobat-snes/media";
	const float statsIconSize = screenHeight * 0.018f;
	const float statsTop = mSize.y() * 0.864f;
	const float statsRowGap = mSize.y() * 0.031f;
	const float statsIconX = mSize.x() * 0.724f;
	auto setupStatsIcon = [&](ImageComponent& icon, const std::string& fileName, unsigned int color, float row)
	{
		icon.setImage(statsIconRoot + "/" + fileName);
		icon.setOrigin(0.5f, 0.5f);
		icon.setPosition(statsIconX, statsTop + statsRowGap * row + statsIconSize * 0.55f);
		icon.setMaxSize(statsIconSize, statsIconSize);
		icon.setColorShift(color);
		icon.setAllowFading(false);
	};
	setupStatsIcon(mPreviewFavoritesIcon, "icon-heart.svg", 0xE6A0A8FF, 0.0f);
	setupStatsIcon(mPreviewLastPlayedIcon, "icon-clock.svg", 0x6AC7D4FF, 1.0f);
	setupStatsIcon(mPreviewPlayedIcon, "icon-gamepad.svg", 0xC9D7E8FF, 2.0f);

	const auto compactFont = Font::get(static_cast<unsigned int>(screenHeight * 0.020f));
	const auto microFont = Font::get(static_cast<unsigned int>(screenHeight * 0.015f));
	const auto titleFont = Font::get(static_cast<unsigned int>(screenHeight * 0.026f));
	mPreviewKind.setFont(compactFont);
	mPreviewKind.setColor(0xFFD451FF);
	mPreviewTitle.setFont(titleFont);
	mPreviewTitle.setColor(0xFFFFFFFF);
	mPreviewDescription.setFont(microFont);
	mPreviewDescription.setColor(0xFFFFFFFF);
	mPreviewDescription.setLineSpacing(1.12f);
	mPreviewLibraryLabel.setFont(microFont);
	mPreviewLibraryLabel.setColor(0xE6D6A7FF);
	mPreviewLibraryValue.setFont(compactFont);
	mPreviewLibraryValue.setColor(0xFFFFFFFF);
	mPreviewFolderLabel.setFont(microFont);
	mPreviewFolderLabel.setColor(0xE6D6A7FF);
	mPreviewFolderValue.setFont(microFont);
	mPreviewFolderValue.setColor(0xC9D7E8FF);

	const float previewLeft = mSize.x() * 0.715f;
	const float previewWidth = mSize.x() * 0.245f;
	mPreviewKind.setPosition(previewLeft, mSize.y() * 0.190f);
	mPreviewKind.setSize(previewWidth, mSize.y() * 0.026f);
	mPreviewTitle.setPosition(previewLeft, mSize.y() * 0.230f);
	mPreviewTitle.setSize(previewWidth, mSize.y() * 0.040f);
	mPreviewDescription.setPosition(previewLeft, mSize.y() * 0.285f);
	mPreviewDescription.setSize(previewWidth, mSize.y() * 0.080f);
	// Keep the legacy total fields hidden; center the CRT in the preview panel.
	mPreviewLibraryLabel.setVisible(false);
	mPreviewLibraryValue.setVisible(false);
	mPreviewLibraryValue.setPosition(mSize.x() * 0.748f, libraryCountCenterY - mSize.y() * 0.020f);
	mPreviewLibraryValue.setSize(mSize.x() * 0.045f, mSize.y() * 0.040f);
	mPreviewLibraryValue.setVerticalAlignment(ALIGN_CENTER);
	// Compact vertical activity summary: each metric has its own pictogram.
	const float statsTextLeft = mSize.x() * 0.741f;
	const float statsLabelWidth = mSize.x() * 0.116f;
	const float statsValueLeft = mSize.x() * 0.860f;
	const float statsValueWidth = mSize.x() * 0.095f;
	const float statsTextHeight = mSize.y() * 0.024f;
	mPreviewFolderLabel.setPosition(statsTextLeft, statsTop + statsRowGap * 2.0f);
	mPreviewFolderLabel.setSize(statsLabelWidth, statsTextHeight);
	mPreviewFolderValue.setPosition(statsValueLeft, statsTop + statsRowGap * 2.0f);
	mPreviewFolderValue.setSize(statsValueWidth, statsTextHeight);
	mPreviewFolderValue.setHorizontalAlignment(ALIGN_RIGHT);

	const float previewRuleHeight = std::max(2.0f, screenHeight * 0.0025f);
	mPreviewGoldRule.setImage(std::string(":/white.png"), true);
	mPreviewGoldRule.setResize(previewWidth * 0.22f, previewRuleHeight);
	mPreviewGoldRule.setVisible(false);
	mPreviewGoldRule.setColorShift(0xC9A74BFF);
	mPreviewCyanRule.setImage(std::string(":/white.png"), true);
	mPreviewCyanRule.setResize(previewWidth * 0.22f, previewRuleHeight);
	mPreviewCyanRule.setPosition(Vector3f(previewLeft, mSize.y() * 0.850f, 0));
	mPreviewCyanRule.setColorShift(0x6AC7D4FF);

	mPreviewStatusLabel.setFont(microFont);
	mPreviewStatusLabel.setColor(0xD9C889FF);
	mPreviewStatusLabel.setPosition(Vector3f(statsTextLeft, statsTop, 0));
	mPreviewStatusLabel.setSize(statsLabelWidth, statsTextHeight);
	mPreviewStatusValue.setFont(compactFont);
	mPreviewStatusValue.setColor(0xFFFFFFFF);
	mPreviewStatusValue.setPosition(Vector3f(statsValueLeft, statsTop, 0));
	mPreviewStatusValue.setSize(statsValueWidth, statsTextHeight);
	mPreviewStatusValue.setHorizontalAlignment(ALIGN_RIGHT);
	mPreviewLastLabel.setFont(microFont);
	mPreviewLastLabel.setColor(0xD9C889FF);
	mPreviewLastLabel.setPosition(Vector3f(statsTextLeft, statsTop + statsRowGap, 0));
	mPreviewLastLabel.setSize(statsLabelWidth, statsTextHeight);
	mPreviewLastValue.setFont(microFont);
	mPreviewLastValue.setColor(0xFFFFFFFF);
	mPreviewLastValue.setPosition(Vector3f(statsValueLeft, statsTop + statsRowGap, 0));
	mPreviewLastValue.setSize(statsValueWidth, statsTextHeight);
	mPreviewLastValue.setHorizontalAlignment(ALIGN_RIGHT);
	mPreviewHint.setFont(microFont);
	mPreviewHint.setColor(0xB7DDE2FF);
	mPreviewHint.setPosition(Vector3f(previewLeft, mSize.y() * 0.910f, 0));
	mPreviewHint.setSize(previewWidth, mSize.y() * 0.022f);

	addChild(&mModalBackdrop);
	addChild(&mCard);
	addChild(&mCardInnerFrame);
	addChild(&mPreviewBackdrop);
	addChild(&mCenterFanart);
	addChild(&mCenterFigurine);
	addChild(&mPreviewPanel);
	addChild(&mPreviewScreenFrame);
	addChild(&mPreviewScreenSurface);
	addChild(&mPreviewLogo);
	addChild(&mPreviewGameArt);
	addChild(&mPreviewRegionFlag);
	addChild(&mPreviewScreenImage);
	addChild(&mPreviewEmptyArt);
	addChild(&mPreviewLibraryIcon);
	addChild(&mPreviewFavoritesIcon);
	addChild(&mPreviewLastPlayedIcon);
	addChild(&mPreviewPlayedIcon);
	addChild(&mPreviewEmptyMessage);
	addChild(&mPreviewKind);
	addChild(&mPreviewTitle);
	addChild(&mPreviewDescription);
	addChild(&mPreviewLibraryLabel);
	addChild(&mPreviewLibraryValue);
	addChild(&mPreviewFolderLabel);
	addChild(&mPreviewFolderValue);
	addChild(&mPreviewStatusLabel);
	addChild(&mPreviewStatusValue);
	addChild(&mPreviewLastLabel);
	addChild(&mPreviewLastValue);
	addChild(&mPreviewHint);
	addChild(&mPreviewGoldRule);
	addChild(&mPreviewCyanRule);
	addChild(&mMenu);

	// GuiComponent renders children in ascending z order, so the last layer
	// is the visual foreground. Keep the blackout behind the card and the
	// native menu above the preview.
	mModalBackdrop.setZIndex(-100.0f);
	mCard.setZIndex(-50.0f);
	mCardInnerFrame.setZIndex(-49.0f);
	mPreviewBackdrop.setZIndex(-40.0f);
	mCenterFanart.setZIndex(-45.0f);
	mCenterFigurine.setZIndex(8.0f);
	mPreviewPanel.setZIndex(0.0f);
	mPreviewScreenSurface.setZIndex(15.0f);
	mPreviewScreenImage.setZIndex(16.0f);
	mPreviewEmptyArt.setZIndex(16.0f);
	mPreviewEmptyMessage.setZIndex(17.0f);
	mPreviewScreenFrame.setZIndex(21.0f);
	mPreviewLibraryIcon.setZIndex(30.0f);
	mPreviewFavoritesIcon.setZIndex(30.0f);
	mPreviewLastPlayedIcon.setZIndex(30.0f);
	mPreviewPlayedIcon.setZIndex(30.0f);
	// The regional console showcase is created only for official variants that
	// actually contain games, then placed in the free strip below the selector.
	mPreviewLogo.setZIndex(20.0f);
	mPreviewGameArt.setZIndex(20.0f);
	mPreviewRegionFlag.setZIndex(22.0f);
	mPreviewGoldRule.setZIndex(25.0f);
	mPreviewCyanRule.setZIndex(25.0f);
	mPreviewKind.setZIndex(30.0f);
	mPreviewTitle.setZIndex(30.0f);
	mPreviewDescription.setZIndex(30.0f);
	mPreviewLibraryLabel.setZIndex(30.0f);
	mPreviewLibraryValue.setZIndex(30.0f);
	mPreviewFolderLabel.setZIndex(30.0f);
	mPreviewFolderValue.setZIndex(30.0f);
	mPreviewStatusLabel.setZIndex(30.0f);
	mPreviewStatusValue.setZIndex(30.0f);
	mPreviewLastLabel.setZIndex(30.0f);
	mPreviewLastValue.setZIndex(30.0f);
	mPreviewHint.setZIndex(30.0f);
	mMenu.setZIndex(50.0f);

	mMenu.getList()->setCursorChangedCallback([this](CursorState) { updatePreview(); });
	// Draw the cyan rails inside the focused row, behind its media and text.
	// This is opt-in and affects only the SNES selector's list.
	mMenu.getList()->setSelectionLines(0x58E7F4D8, screenHeight * 0.008f,
		std::max(2.0f, screenHeight * 0.0025f));
	mMenu.setPosition(mSize.x() * 0.025f, mSize.y() * 0.012f);

	if (system == nullptr)
		return;

	std::vector<FolderData*> officialFolders;
	std::vector<FolderData*> hackFolders;
	std::vector<FolderData*> accessoryFolders;
	std::vector<FolderData*> otherFolders;
	for (auto folder : getPlayableSnesVariants(system))
	{
		mVariants.push_back(folder);
		const std::string key = getSnesFolderKey(folder);
		FileData* parent = folder->getParent();
		const bool translation = parent != nullptr && Utils::String::toLower(parent->getFileName()).find("translat") != std::string::npos;
		if (translation)
			mTranslationFolders.push_back(folder);
		else if (isSnesAccessory(folder))
			accessoryFolders.push_back(folder);
		else if (key.find("hack") != std::string::npos || key.find("homebrew") != std::string::npos || key.find("modific") != std::string::npos)
			hackFolders.push_back(folder);
		else if (isDefaultEligible(folder))
			officialFolders.push_back(folder);
		else
			otherFolders.push_back(folder);
	}
	// These accessories remain real, independently launchable libraries so
	// each keeps its configured emulator and progress. Surface their roots here
	// without copying ROMs or creating duplicate rows when SNES already has an
	// equivalent accessory folder.
	const std::vector<std::string> accessorySystemIds = { "sgb", "satellaview", "sufami", "snes-msu1", "sgb-msu1" };
	for (const auto& systemId : accessorySystemIds)
	{
		auto accessorySystem = SystemData::getSystem(systemId);
		auto accessoryRoot = accessorySystem != nullptr ? accessorySystem->getRootFolder() : nullptr;
		const bool alreadyListed = std::any_of(accessoryFolders.begin(), accessoryFolders.end(), [&systemId](FolderData* folder)
		{
			if (folder != nullptr && folder->getSystem() != nullptr && folder->getSystem()->getName() == systemId)
				return true;
			const std::string key = getSnesFolderKey(folder);
			if (systemId == "sgb")
				return key.find("super-game") != std::string::npos || key.find("super game") != std::string::npos ||
					key.find("supergameboy") != std::string::npos || key.find("sgb") != std::string::npos;
			if (systemId == "satellaview")
				return key.find("satell") != std::string::npos || key.find("bs-x") != std::string::npos || key.find("bsx") != std::string::npos;
			if (systemId == "snes-msu1")
				return key.find("snes") != std::string::npos && key.find("msu") != std::string::npos;
			if (systemId == "sgb-msu1")
				return (key.find("sgb") != std::string::npos || key.find("gameboy") != std::string::npos || key.find("game boy") != std::string::npos) &&
					key.find("msu") != std::string::npos;
			return key.find("sufami") != std::string::npos;
		});
		if (accessoryRoot != nullptr && !accessoryRoot->getFilesRecursive(GAME).empty() && !alreadyListed)
		{
			accessoryFolders.push_back(accessoryRoot);
			mVariants.push_back(accessoryRoot);
		}
	}
	mAccessoryFolders = accessoryFolders;
	auto regionalOrder = [](FolderData* left, FolderData* right)
	{
		auto rank = [](FolderData* folder)
		{
			const std::string key = getSnesFolderKey(folder);
			if (key.find("usa") != std::string::npos || key.find("america") != std::string::npos) return 0;
			if (key.find("europ") != std::string::npos) return 1;
			if (key.find("jap") != std::string::npos) return 2;
			return 3;
		};
		return rank(left) < rank(right);
	};
	std::stable_sort(officialFolders.begin(), officialFolders.end(), regionalOrder);
	std::stable_sort(mTranslationFolders.begin(), mTranslationFolders.end(), [](FolderData* left, FolderData* right)
	{
		return Utils::String::toLower(left->getFileName()) < Utils::String::toLower(right->getFileName());
	});
	for (auto folder : officialFolders)
	{
		const std::string region = getSnesFolderKey(folder);
		std::string hardwareArt;
		std::string fanart;
		if (region.find("usa") != std::string::npos || region.find("america") != std::string::npos)
		{
			hardwareArt = getSnesThemeAsset(themeRoot,
				{ "art/consoles/us/snes.png", "_inc/arts/snes US.png", "_inc/retrobat-snes/media/hardware/us-snes.png" },
				"art/consoles/us/snes.png");
			fanart = getSnesThemeAsset(themeRoot,
				{ "art/background/us/snes.jpg", "_inc/retrobat-snes/media/fanart/us-snes.jpg" },
				"system/snes/snes.jpg");
		}
		else if (region.find("jap") != std::string::npos)
		{
			hardwareArt = getSnesThemeAsset(themeRoot,
				{ "art/consoles/jp/snes.png", "_inc/arts/snes JP.png", "_inc/retrobat-snes/media/hardware/jp-snes.png" },
				"art/consoles/jp/snes.png");
			fanart = getSnesThemeAsset(themeRoot,
				{ "art/background/jp/snes.jpg", "_inc/retrobat-snes/media/fanart/jp-snes.jpg" },
				"system/snes/snes.jpg");
		}
		else if (region.find("europ") != std::string::npos)
		{
			hardwareArt = getSnesThemeAsset(themeRoot,
				{ "art/consoles/snes.png", "_inc/arts/snes.png", "_inc/retrobat-snes/media/hardware/eu-snes.png" },
				"art/consoles/snes.png");
			fanart = getSnesThemeAsset(themeRoot,
				{ "art/background/snes.jpg", "_inc/retrobat-snes/media/fanart/snes.jpg" },
				"system/snes/snes.jpg");
		}
		else
			continue;

		if (!Utils::FileSystem::exists(hardwareArt))
			continue;

		auto hardware = std::make_shared<ImageComponent>(window, true, false);
		hardware->setImage(hardwareArt);
		hardware->setOrigin(0.5f, 0.5f);
		hardware->setAllowFading(false);
		hardware->setZIndex(60.0f);
		addChild(hardware.get());
		mShowcaseHardware.emplace_back(hardware);

		auto screen = std::make_shared<ImageComponent>(window, true, false);
		screen->setOrigin(0.5f, 0.5f);
		screen->setAllowFading(false);
		auto games = folder->getFilesRecursive(GAME);
		FileData* lastPlayedGame = nullptr;
		std::string latestPlayed;
		std::vector<std::string> availableGameImages;
		if (!games.empty())
		{
			for (auto game : games)
			{
				const std::string lastPlayed = game->getMetadata(MetaDataId::LastPlayed);
				// Unplayed entries resolve to the metadata default "0", not an empty string.
				if (!lastPlayed.empty() && lastPlayed != "0" && (lastPlayedGame == nullptr || lastPlayed > latestPlayed))
				{
					latestPlayed = lastPlayed;
					lastPlayedGame = game;
				}

				const std::string image = getRegionalGameImagePath(game, folder);
				if (!image.empty() && Utils::FileSystem::exists(image))
					availableGameImages.push_back(image);
			}
		}

		std::string selectedGameImage;
		if (lastPlayedGame != nullptr)
			selectedGameImage = getRegionalGameImagePath(lastPlayedGame, folder);
		else if (!availableGameImages.empty())
			selectedGameImage = availableGameImages[Randomizer::random(static_cast<int>(availableGameImages.size()))];

		if (!selectedGameImage.empty() && Utils::FileSystem::exists(selectedGameImage))
		{
			screen->setImage(selectedGameImage);
			screen->setVisible(true);
		}
		else
			screen->setVisible(false);
		screen->setZIndex(61.0f);
		addChild(screen.get());
		mShowcaseScreens.emplace_back(screen);
	}
	// Fit the complete selector into the visible menu viewport. MenuComponent
	// otherwise drops rows after maxHeight and turns the remaining rows into a
	// scrolling list, which hides translation folders on shorter screens.
	// Reserve the lower strip for the fixed official-console hardware showcase; the
	// content scale above keeps every selector row visible without scrolling.
	const float menuMaxHeight = mSize.y() * 0.950f;
	mMenu.setMaxHeight(menuMaxHeight);
	const float listViewportHeight = std::max(1.0f, menuMaxHeight - mMenu.getTitleHeight() - mMenu.getButtonGridHeight() - 8.0f);
	// Keep the previously approved horizontal sizing reference unchanged while
	// the selector gains vertical room below.
	const float horizontalReferenceViewportHeight = std::max(1.0f, screenHeight * 0.88f * 0.735f -
		Font::get(static_cast<unsigned int>(screenHeight * 0.030f))->getHeight() - mMenu.getButtonGridHeight() - 8.0f);
	const float selectionContentWidth = mSize.x() * 0.335f;
	const size_t sectionCount = (!officialFolders.empty() ? 1 : 0) + (!hackFolders.empty() ? 1 : 0) +
		(!mTranslationFolders.empty() ? 1 : 0) + (!accessoryFolders.empty() ? 1 : 0) + (!otherFolders.empty() ? 1 : 0);
	const size_t sectionGapCount = sectionCount > 0 ? sectionCount - 1 : 0;
	auto innerRowGapCount = [](size_t rowCount) { return rowCount > 1 ? rowCount - 1 : 0; };
	// Accessories are rendered together in one horizontal lane, so they do not
	// consume vertical inter-option gaps in the fitted selector height.
	const size_t renderedOptionRowGapCount = innerRowGapCount(officialFolders.size()) +
		innerRowGapCount(hackFolders.size()) + innerRowGapCount(otherFolders.size());
	const float baseRowsHeight =
		static_cast<float>(officialFolders.size()) * screenHeight * 0.185f +
		static_cast<float>(hackFolders.size()) * screenHeight * 0.120f +
		(!mTranslationFolders.empty() ? screenHeight * 0.120f : 0.0f) +
		(!accessoryFolders.empty() ? screenHeight * 0.220f : 0.0f) +
		static_cast<float>(otherFolders.size()) * screenHeight * 0.075f +
		static_cast<float>(sectionCount) * ThemeData::getMenuTheme()->Group.font->getHeight() +
		static_cast<float>(sectionGapCount) * screenHeight * 0.032f +
		static_cast<float>(renderedOptionRowGapCount) * screenHeight * 0.012f;
	// Preserve the existing horizontal origins/translation columns while the
	// vertical fit is recalculated using the actual rendered header height.
	const float horizontalLayoutScale = baseRowsHeight > 0.0f
		? std::min(1.0f, horizontalReferenceViewportHeight / (baseRowsHeight * 1.08f))
		: 1.0f;
	const float sectionHeaderHeight = screenHeight * 0.030f;
	const float sectionDividerHeightUnscaled = screenHeight * 0.0025f;
	const float sectionGapHeight = screenHeight * 0.060f;
	const float optionRowGapHeight = screenHeight * 0.026f;
	const float sectionContentPadding = screenHeight * 0.010f;
	const float fixedVerticalGaps = static_cast<float>(sectionGapCount) * sectionGapHeight +
		static_cast<float>(renderedOptionRowGapCount) * optionRowGapHeight +
		static_cast<float>(sectionCount) * sectionContentPadding * 2.0f;
	const float estimatedRowsHeight =
		static_cast<float>(officialFolders.size()) * screenHeight * 0.205f +
		static_cast<float>(hackFolders.size()) * screenHeight * 0.150f +
		(!mTranslationFolders.empty() ? screenHeight * 0.180f : 0.0f) +
		(!accessoryFolders.empty() ? screenHeight * 0.220f : 0.0f) +
		static_cast<float>(otherFolders.size()) * screenHeight * 0.085f +
		static_cast<float>(sectionCount) * sectionHeaderHeight;
	const float fixedSectionHeight = static_cast<float>(sectionCount) * std::max(sectionHeaderHeight,
		ThemeData::getMenuTheme()->Group.font->getHeight());
	const float scalableRowsHeight = std::max(1.0f,
		estimatedRowsHeight - static_cast<float>(sectionCount) * sectionHeaderHeight);
	const float layoutScale = std::min(1.0f,
		std::max(0.0f, listViewportHeight - fixedSectionHeight - fixedVerticalGaps - screenHeight * 0.010f) /
		(scalableRowsHeight * 1.02f));

	auto addFocusIcon = [this, window, screenHeight, horizontalLayoutScale, themeRoot](const std::string& rowId, bool pointToNextColumn)
	{
		auto icon = std::make_shared<ImageComponent>(window, true);
		const std::string themeHandPath = themeRoot + "/resources/hand_pointer.png";
		icon->setImage(Utils::FileSystem::exists(themeHandPath) ? themeHandPath : ":/hand_pointer.png");
		icon->setOrigin(pointToNextColumn ? 1.0f : 0.0f, 0.0f);
		icon->setColorShift(0xF5E5B8FF);
	icon->setMaxSize(screenHeight * 0.055f * std::max(0.72f, horizontalLayoutScale),
		screenHeight * 0.055f * std::max(0.72f, horizontalLayoutScale));
		icon->setVisible(false);
		mFocusIcons.emplace_back(rowId, icon);
		return icon;
	};
	auto addOptionRowGap = [this, window, selectionSurfaceWidth, optionRowGapHeight]()
	{
		ComponentListRow gap;
		auto spacer = std::make_shared<GuiComponent>(window);
		spacer->setSize(selectionSurfaceWidth, optionRowGapHeight);
		gap.addElement(spacer, false);
		gap.selectable = false;
		mMenu.addRow(gap, false, true);
	};
	struct SectionAccentSpan
	{
		std::shared_ptr<ImageComponent> line;
		int firstRow;
		int lastRow = -1;
		unsigned int color;
		float width;
	};
	std::vector<SectionAccentSpan> sectionAccentSpans;
	int activeSectionAccent = -1;
	std::shared_ptr<GuiComponent> finalSectionBreathingRoom;
	const float selectorLeftInset = screenHeight * 0.018f;
	const float selectorHandWidth = screenHeight * 0.055f * horizontalLayoutScale;
	const float selectorHandToCardGap = screenHeight * 0.012f;
	auto addSectionGap = [this, window, selectionSurfaceWidth, screenHeight, layoutScale, sectionGapHeight, sectionContentPadding,
		&sectionAccentSpans, &activeSectionAccent](unsigned int accentColor)
	{
		if (activeSectionAccent >= 0)
		{
			ComponentListRow breathingRoom;
			auto spacer = std::make_shared<GuiComponent>(window);
			spacer->setSize(selectionSurfaceWidth, sectionContentPadding);
			breathingRoom.addElement(spacer, false);
			breathingRoom.selectable = false;
			mMenu.addRow(breathingRoom, false, true);
			sectionAccentSpans[activeSectionAccent].lastRow = mMenu.getList()->size() - 1;
			activeSectionAccent = -1;
		}
		const float dividerHeight = std::max(2.0f, screenHeight * 0.0025f * layoutScale);
		auto addSpacer = [this, window, selectionSurfaceWidth](float height)
		{
			ComponentListRow spacerRow;
			auto spacer = std::make_shared<GuiComponent>(window);
			spacer->setSize(selectionSurfaceWidth, height);
			spacerRow.addElement(spacer, false);
			spacerRow.selectable = false;
			mMenu.addRow(spacerRow, false, true);
		};
		addSpacer(std::max(1.0f, (sectionGapHeight - dividerHeight) * 0.5f));
		ComponentListRow dividerRow;
		auto separator = std::make_shared<ImageComponent>(window, true);
		separator->setImage(std::string(":/white.png"), true);
		separator->setResize(selectionSurfaceWidth, dividerHeight);
		separator->setColorShift((accentColor & 0xFFFFFF00) | 0x9A);
		dividerRow.addElement(separator, false);
		dividerRow.selectable = false;
		mMenu.addRow(dividerRow, false, true);
		addSpacer(std::max(1.0f, (sectionGapHeight - dividerHeight) * 0.5f));
	};
	auto addSectionHeader = [this, window, screenHeight, themeRoot, layoutScale, sectionHeaderHeight, selectionSurfaceWidth, accessorySurfaceWidth, sectionContentPadding,
		&sectionAccentSpans, &activeSectionAccent](const std::string& title, unsigned int accentColor)
	{
		ComponentListRow section;
		auto headingInset = std::make_shared<GuiComponent>(window);
		headingInset->setSize(screenHeight * 0.018f, 1.0f);
		section.addElement(headingInset, false);
		auto accent = std::make_shared<ImageComponent>(window, true);
		accent->setImage(std::string(":/white.png"), true);
		accent->setResize(std::max(4.0f, screenHeight * 0.006f * layoutScale),
			std::max(screenHeight * 0.024f, sectionHeaderHeight * 0.72f));
		accent->setColorShift(accentColor);
		section.addElement(accent, false);
		auto titleGap = std::make_shared<GuiComponent>(window);
		titleGap->setSize(screenHeight * 0.014f, 1.0f);
		section.addElement(titleGap, false);
		std::string headingFontPath = themeRoot + "/_inc/fonts/RobotoCondensed-Bold.ttf";
		if (!Utils::FileSystem::exists(headingFontPath))
			headingFontPath = ThemeData::getMenuTheme()->Title.font->getPath();
		auto heading = std::make_shared<TextComponent>(window, title,
			Font::get(static_cast<unsigned int>(screenHeight * 0.026f), headingFontPath), 0xEFFBFFFF, ALIGN_LEFT);
		heading->setMultiLine(TextComponent::MultiLineType::SINGLELINE);
		section.addElement(heading, false);
		section.selectable = false;
		section.group = true;
		section.groupColor = accentColor;
		auto sectionLine = std::make_shared<ImageComponent>(window, true);
		sectionLine->setImage(std::string(":/white.png"), true);
		sectionLine->setColorShift(accentColor);
		sectionLine->setAllowFading(false);
		sectionLine->setOrigin(0.0f, 0.0f);
		sectionLine->setZIndex(55.0f);
		const float sectionWidth = title == "ACCESORIOS" ? accessorySurfaceWidth : selectionSurfaceWidth;
		sectionAccentSpans.push_back({ sectionLine, mMenu.getList()->size(), -1, accentColor, sectionWidth });
		activeSectionAccent = static_cast<int>(sectionAccentSpans.size()) - 1;
		mMenu.addRow(section, false, true);
		ComponentListRow breathingRoom;
		auto spacer = std::make_shared<GuiComponent>(window);
		spacer->setSize(selectionSurfaceWidth, sectionContentPadding);
		breathingRoom.addElement(spacer, false);
		breathingRoom.selectable = false;
		mMenu.addRow(breathingRoom, false, true);
	};
	const auto badgeFont = Font::get(static_cast<unsigned int>(screenHeight * 0.022f));
	const auto scaledCompactFont = Font::get(static_cast<unsigned int>(screenHeight * 0.018f));
	const float regionFlagWidth = screenHeight * 0.076f;
	const float regionFlagHeight = screenHeight * 0.0507f;
	auto addFolderRows = [this, window, screenHeight, themeRoot, scaledCompactFont, badgeFont, regionFlagWidth, regionFlagHeight, layoutScale, horizontalLayoutScale, selectionSurfaceWidth, selectionContentWidth, selectorLeftInset, selectorHandToCardGap, addFocusIcon, addOptionRowGap](const std::vector<FolderData*>& folders, int rowKind)
	{
		size_t folderIndex = 0;
		for (auto folder : folders)
		{
			const std::string id = folder->getPath();
			ComponentListRow row;
			row.selectionWidth = selectionSurfaceWidth;
			auto leftInset = std::make_shared<GuiComponent>(window);
			leftInset->setSize(selectorLeftInset, 0);
			row.addElement(leftInset, false);
			if (rowKind == 0 || rowKind == 1)
			{
				row.addElement(addFocusIcon(id, false), false);
				auto cardGap = std::make_shared<GuiComponent>(window);
				cardGap->setSize(selectorHandToCardGap, 1.0f);
				row.addElement(cardGap, false);
				const int detailRows = 1;
				auto details = std::make_shared<ComponentGrid>(window, Vector2i(2, detailRows));
				const float detailsWidth = rowKind == 1
					? std::max(1.0f, selectionSurfaceWidth - selectorLeftInset -
						screenHeight * 0.055f * std::max(0.72f, horizontalLayoutScale) - selectorHandToCardGap)
					: selectionContentWidth;
				const float rowHeight = screenHeight * (rowKind == 0 ? 0.190f : 0.150f) * layoutScale;
				details->setSize(detailsWidth, rowHeight);
				const std::string region = getSnesFolderKey(folder);
				const bool isUsa = region.find("usa") != std::string::npos || region.find("america") != std::string::npos;
				const bool isJapan = region.find("jap") != std::string::npos;
				const bool emphasizeRegionalArtwork = rowKind == 0 && (isJapan ||
					region.find("europe") != std::string::npos || region.find("europa") != std::string::npos);
				std::string hardwarePath = isUsa
					? getSnesThemeAsset(themeRoot, { "_inc/retrobat-snes/media/hardware/snes-usa.png", "_inc/retrobat-snes/media/hardware/us-snes.png", "_systemmedia/hardware/snesusa.png", "_inc/arts/snes US.png" }, "art/consoles/us/snes.png")
					: isJapan
						? getSnesThemeAsset(themeRoot, { "_inc/retrobat-snes/media/hardware/snes-japan.png", "_inc/retrobat-snes/media/hardware/jp-snes.png", "_systemmedia/hardware/snes-jp.png" }, "art/consoles/jp/snes.png")
						: getSnesThemeAsset(themeRoot, { "_inc/retrobat-snes/media/hardware/snes-europe.png", "_inc/retrobat-snes/media/hardware/eu-snes.png", "_systemmedia/hardware/snes.png", "_inc/arts/snes.png" }, "art/consoles/snes.png");
				const std::string mediaKey = getSnesMediaKey(folder);
				const std::string keyedHardwarePath = getSnesKeyedThemeAsset(themeRoot, "consoles", mediaKey, { "png" });
				if (!keyedHardwarePath.empty())
					hardwarePath = keyedHardwarePath;
				std::string logoPath = rowKind == 0
					? isUsa
						? getSnesThemeAsset(themeRoot, { "_inc/logos/us/snes.svg", "_inc/logos/snes-us.svg", "_inc/logos/snes US.svg" }, "art/logos/us/snes.png", themeRoot + "/_inc/logos/snes.svg")
						: isJapan
							? getSnesThemeAsset(themeRoot, { "_inc/logos/jp/snes.svg", "_inc/logos/snes-jp.svg", "_inc/logos/snes JP.svg" }, "art/logos/jp/snes.png", themeRoot + "/_inc/logos/snes.svg")
							: getSnesThemeAsset(themeRoot, { "_inc/logos/snes-eu.svg", "_inc/logos/snes EU.svg" }, "art/logos/snes.png", themeRoot + "/_inc/logos/snes.svg")
					: getSnesThemeAsset(themeRoot, { "_inc/logos/snesh.svg", "_inc/logos/snesh.png" }, "art/logos/snesh.png");
				const std::string keyedLogoPath = getSnesKeyedThemeAsset(themeRoot, "logos", mediaKey, { "png", "svg" });
				if (!keyedLogoPath.empty())
					logoPath = keyedLogoPath;
					const float cardWidth = rowKind == 0 ? std::min(detailsWidth * 0.82f, rowHeight * 3.30f)
						: rowKind == 1 ? detailsWidth * 0.94f
						: std::min(detailsWidth * 0.48f, rowHeight * 2.20f);
				details->setColWidthPerc(0, cardWidth / detailsWidth, false);
				details->setColWidthPerc(1, 1.0f - cardWidth / detailsWidth, false);
				if (rowKind == 0)
				{
					// Horizontal console card: hardware at left, regional logo and count
					// stacked at right, with the flag over the hardware as the region cue.
					auto card = std::make_shared<ComponentGrid>(window, Vector2i(2, 2));
					card->setSize(cardWidth, rowHeight);
					const float hardwareColumnWidth = emphasizeRegionalArtwork ? 0.68f : 0.64f;
					card->setColWidthPerc(0, hardwareColumnWidth, false);
					card->setColWidthPerc(1, 1.0f - hardwareColumnWidth, false);
					card->setRowHeightPerc(0, 0.68f, false);
					card->setRowHeightPerc(1, 0.32f, false);
					auto hardware = std::make_shared<ImageComponent>(window, true, false);
					if (!hardwarePath.empty() && Utils::FileSystem::exists(hardwarePath))
						hardware->setImage(hardwarePath);
					hardware->setOrigin(0.5f, 0.5f);
					hardware->setAllowFading(false);
					hardware->setMaxSize(cardWidth * hardwareColumnWidth,
						rowHeight * (emphasizeRegionalArtwork ? 1.25f : 1.12f));
					card->setEntry(hardware, Vector2i(0, 0), false, false, Vector2i(1, 2));

					const std::string flagPath = getSnesOfficialFlagPath(folder, themeRoot);
					auto flag = std::make_shared<ImageComponent>(window, true);
					flag->setImage(flagPath);
					flag->setOrigin(0.0f, 0.0f);
					const float flagWidth = std::min(regionFlagWidth, cardWidth * 0.28f);
					const float flagHeight = std::min(regionFlagHeight, rowHeight * 0.46f);
					auto flagShadow = std::make_shared<ImageComponent>(window, true, false);
					flagShadow->setImage(flagPath);
					flagShadow->setColorShift(0x0000009A);
					flagShadow->setMaxSize(flagWidth, flagHeight);
					flagShadow->setOrigin(0.035f, 0.055f);
					flagShadow->setZIndex(1.0f);
					card->setEntry(flagShadow, Vector2i(0, 0), false, false);
					flag->setMaxSize(flagWidth, flagHeight);
					flag->setPosition(cardWidth * 0.025f, rowHeight * 0.025f);
					flag->setZIndex(2.0f);
					card->setEntry(flag, Vector2i(0, 0), false, false);

					auto logo = std::make_shared<ImageComponent>(window, true, false);
					if (!logoPath.empty() && Utils::FileSystem::exists(logoPath)) logo->setImage(logoPath);
					logo->setOrigin(0.0f, 0.5f);
					logo->setAllowFading(false);
					logo->setMaxSize(cardWidth * (emphasizeRegionalArtwork ? 0.54f : 0.48f),
						rowHeight * (emphasizeRegionalArtwork ? 1.20f : 1.08f));
					card->setEntry(logo, Vector2i(1, 0), false, false);

					auto count = std::make_shared<TextComponent>(window, getGameCountLabel(folder),
						Font::get(static_cast<unsigned int>(screenHeight * 0.014f)), 0xF1F8FFFF, ALIGN_CENTER);
					count->setRenderBackground(false);
					count->setMultiLine(TextComponent::MultiLineType::SINGLELINE);
					count->setSize(cardWidth * (emphasizeRegionalArtwork ? 0.32f : 0.42f), rowHeight * 0.32f);
					count->setHorizontalAlignment(ALIGN_LEFT);
					card->setEntry(count, Vector2i(1, 1), false, false);
					details->setEntry(card, Vector2i(0, 0), false, false);
				}
				else
				{
					const bool isHack = rowKind == 1;
					auto card = std::make_shared<ComponentGrid>(window, isHack ? Vector2i(2, 1) : Vector2i(1, 2));
					card->setSize(cardWidth, rowHeight);
					auto logo = std::make_shared<ImageComponent>(window, true, false);
					if (!logoPath.empty() && Utils::FileSystem::exists(logoPath)) logo->setImage(logoPath);
					logo->setOrigin(isHack ? 0.0f : 0.5f, 0.5f);
					logo->setAllowFading(false);
					logo->setMaxSize(cardWidth * (isHack ? 0.62f : 1.0f), rowHeight * (isHack ? 1.08f : 0.94f));
					auto count = std::make_shared<TextComponent>(window, getGameCountLabel(folder),
						Font::get(static_cast<unsigned int>(screenHeight * (isHack ? 0.014f : 0.014f))), 0xF1F8FFFF, ALIGN_LEFT);
					count->setRenderBackground(false);
					count->setMultiLine(TextComponent::MultiLineType::SINGLELINE);
					if (isHack)
					{
						const float countGap = screenHeight * 0.024f;
						const float logoColumnWidth = std::min(cardWidth * 0.70f, logo->getSize().x() + countGap);
						const float countColumnWidth = std::max(1.0f, cardWidth - logoColumnWidth);
						card->setColWidth(0, logoColumnWidth, false);
						card->setColWidth(1, countColumnWidth, false);
						count->setSize(countColumnWidth, rowHeight);
						count->setHorizontalAlignment(ALIGN_LEFT);
						card->setEntry(logo, Vector2i(0, 0), false, false);
						card->setEntry(count, Vector2i(1, 0), false, false);
					}
					else
				{
					card->setRowHeightPerc(0, 0.78f, false);
					card->setRowHeightPerc(1, 0.22f, false);
					count->setSize(cardWidth, rowHeight * 0.22f);
					count->setHorizontalAlignment(ALIGN_LEFT);
					card->setEntry(logo, Vector2i(0, 0), false, false);
					card->setEntry(count, Vector2i(0, 1), false, false);
				}
					details->setEntry(card, Vector2i(0, 0), false, false);
				}
				if (rowKind == 0)
				{
					auto badge = std::make_shared<TextComponent>(window, "★ PREDETERMINADA", badgeFont, 0x9BE8EFFF, ALIGN_LEFT);
					badge->setSize(detailsWidth - cardWidth, rowHeight);
					badge->setHorizontalAlignment(ALIGN_LEFT);
					badge->setPadding(Vector4f(screenHeight * 0.040f, 0.0f, 0.0f, 0.0f));
					badge->setGlowColor(0x183F47FF);
					badge->setGlowSize(2);
					badge->setRenderBackground(false);
					badge->setVisible(false);
					badge->setOpacity(0);
					mVariantBadges.emplace_back(folder, badge);
					// Keep the default marker beside the card, never beneath the flag.
					details->setEntry(badge, Vector2i(1, 0), false, false);
				}
				row.addElement(details, false);
			}
			else
			{
				const bool isAccessory = rowKind == 2 && isSnesAccessory(folder);
				const std::string displayName = getSnesFolderDisplayName(folder);
				const std::string accessoryKey = Utils::String::toLower(displayName + " " + getSnesFolderKey(folder));
				std::string description = folder->getMetadata().get("desc");
				if (isAccessory || description.empty())
					description = getGameCountLabel(folder);

				const float detailsWidth = selectionContentWidth;
				const float rowHeight = screenHeight * (isAccessory ? 0.190f : 0.085f) * layoutScale;
				row.addElement(addFocusIcon(id, false), false);
				auto cardGap = std::make_shared<GuiComponent>(window);
				cardGap->setSize(selectorHandToCardGap, 1.0f);
				row.addElement(cardGap, false);
				if (isAccessory)
				{
					std::string artRelativePath = "art/consoles/sgb.png";
					std::string logoRelativePath = "art/logos/sgb.svg";
					const bool isSgb = folder->getSystem() != nullptr && folder->getSystem()->getName() == "sgb";
					if (accessoryKey.find("satell") != std::string::npos || accessoryKey.find("bs-x") != std::string::npos || accessoryKey.find("bsx") != std::string::npos)
					{
						artRelativePath = "art/consoles/satellaview.png";
						logoRelativePath = "art/logos/satellaview.svg";
					}
					else if (accessoryKey.find("sufami") != std::string::npos)
					{
						artRelativePath = "art/consoles/sufami.png";
						logoRelativePath = "art/logos/sufami.svg";
					}
					const bool isSufami = accessoryKey.find("sufami") != std::string::npos;
					std::string artPath = isSgb
						? getSnesThemeAsset(themeRoot, { "_systemmedia/hardware/nintendosupergameboy.png" }, artRelativePath)
						: isSufami
							? getSnesThemeAsset(themeRoot, { "_systemmedia/hardware/sufami.png", "_inc/retrobat-snes/media/hardware/sufami.png" }, artRelativePath)
							: getSnesThemeAsset(themeRoot, { "_systemmedia/hardware/satellaview.png", "_inc/retrobat-snes/media/hardware/satellaview.png" }, artRelativePath);
					std::string logoPath = isSgb
						? getSnesThemeAsset(themeRoot, { "_inc/logos/sgb.svg" }, logoRelativePath)
						: isSufami
							? getSnesThemeAsset(themeRoot, { "_inc/logos/sufami.svg" }, logoRelativePath)
							: getSnesThemeAsset(themeRoot, { "_inc/logos/satellaview.svg" }, logoRelativePath);
					const std::string accessoryMediaKey = getSnesMediaKey(folder);
					const std::string keyedAccessoryArt = getSnesKeyedThemeAsset(themeRoot, "consoles", accessoryMediaKey, { "png" });
					const std::string keyedAccessoryLogo = getSnesKeyedThemeAsset(themeRoot, "logos", accessoryMediaKey, { "png", "svg" });
					if (!keyedAccessoryArt.empty()) artPath = keyedAccessoryArt;
					if (!keyedAccessoryLogo.empty()) logoPath = keyedAccessoryLogo;

					// Keep accessories in the same horizontal card grammar as official
					// consoles: hardware left, logo right, game count directly below it.
					const float cardWidth = std::min(detailsWidth * 0.82f, rowHeight * 3.30f);
					auto details = std::make_shared<ComponentGrid>(window, Vector2i(2, 2));
					details->setSize(cardWidth, rowHeight);
					details->setColWidthPerc(0, 0.60f, false);
					details->setColWidthPerc(1, 0.40f, false);
					details->setRowHeightPerc(0, 0.68f, false);
					details->setRowHeightPerc(1, 0.32f, false);
					auto hardware = std::make_shared<ImageComponent>(window, true, false);
					if (!artPath.empty() && Utils::FileSystem::exists(artPath))
						hardware->setImage(artPath);
					hardware->setOrigin(0.5f, 0.5f);
					hardware->setAllowFading(false);
					hardware->setMaxSize(cardWidth * 0.58f, rowHeight * 0.96f);
					details->setEntry(hardware, Vector2i(0, 0), false, false, Vector2i(1, 2));

					auto logo = std::make_shared<ImageComponent>(window, true, false);
					if (!logoPath.empty() && Utils::FileSystem::exists(logoPath))
						logo->setImage(logoPath);
					logo->setOrigin(0.0f, 0.5f);
					logo->setAllowFading(false);
					logo->setMaxSize(cardWidth * 0.39f, rowHeight * 0.66f);
					const auto accessoryCountFont = Font::get(static_cast<unsigned int>(screenHeight * 0.014f));
					auto count = std::make_shared<TextComponent>(window, getGameCountLabel(folder),
						accessoryCountFont, 0xD7E4EAFF, ALIGN_CENTER);
					count->setRenderBackground(false);
					count->setMultiLine(TextComponent::MultiLineType::SINGLELINE);
					count->setSize(cardWidth * 0.40f, rowHeight * 0.32f);
					count->setHorizontalAlignment(ALIGN_LEFT);
					details->setEntry(logo, Vector2i(1, 0), false, false);
					details->setEntry(count, Vector2i(1, 1), false, false);
					row.addElement(details, false);
				}
				else
				{
					auto details = std::make_shared<ComponentGrid>(window, Vector2i(1, 2));
					details->setSize(detailsWidth, rowHeight);
					details->setRowHeightPerc(0, 0.56f, false);
					details->setRowHeightPerc(1, 0.44f, false);
					auto title = std::make_shared<TextComponent>(window,
						Utils::String::toUpper(displayName),
						Font::get(static_cast<unsigned int>(screenHeight * 0.018f * layoutScale)), 0xF1E4C8FF, ALIGN_LEFT);
					title->setRenderBackground(false);
					title->setMultiLine(TextComponent::MultiLineType::SINGLELINE);
					auto subtitle = std::make_shared<TextComponent>(window, description,
						Font::get(static_cast<unsigned int>(screenHeight * 0.015f * layoutScale)), 0xD7E4EAFF, ALIGN_LEFT);
					subtitle->setRenderBackground(false);
					subtitle->setMultiLine(TextComponent::MultiLineType::SINGLELINE);
					details->setEntry(title, Vector2i(0, 0), false, true);
					details->setEntry(subtitle, Vector2i(0, 1), false, true);
					row.addElement(details, false);
				}
			}

			row.makeAcceptInputHandler([this, folder]
			{
				SystemData* owner = folder->getSystem();
				if (owner != nullptr && folder == owner->getRootFolder())
					ViewController::get()->goToGameList(owner, true);
				else
					openVariant(owner != nullptr ? owner : mSystem, folder);
				delete this;
			});
			const int rowIndex = static_cast<int>(mMenu.getList()->size());
			// On entry, land on the first real official variant instead of an
			// empty band/spacer or an uninitialized location in the list.
			const bool focusInitialOfficialVariant = rowKind == 0 && folderIndex == 0;
			mMenu.addRow(row, focusInitialOfficialVariant, true, id);
			mSelectionRows.emplace_back(id, rowIndex);
			if (++folderIndex < folders.size())
				addOptionRowGap();
		}
	};
	if (!officialFolders.empty())
	{
		addSectionHeader("VARIANTES OFICIALES", 0x75A7FFFF);
		addFolderRows(officialFolders, 0);
	}
	if (!hackFolders.empty())
	{
		if (!officialFolders.empty()) addSectionGap(0xE6B85CFF);
		addSectionHeader("HACKS", 0xE6B85CFF);
		addFolderRows(hackFolders, 1);
	}
	if (!mTranslationFolders.empty())
	{
		if (!officialFolders.empty() || !hackFolders.empty()) addSectionGap(0x6AC7D4FF);
		addSectionHeader("TRADUCCIONES", 0x6AC7D4FF);
	const float laneHeight = screenHeight * 0.180f * layoutScale;
		const float laneHeightWidth = screenHeight * 0.180f;
		const float laneMaxWidth = std::max(1.0f, selectionContentWidth - selectorLeftInset);
		const float laneWidth = std::min(laneMaxWidth, laneHeightWidth * mTranslationFolders.size());
		const float cellWidth = laneWidth / mTranslationFolders.size();
		auto lane = std::make_shared<ComponentGrid>(window, Vector2i(static_cast<int>(mTranslationFolders.size()), 1));
		lane->setSize(laneWidth, laneHeight);
		lane->setRowHeight(0, laneHeight, false);
		auto laneInset = std::make_shared<GuiComponent>(window);
		laneInset->setSize(selectorLeftInset, 0);
		ComponentListRow row;
		row.addElement(laneInset, false);
		for (size_t index = 0; index < mTranslationFolders.size(); ++index)
		{
			auto folder = mTranslationFolders[index];
			auto cell = std::make_shared<ComponentGrid>(window, Vector2i(2, 2));
			cell->setSize(cellWidth, laneHeight);
			const float renderedHandWidth = screenHeight * 0.055f * std::max(0.72f, horizontalLayoutScale);
			const float handColumnWidth = std::min(renderedHandWidth, cellWidth * 0.34f);
			cell->setColWidth(0, handColumnWidth, false);
			cell->setColWidth(1, cellWidth - handColumnWidth, false);
			cell->setRowHeightPerc(0, 0.68f, false);
			cell->setRowHeightPerc(1, 0.32f, false);
			const std::string focusId = TRANSLATION_ROW_ID + ":" + std::to_string(index);
			auto translationHand = addFocusIcon(focusId, true);
			cell->setEntry(translationHand, Vector2i(0, 0), false, false);
			// Keep a visible gap between the hand pointer and the selected flag;
			// only the pointer moves, leaving the flag and count cells in place.
			translationHand->setPosition(translationHand->getPosition().x() + screenHeight * 0.020f,
				translationHand->getPosition().y());
			auto flag = std::make_shared<ImageComponent>(window, true);
			flag->setImage(getSnesTranslationFlagPath(folder, themeRoot));
			flag->setOrigin(0.5f, 0.5f);
			const float laneFlagHeight = std::min(regionFlagHeight, laneHeight * 0.70f);
			const float laneFlagWidth = laneFlagHeight * 1.5f;
			const std::string flagPath = getSnesTranslationFlagPath(folder, themeRoot);
			auto flagShadow = std::make_shared<ImageComponent>(window, true, false);
			flagShadow->setImage(flagPath);
			flagShadow->setColorShift(0x0000009A);
			flagShadow->setMaxSize(laneFlagWidth, laneFlagHeight);
			flagShadow->setOrigin(0.52f, 0.56f);
			flagShadow->setZIndex(1.0f);
			cell->setEntry(flagShadow, Vector2i(1, 0), false, false);
			flag->setMaxSize(laneFlagWidth, laneFlagHeight);
			flag->setZIndex(2.0f);
			auto count = std::make_shared<TextComponent>(window, getGameCountLabel(folder), scaledCompactFont, 0xD7E4EAFF, ALIGN_CENTER);
			count->setRenderBackground(false);
			mTranslationCounts.push_back(count);
			cell->setEntry(flag, Vector2i(1, 0), false, false);
			count->setSize(cellWidth - handColumnWidth, laneHeight * 0.32f);
			count->setHorizontalAlignment(ALIGN_CENTER);
			cell->setEntry(count, Vector2i(1, 1), false, false);
			lane->setColWidth(static_cast<int>(index), cellWidth, false);
			lane->setEntry(cell, Vector2i(static_cast<int>(index), 0), false, false);
		}
		row.addElement(lane, false);
		row.input_handler = [this](InputConfig* config, Input input)
		{
			if (input.value == 0)
				return false;
			if (config->isMappedLike("left", input) || config->isMappedLike("right", input))
			{
				const int direction = config->isMappedLike("left", input) ? -1 : 1;
				const int count = static_cast<int>(mTranslationFolders.size());
				if (count > 1)
					mTranslationCursor = (mTranslationCursor + direction + count) % count;
				updateFocusIndicator();
				updatePreview();
				return true;
			}
			if (config->isMappedTo(BUTTON_OK, input) && mTranslationCursor >= 0 && mTranslationCursor < static_cast<int>(mTranslationFolders.size()))
			{
				openVariant(mSystem, mTranslationFolders[mTranslationCursor]);
				delete this;
				return true;
			}
			return false;
		};
		const int translationRowIndex = static_cast<int>(mMenu.getList()->size());
		row.selectionWidth = selectionSurfaceWidth;
		mMenu.addRow(row, false, true, TRANSLATION_ROW_ID);
		mSelectionRows.emplace_back(TRANSLATION_ROW_ID, translationRowIndex);
		updateFocusIndicator();
	}
	if (!accessoryFolders.empty())
	{
		if (!officialFolders.empty() || !hackFolders.empty() || !mTranslationFolders.empty()) addSectionGap(0xD9965BFF);
		addSectionHeader("ACCESORIOS", 0xD9965BFF);
		const float laneHeight = screenHeight * 0.220f * layoutScale;
		const float laneMaxWidth = std::max(1.0f, accessorySurfaceWidth - selectorLeftInset * 2.0f);
		const float laneWidth = std::min(laneMaxWidth, screenHeight * 0.300f * accessoryFolders.size());
		const float cellWidth = laneWidth / accessoryFolders.size();
		auto lane = std::make_shared<ComponentGrid>(window, Vector2i(static_cast<int>(accessoryFolders.size()), 1));
		lane->setSize(laneWidth, laneHeight);
		lane->setRowHeight(0, laneHeight, false);
		ComponentListRow row;
		auto laneInset = std::make_shared<GuiComponent>(window);
		laneInset->setSize(selectorLeftInset, 0);
		row.addElement(laneInset, false);
		for (size_t index = 0; index < accessoryFolders.size(); ++index)
		{
			auto folder = accessoryFolders[index];
			const std::string displayName = getSnesFolderDisplayName(folder);
			const std::string accessoryKey = Utils::String::toLower(displayName + " " + getSnesFolderKey(folder));
			const std::string accessorySystem = folder->getSystem() != nullptr ? folder->getSystem()->getName() : std::string();
			const bool isSgb = accessorySystem == "sgb";
			const bool isSgbMsu = accessorySystem == "sgb-msu1";
			const bool isSnesMsu = accessorySystem == "snes-msu1";
			const bool isSatellaview = accessorySystem == "satellaview";
			const bool isSufami = accessoryKey.find("sufami") != std::string::npos;
			std::string artRelativePath = "art/consoles/satellaview.png";
			std::string logoRelativePath = "art/logos/satellaview.svg";
			std::string hardwareAsset = "_systemmedia/hardware/satellaview.png";
			std::string logoAsset = "_inc/logos/satellaview-badge.svg";
			if (isSgb)
			{
				artRelativePath = "art/consoles/sgb.png";
				logoRelativePath = "art/logos/sgb.svg";
				hardwareAsset = "_systemmedia/hardware/nintendosupergameboy.png";
				logoAsset = "_inc/logos/sgb-badge.svg";
			}
			else if (isSgbMsu)
			{
				// MSU-1 modifies the cartridge/audio path; it has no distinct
				// physical console, so keep the authentic Super Game Boy hardware.
				artRelativePath = "art/consoles/sgb.png";
				logoRelativePath = "art/logos/sgb-msu1.png";
				hardwareAsset = "_systemmedia/hardware/nintendosupergameboy.png";
				logoAsset = "_inc/logos/sgb-msu1-badge.svg";
			}
			else if (isSufami)
			{
				artRelativePath = "art/consoles/sufami.png";
				logoRelativePath = "art/logos/sufami.svg";
				hardwareAsset = "_systemmedia/hardware/sufami.png";
				logoAsset = "_inc/logos/sufami-badge.svg";
			}
			else if (isSnesMsu)
			{
				// MSU-1 is a cartridge expansion, not a separate console model.
				artRelativePath = "art/consoles/us/snes.png";
				logoRelativePath = "art/logos/snes-msu1.png";
				hardwareAsset = "_systemmedia/hardware/snesusa.png";
				logoAsset = "_inc/logos/snes-msu1-badge.svg";
			}
			else if (isSatellaview)
				logoRelativePath = "art/logos/satellaview-w.svg";
			std::string hardwarePath = getSnesThemeAsset(themeRoot, { hardwareAsset }, artRelativePath);
			std::string logoPath = getSnesThemeAsset(themeRoot, { logoAsset }, logoRelativePath);
			const std::string accessoryMediaKey = getSnesMediaKey(folder);
			if (isSgb || isSgbMsu || isSnesMsu)
			{
				const std::string keyedLogoPath = getSnesKeyedThemeAsset(themeRoot, "logos", accessoryMediaKey, { "png", "svg" });
				if (!keyedLogoPath.empty())
					logoPath = keyedLogoPath;
			}
			if (isSgb || isSgbMsu || isSnesMsu)
			{
				// Use each library's explicitly assigned artwork, including its MSU
				// hardware illustration, instead of falling back to base SNES hardware.
				const std::string keyedHardwarePath = getSnesKeyedThemeAsset(themeRoot, "consoles", accessoryMediaKey, { "png" });
				if (!keyedHardwarePath.empty())
					hardwarePath = keyedHardwarePath;
			}
			const float handColumnWidth = std::min(screenHeight * 0.066f * std::max(0.72f, horizontalLayoutScale), cellWidth * 0.26f);
			const float cardWidth = std::max(1.0f, cellWidth - handColumnWidth);
			auto cell = std::make_shared<ComponentGrid>(window, Vector2i(2, 1));
			cell->setSize(cellWidth, laneHeight);
			cell->setColWidth(0, handColumnWidth, false);
			cell->setColWidth(1, cardWidth, false);
			const std::string focusId = ACCESSORY_ROW_ID + ":" + std::to_string(index);
			auto accessoryFocusIcon = addFocusIcon(focusId, true);
            accessoryFocusIcon->setMaxSize(screenHeight * 0.062f * std::max(0.72f, horizontalLayoutScale),
                screenHeight * 0.062f * std::max(0.72f, horizontalLayoutScale));
			accessoryFocusIcon->setOrigin(1.0f, 0.5f);
			cell->setEntry(accessoryFocusIcon, Vector2i(0, 0), false, false, Vector2i(1, 1));

			// Each accessory keeps the established horizontal card: hardware on
			// the left, a slightly enlarged logo and its count grouped on the right.
			auto card = std::make_shared<ComponentGrid>(window, Vector2i(2, 2));
			card->setSize(cardWidth, laneHeight);
			// Match the official console card's hardware/logo proportions.
			const bool enlargeSgbHardware = isSgb || isSgbMsu;
			const float hardwareColumn = enlargeSgbHardware ? 0.74f : isSnesMsu ? 0.70f : 0.64f;
			const float identityColumn = 1.0f - hardwareColumn;
			card->setColWidthPerc(0, hardwareColumn, false);
			card->setColWidthPerc(1, identityColumn, false);
			card->setRowHeightPerc(0, 0.68f, false);
			card->setRowHeightPerc(1, 0.32f, false);
			auto hardware = std::make_shared<ImageComponent>(window, true, false);
			if (!hardwarePath.empty() && Utils::FileSystem::exists(hardwarePath))
				hardware->setImage(hardwarePath);
			hardware->setOrigin(0.5f, 0.5f);
			hardware->setAllowFading(false);
			const float hardwareMaxWidth = isSgb ? 0.84f : isSnesMsu ? 0.74f : enlargeSgbHardware ? 0.72f : 0.60f;
			const float hardwareMaxHeight = isSgb ? 1.55f : isSnesMsu ? 1.50f : enlargeSgbHardware ? 1.42f : 1.26f;
			hardware->setMaxSize(cardWidth * hardwareMaxWidth, laneHeight * hardwareMaxHeight);
			card->setEntry(hardware, Vector2i(0, 0), false, false, Vector2i(1, 2));
			auto logo = std::make_shared<ImageComponent>(window, true, false);
			if (!logoPath.empty() && Utils::FileSystem::exists(logoPath))
				logo->setImage(logoPath);
			logo->setOrigin(0.0f, 0.5f);
			logo->setAllowFading(false);
			const float logoWidth = enlargeSgbHardware ? identityColumn * 0.94f : isSnesMsu ? identityColumn : isSatellaview ? 0.35f : 0.32f;
			logo->setMaxSize(cardWidth * logoWidth, laneHeight * 1.04f);
			card->setEntry(logo, Vector2i(1, 0), false, false);
			auto count = std::make_shared<TextComponent>(window, getGameCountLabel(folder),
				Font::get(static_cast<unsigned int>(screenHeight * (enlargeSgbHardware ? 0.0125f : 0.014f))),
				0xD7E4EAFF, ALIGN_LEFT);
			count->setRenderBackground(false);
			count->setMultiLine(TextComponent::MultiLineType::SINGLELINE);
			count->setSize(cardWidth * (enlargeSgbHardware || isSnesMsu ? identityColumn : 0.32f), laneHeight * 0.32f);
			count->setHorizontalAlignment(ALIGN_LEFT);
			mAccessoryCounts.push_back(count);
			card->setEntry(count, Vector2i(1, 1), false, false);
			cell->setEntry(card, Vector2i(1, 0), false, false);
			lane->setColWidth(static_cast<int>(index), cellWidth, false);
			lane->setEntry(cell, Vector2i(static_cast<int>(index), 0), false, false);
		}
		row.input_handler = [this](InputConfig* config, Input input)
		{
			if (input.value == 0)
				return false;
			if (config->isMappedLike("left", input) || config->isMappedLike("right", input))
			{
				const int direction = config->isMappedLike("left", input) ? -1 : 1;
				const int count = static_cast<int>(mAccessoryFolders.size());
				if (count > 1)
					mAccessoryCursor = (mAccessoryCursor + direction + count) % count;
				updateFocusIndicator();
				updatePreview();
				return true;
			}
			if (config->isMappedTo(BUTTON_OK, input) && mAccessoryCursor >= 0 &&
				mAccessoryCursor < static_cast<int>(mAccessoryFolders.size()))
			{
				auto folder = mAccessoryFolders[mAccessoryCursor];
				// Accessory libraries are independent systems, not SNES subfolders.
				// Open their real GameListView so the normal scraper and gamelist
				// persistence use the accessory's platform and root path.
				openVariant(folder != nullptr ? folder->getSystem() : nullptr, folder);
				delete this;
				return true;
			}
			return false;
		};
		const int accessoryRowIndex = static_cast<int>(mMenu.getList()->size());
		row.selectionWidth = accessorySurfaceWidth;
		row.addElement(lane, false);
		mMenu.addRow(row, false, true, ACCESSORY_ROW_ID);
		mSelectionRows.emplace_back(ACCESSORY_ROW_ID, accessoryRowIndex);
		updateFocusIndicator();
	}
	if (!otherFolders.empty())
	{
		if (!officialFolders.empty() || !hackFolders.empty() || !mTranslationFolders.empty() || !accessoryFolders.empty()) addSectionGap(0xAFA2CFFF);
		addSectionHeader("OTRAS BIBLIOTECAS", 0xAFA2CFFF);
		addFolderRows(otherFolders, 2);
	}
	if (activeSectionAccent >= 0)
	{
		ComponentListRow breathingRoom;
		auto spacer = std::make_shared<GuiComponent>(window);
		spacer->setSize(selectionSurfaceWidth, sectionContentPadding);
		finalSectionBreathingRoom = spacer;
		breathingRoom.addElement(spacer, false);
		breathingRoom.selectable = false;
		mMenu.addRow(breathingRoom, false, true);
		sectionAccentSpans[activeSectionAccent].lastRow = mMenu.getList()->size() - 1;
	}
	// Keep the final panel's external padding, but consume its inner bottom
	// spacer if rounding/minimum row heights leave the native list a few pixels
	// taller than its real viewport. This prevents an otherwise pointless scroll.
	if (finalSectionBreathingRoom != nullptr)
	{
		const float safeListHeight = std::max(1.0f, listViewportHeight - screenHeight * 0.002f);
		const float overflow = mMenu.getList()->getTotalRowHeight() - safeListHeight;
		if (overflow > 0.0f)
		{
			const float remainingPadding = std::max(0.0f, finalSectionBreathingRoom->getSize().y() - overflow);
			finalSectionBreathingRoom->setSize(selectionSurfaceWidth, remainingPadding);
		}
	}

	// Give the native list its complete viewport. MenuComponent::updateSize()
	// truncates its own height to the last whole row when content exceeds the
	// limit; using that partial height here would permanently clip later rows.
	mMenu.setMaxHeight(menuMaxHeight);
	mMenu.setMaxWidth(accessorySurfaceWidth);
	mMenu.setSize(accessorySurfaceWidth, menuMaxHeight);
	updatePreview();

	mMenu.setPosition(mSize.x() * 0.020f, mSize.y() * 0.025f);
	const Vector3f listPosition = mMenu.getList()->getPosition();
	const float sectionAccentX = mMenu.getPosition().x() + listPosition.x();
	const float sectionRowsTop = mMenu.getPosition().y() + listPosition.y();
	for (size_t spanIndex = 0; spanIndex < sectionAccentSpans.size(); ++spanIndex)
	{
		const auto& span = sectionAccentSpans[spanIndex];
		if (span.lastRow < span.firstRow)
			continue;
		float yOffset = 0.0f;
		float spanHeight = 0.0f;
		float headingHeight = 0.0f;
		for (int row = 0; row <= span.lastRow; ++row)
		{
			const float rowHeight = mMenu.getList()->getRowHeight(row);
			if (row < span.firstRow)
				yOffset += rowHeight;
			else
				spanHeight += rowHeight;
			if (row == span.firstRow)
				headingHeight = rowHeight;
		}
		const float panelHorizontalInset = screenHeight * 0.012f;
		const float panelVerticalInset = screenHeight * 0.009f;
		// Let the section frame wrap the full-width selected-row band with a
		// deliberate inner margin instead of letting that band protrude.
		const float panelLeft = sectionAccentX - panelHorizontalInset;
		const float panelTop = sectionRowsTop + yOffset - panelVerticalInset;
		const float panelWidth = span.width + panelHorizontalInset * 2.0f;
		const float panelHeight = std::max(1.0f, spanHeight + panelVerticalInset * 2.0f);
		auto addPanelLayer = [this, window](unsigned int color, float zIndex,
			float x, float y, float width, float height)
		{
			auto panel = std::make_shared<ImageComponent>(window, true, false);
			panel->setImage(std::string(":/white.png"), true);
			panel->setColorShift(color);
			panel->setResize(width, height);
			panel->setPosition(x, y);
			panel->setZIndex(zIndex);
			mSectionPanels.push_back(panel);
			addChild(panel.get());
		};
		const float borderWidth = std::max(2.0f, screenHeight * 0.0025f);
		// Translucent section backing reveals a little of the fanart without
		// fading the independently rendered labels, icons, counters, or frames.
		const unsigned int darkFill = 0x080D1778;
		const unsigned int outerGlowColor = (span.color & 0xFFFFFF00) | 0x24;
		const unsigned int innerGlowColor = (span.color & 0xFFFFFF00) | 0x58;
		const unsigned int borderColor = (span.color & 0xFFFFFF00) | 0xF0;
		addPanelLayer(darkFill, 7.0f, panelLeft, panelTop, panelWidth, panelHeight);
		// Build a restrained neon edge from two low-opacity halos and a crisp
		// color-matched core. The frame geometry and its section hue stay fixed.
		auto addFrame = [&addPanelLayer](unsigned int color, float zIndex,
			float x, float y, float width, float height, float stroke)
		{
			addPanelLayer(color, zIndex, x, y, width, stroke);
			addPanelLayer(color, zIndex, x, y + height - stroke, width, stroke);
			addPanelLayer(color, zIndex, x, y, stroke, height);
			addPanelLayer(color, zIndex, x + width - stroke, y, stroke, height);
		};
		const float outerSpread = borderWidth * 1.45f;
		const float outerStroke = borderWidth * 2.10f;
		addFrame(outerGlowColor, 49.0f, panelLeft - outerSpread, panelTop - outerSpread,
			panelWidth + outerSpread * 2.0f, panelHeight + outerSpread * 2.0f, outerStroke);
		const float innerSpread = borderWidth * 0.42f;
		const float innerStroke = borderWidth * 1.35f;
		addFrame(innerGlowColor, 50.0f, panelLeft - innerSpread, panelTop - innerSpread,
			panelWidth + innerSpread * 2.0f, panelHeight + innerSpread * 2.0f, innerStroke);
		addFrame(borderColor, 52.0f, panelLeft, panelTop, panelWidth, panelHeight, borderWidth);
		auto ledRunner = std::make_shared<SectionLedRunner>(window, panelLeft, panelTop,
			panelWidth, panelHeight, borderWidth, span.color,
			static_cast<float>(spanIndex) / static_cast<float>(sectionAccentSpans.size()));
		mSectionLedRunners.push_back(ledRunner);
		addChild(ledRunner.get());
		const float accentHeight = std::min(headingHeight * 0.78f, screenHeight * 0.030f * layoutScale);
		span.line->setResize(std::max(4.0f, screenHeight * 0.006f * layoutScale), std::max(1.0f, accentHeight));
		span.line->setPosition(panelLeft + screenHeight * 0.018f, sectionRowsTop + yOffset + (headingHeight - accentHeight) * 0.5f);
		mSectionAccentLines.push_back(span.line);
		addChild(span.line.get());
	}
	const float showcaseTop = mMenu.getPosition().y() + mMenu.getSize().y() + mSize.y() * 0.012f;
	const float showcaseBottom = mSize.y() * 0.985f;
	const float showcaseHeight = std::max(1.0f, showcaseBottom - showcaseTop);
	const size_t showcaseCount = mShowcaseHardware.size();
	if (showcaseCount > 0)
	{
		const float showcaseBaseWidth = mSize.x() * 0.340f;
		const float showcaseWidth = showcaseBaseWidth * 0.94f;
		const float showcaseLeft = mMenu.getPosition().x() + (showcaseBaseWidth - showcaseWidth) * 0.5f;
		const float cellWidth = showcaseWidth / static_cast<float>(showcaseCount);
		const float centerY = showcaseTop + showcaseHeight * 0.5f;
		const float showcaseShiftRight = mSize.x() * 0.22f;
		for (size_t index = 0; index < showcaseCount; ++index)
		{
			const float centerX = showcaseLeft + cellWidth * (static_cast<float>(index) + 0.5f) + showcaseShiftRight;
			auto& hardware = mShowcaseHardware[index];
			hardware->setPosition(centerX, centerY);
			hardware->setMaxSize(cellWidth * 0.88f, showcaseHeight * 0.96f);
			if (index < mShowcaseScreens.size() && mShowcaseScreens[index] != nullptr)
			{
				auto& screen = mShowcaseScreens[index];
				const float artWidth = hardware->getSize().x();
				screen->setPosition(centerX, centerY - artWidth * 0.118f);
				screen->setResize(artWidth * 0.325f, artWidth * 0.220f);
			}
		}
	}
	updateDefaultBadge(false);
	updateFocusIndicator();
}

void GuiSnesVariantSelector::updateDefaultBadge(bool animate)
{
	const FolderData* defaultFolder = getDefaultVariant(mSystem);
	mAnimateDefaultBadges = animate;

	for (auto& entry : mVariantBadges)
	{
		auto& badge = entry.second;
		const bool isDefault = entry.first == defaultFolder;
		if (isDefault)
			badge->setVisible(true);
		else if (!animate)
		{
			badge->setOpacity(0);
			badge->setVisible(false);
		}
	}

	if (!animate && defaultFolder != nullptr)
	{
		for (auto& entry : mVariantBadges)
			if (entry.first == defaultFolder)
				entry.second->setOpacity(255);
	}
}

void GuiSnesVariantSelector::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	updateFocusIndicator();
	updateFocusPulse(deltaTime);
	if (mAnimateDefaultBadges)
	{
		const FolderData* defaultFolder = getDefaultVariant(mSystem);
		const int opacityStep = std::max(1, deltaTime * 2);
		bool transitionRunning = false;
		for (auto& entry : mVariantBadges)
		{
			auto& badge = entry.second;
			const bool isDefault = entry.first == defaultFolder;
			const int opacity = badge->getOpacity();
			const int nextOpacity = isDefault
				? std::min(255, opacity + opacityStep)
				: std::max(0, opacity - opacityStep);

			badge->setOpacity(static_cast<unsigned char>(nextOpacity));
			if (nextOpacity == 0)
				badge->setVisible(false);
			else
				transitionRunning = transitionRunning || nextOpacity < 255 || !isDefault;
		}

		mAnimateDefaultBadges = transitionRunning;
	}
}

void GuiSnesVariantSelector::updatePreview()
{
	FolderData* folder = getSelectedFolder();
	if (folder == nullptr)
		return;

	const std::string label = getSnesFolderDisplayName(folder);
	const std::string key = Utils::String::toLower(label + " " + getSnesFolderKey(folder));
	const std::string themeRoot = getSnesThemeRoot(mSystem);
	const bool isAccessory = isSnesAccessory(folder);
	FileData* parent = folder->getParent();
	const bool isTranslation = parent != nullptr && Utils::String::toLower(parent->getFileName()).find("translat") != std::string::npos;
	const bool isHack = key.find("hack") != std::string::npos || key.find("homebrew") != std::string::npos || key.find("modific") != std::string::npos;
	FolderData* regionalPresentation = (isHack || isTranslation || isAccessory) ? getDefaultVariant(mSystem) : folder;
	if (regionalPresentation == nullptr)
		regionalPresentation = folder;
	const std::string regionKey = Utils::String::toLower(getSnesFolderKey(regionalPresentation));

	std::string kind = "REGIÓN EUROPA";
	std::string title = "SUPER NINTENDO";
	std::string fanart = getSnesThemeAsset(themeRoot,
		{ "art/background/snes.jpg", "_inc/retrobat-snes/media/fanart/snes.jpg" },
		"system/snes/snes.jpg");
	// The center watermark uses system artwork, not the game image/fanart used
	// by the technical preview. Keep its curated source separate so it can never
	// replace or tint the preview card's media.
	std::string centerFanart = getSnesThemeAsset(themeRoot,
		{ "_systemmedia/_fanartsysteme/fresh/snes.jpg" },
		"system/snes/background-fanart.jpg", fanart);
		std::string figurine = getSnesThemeAsset(themeRoot, {}, "art/figurines/snes.png");
	if (!isAccessory)
	{
		if (regionKey.find("usa") != std::string::npos || regionKey.find("américa") != std::string::npos || regionKey.find("america") != std::string::npos)
			figurine = getSnesThemeAsset(themeRoot, {}, "art/figurines/streetfighter.png", figurine);
		else if (regionKey.find("jap") != std::string::npos)
			figurine = getSnesThemeAsset(themeRoot, {}, "art/figurines/zelda.png", figurine);
		else if (regionKey.find("europ") != std::string::npos)
			figurine = getSnesThemeAsset(themeRoot, {}, "art/figurines/mario.png", figurine);
	}
	std::string logo = getSnesThemeAsset(themeRoot,
		{ "art/logos/snes.svg", "_inc/logos/snes.svg" },
		"art/logos/snes.png");

	if (!isAccessory && (regionKey.find("usa") != std::string::npos || regionKey.find("américa") != std::string::npos || regionKey.find("america") != std::string::npos))
	{
		kind = "REGIÓN USA";
		centerFanart = getSnesThemeAsset(themeRoot,
			{ "_systemmedia/_fanartsysteme/fresh/snessa1.jpg" },
			"system/snes/background-fanart.jpg", centerFanart);
		fanart = getSnesThemeAsset(themeRoot,
			{ "art/background/us/snes.jpg", "_inc/retrobat-snes/media/fanart/us-snes.jpg" },
			"system/snes/snes.jpg", fanart);
		logo = getSnesThemeAsset(themeRoot,
			{ "art/logos/us/snes.svg", "_inc/logos/us/snes.svg" },
			"art/logos/us/snes.png", logo);
	}
	else if (!isAccessory && regionKey.find("jap") != std::string::npos)
	{
		kind = "REGIÓN JAPÓN";
		title = "SUPER FAMICOM";
		centerFanart = getSnesThemeAsset(themeRoot,
			{ "_systemmedia/_fanartsysteme/fresh/snesusa.jpg" },
			"system/snes/background-fanart.jpg", centerFanart);
		fanart = getSnesThemeAsset(themeRoot,
			{ "art/background/jp/snes.jpg", "_inc/retrobat-snes/media/fanart/jp-snes.jpg" },
			"system/snes/snes.jpg", fanart);
		logo = getSnesThemeAsset(themeRoot,
			{ "art/logos/jp/snes.svg", "_inc/logos/jp/snes.svg" },
			"art/logos/jp/snes.png", logo);
	}
	if (isAccessory)
	{
		kind = "ACCESORIO SNES";
		title = label;
		std::string assetKey;
		std::string logoFile;
		if (key.find("satell") != std::string::npos || key.find("bs-x") != std::string::npos || key.find("bsx") != std::string::npos)
		{
			assetKey = "satellaview";
			logoFile = "satellaview-badge.svg";
		}
		else if (key.find("snes-msu") != std::string::npos)
		{
			assetKey = "snes";
			logoFile = "snes-msu1-badge.svg";
		}
		else if (key.find("sgb-msu") != std::string::npos)
		{
			assetKey = "sgb";
			logoFile = "sgb-msu1-badge.svg";
		}
		else if (key.find("sufami") != std::string::npos)
		{
			assetKey = "sufami";
			logoFile = "sufami-badge.svg";
		}
		else
		{
			assetKey = "sgb";
			logoFile = "sgb-badge.svg";
		}
		figurine = getSnesThemeAsset(themeRoot, {}, "art/figurines/" + assetKey + (key.find("msu") != std::string::npos ? "-msu1" : "") + ".png", figurine);
		logo = getSnesThemeAsset(themeRoot,
			{ "_inc/logos/" + logoFile, "_systemmedia/_logosyst/clearlogos/" + logoFile, "art/logos/" + logoFile },
			"art/logos/" + assetKey + ".png", logo);
		fanart = getSnesThemeAsset(themeRoot,
			{ "_systemmedia/_fanartsysteme/fresh/" + assetKey + ".jpg", "_systemmedia/_fanartsysteme/custom/" + assetKey + ".jpg", "art/background/" + assetKey + ".jpg" },
			"system/" + assetKey + "/background.jpg", fanart);
		if (key.find("sgb-msu") != std::string::npos)
		{
			const std::string hypermaxSgbMsu = Utils::FileSystem::getParent(themeRoot) + "/Hypermax-Plus-PixN/_inc/wallpaper1/sgb-msu1.jpg";
			centerFanart = Utils::FileSystem::exists(hypermaxSgbMsu) ? hypermaxSgbMsu :
				getSnesThemeAsset(themeRoot,
					{ "_systemmedia/_fanartsysteme/fresh/snes-msu1.jpg" },
					"system/sgb-msu1/background-fanart.jpg", centerFanart);
		}
		else if (key.find("snes-msu") != std::string::npos)
		{
			centerFanart = getSnesThemeAsset(themeRoot,
				{ "_systemmedia/_fanartsysteme/fresh/snes-msu1.jpg" },
				"system/snes/background-fanart.jpg", fanart);
		}
		else if (assetKey == "sufami")
		{
			centerFanart = getSnesThemeAsset(themeRoot,
				{ "_inc/retrobat-snes/media/by-system/center-fanart/sufami.jpg",
				  "_systemmedia/_fanartsysteme/fresh/sufami.jpg" },
				"system/sufami/background-fanart.jpg", fanart);
		}
		else
		{
			centerFanart = getSnesThemeAsset(themeRoot,
				{ "_systemmedia/_fanartsysteme/fresh/" + assetKey + ".jpg" },
				"system/" + assetKey + "/background-fanart.jpg", fanart);
		}
	}
	else if (isHack || isTranslation)
	{
		kind = isHack ? "HACK" : "TRADUCCIÓN";
		if (isHack)
			logo = getSnesThemeAsset(themeRoot, { "_inc/logos/snesh.svg", "_inc/logos/snesh.png" }, "art/logos/snesh.png", logo);
	}
	else if (key.find("msu") != std::string::npos)
	{
		kind = "EXPANSIÓN MSU-1";
		title = "SNES MSU-1";
		logo = themeRoot + "/_inc/logos/snes-msu.svg";
		figurine = getSnesThemeAsset(themeRoot, {}, "art/figurines/snes-msu1.png", figurine);
		centerFanart = getSnesThemeAsset(themeRoot,
			{ "_systemmedia/_fanartsysteme/fresh/snes-msu1.jpg" },
			"system/snes/background-fanart.jpg", centerFanart);
	}
	std::string description;
	if (isAccessory)
	{
		if (key.find("satell") != std::string::npos || key.find("bs-x") != std::string::npos || key.find("bsx") != std::string::npos)
			description = "Servicio satelital de Super Famicom que difundió juegos y eventos descargables en Japón.";
		else if (key.find("sufami") != std::string::npos)
			description = "Expansión de Bandai para Super Famicom, con ranuras para un cartucho base y otro de expansión.";
		else if (key.find("sgb-msu") != std::string::npos)
			description = "Biblioteca Super Game Boy con audio expandido mediante MSU-1, conservando el adaptador original.";
		else if (key.find("snes-msu") != std::string::npos)
			description = "Expansión comunitaria que añade audio PCM de alta calidad a juegos de Super Nintendo.";
		else
			description = "Adaptador que permitió jugar cartuchos de Game Boy en Super Nintendo, con color y marcos temáticos.";
	}
	else if (isHack)
		description = "Modificaciones creadas por la comunidad que transforman juegos originales con nuevos retos, reglas o contenido.";
	else if (isTranslation)
		description = "Traducciones de la comunidad que adaptan textos y diálogos para disfrutar juegos inéditos en tu idioma.";
	else if (regionKey.find("usa") != std::string::npos || regionKey.find("américa") != std::string::npos || regionKey.find("america") != std::string::npos)
		description = "La Super Nintendo llegó a Norteamérica en 1991 y llevó la generación de 16 bits a una audiencia masiva.";
	else if (regionKey.find("jap") != std::string::npos)
		description = "La Super Famicom debutó en Japón en 1990 y consolidó una generación de clásicos de 16 bits.";
	else
		description = "La Super Nintendo llegó a Europa en 1992, adaptada a las regiones PAL y a su estándar de vídeo.";

	const std::string identityMediaKey = getSnesMediaKey(folder);
	const std::string keyedFigurine = getSnesKeyedThemeAsset(themeRoot, "figurines", identityMediaKey, { "png" });
	const std::string keyedPreviewLogo = getSnesKeyedThemeAsset(themeRoot, "logos", identityMediaKey, { "png", "svg" });
	if (!keyedFigurine.empty())
		figurine = keyedFigurine;
	if (!keyedPreviewLogo.empty())
		logo = keyedPreviewLogo;

	mPreviewBackdrop.setImage(fanart);
	mCenterFanart.setImage(centerFanart);
	mCenterFigurine.setImage(figurine);
	mPreviewLogo.setImage(logo);
	const auto games = folder->getFilesRecursive(GAME);
	std::string hardwareArt;
	if (isAccessory)
	{
		if (key.find("satell") != std::string::npos || key.find("bs-x") != std::string::npos || key.find("bsx") != std::string::npos)
			hardwareArt = getSnesThemeAsset(themeRoot, { "_systemmedia/hardware/satellaview.png" }, "art/consoles/satellaview.png");
		else if (key.find("sufami") != std::string::npos)
			hardwareArt = getSnesThemeAsset(themeRoot, { "_systemmedia/hardware/sufami.png", "_inc/retrobat-snes/media/hardware/sufami.png" }, "art/consoles/sufami.png");
		else if (key.find("snes-msu") != std::string::npos)
			hardwareArt = getSnesThemeAsset(themeRoot, { "_systemmedia/hardware/snesusa.png", "_inc/retrobat-snes/media/hardware/us-snes.png" }, "art/consoles/us/snes.png");
		else
			hardwareArt = getSnesThemeAsset(themeRoot, { "_systemmedia/hardware/nintendosupergameboy.png" }, "art/consoles/sgb.png");
	}
	else if (regionKey.find("usa") != std::string::npos || regionKey.find("america") != std::string::npos)
		hardwareArt = getSnesThemeAsset(themeRoot, { "_systemmedia/hardware/snesusa.png", "_inc/retrobat-snes/media/hardware/us-snes.png" }, "art/consoles/us/snes.png");
	else if (regionKey.find("jap") != std::string::npos)
		hardwareArt = getSnesThemeAsset(themeRoot, { "_systemmedia/hardware/snes-jp.png", "_inc/retrobat-snes/media/hardware/jp-snes.png" }, "art/consoles/jp/snes.png");
	else
		hardwareArt = getSnesThemeAsset(themeRoot, { "_systemmedia/hardware/snes.png", "_inc/retrobat-snes/media/hardware/eu-snes.png" }, "art/consoles/snes.png");
	const std::string hardwareMediaKey = isAccessory ? identityMediaKey : getSnesMediaKey(regionalPresentation);
	const std::string keyedPreviewHardware = getSnesKeyedThemeAsset(themeRoot, "consoles", hardwareMediaKey, { "png" });
	if (!keyedPreviewHardware.empty())
		hardwareArt = keyedPreviewHardware;
	mPreviewGameArt.setImage(hardwareArt);
	const bool enlargeAccessoryConsole = isAccessory &&
		(hardwareMediaKey == "sgb" || hardwareMediaKey == "snes-msu1");
	mPreviewGameArt.setPosition(mSize.x() * 0.8375f,
		mSize.y() * (enlargeAccessoryConsole ? 0.765f : 0.755f));
	mPreviewGameArt.setMaxSize(mSize.x() * 0.245f,
		mSize.y() * (enlargeAccessoryConsole ? 0.210f : 0.180f));
	const std::string translationFlag = isTranslation ? getSnesTranslationFlagPath(folder, themeRoot) : std::string();
	if (!translationFlag.empty())
	{
		mPreviewRegionFlag.setImage(translationFlag);
		const Vector2f hardwareSize = mPreviewGameArt.getSize();
		const Vector3f hardwarePosition = mPreviewGameArt.getPosition();
		mPreviewRegionFlag.setPosition(hardwarePosition.x() - hardwareSize.x() * 0.39f,
			hardwarePosition.y() - hardwareSize.y() * 0.31f);
		mPreviewRegionFlag.setVisible(true);
	}
	else
		mPreviewRegionFlag.setVisible(false);
	mPreviewKind.setText(kind);
	mPreviewTitle.setText(title);
	mPreviewDescription.setText(description);
	const size_t count = games.size();
	int gamesPlayed = 0;
	int favorites = 0;
	std::string lastPlayed;
	FileData* lastPlayedGame = nullptr;
	for (auto game : games)
	{
		if (Utils::String::toInteger(game->getMetadata(MetaDataId::PlayCount)) > 0)
			gamesPlayed++;
		if (game->getFavorite())
			favorites++;

		const std::string gameLastPlayed = game->getMetadata(MetaDataId::LastPlayed);
		if (!gameLastPlayed.empty() && gameLastPlayed > lastPlayed &&
			Utils::String::toInteger(game->getMetadata(MetaDataId::PlayCount)) > 0)
		{
			lastPlayed = gameLastPlayed;
			lastPlayedGame = game;
		}
	}
	if (lastPlayedGame == nullptr && gamesPlayed > 0)
	{
		for (auto game : games)
		{
			if (Utils::String::toInteger(game->getMetadata(MetaDataId::PlayCount)) > 0)
			{
				lastPlayedGame = game;
				break;
			}
		}
	}

	if (lastPlayedGame != nullptr)
	{
		const std::string lastGameImage = getRegionalGameImagePath(lastPlayedGame, folder);
		mPreviewScreenImage.setVisible(false);
		mPreviewEmptyArt.setVisible(false);
		if (!lastGameImage.empty())
		{
			mPreviewScreenImage.setImage(lastGameImage);
			mPreviewScreenImage.setVisible(true);
			mPreviewEmptyMessage.setText("");
			mPreviewEmptyMessage.setVisible(false);
		}
		else
		{
			mPreviewEmptyMessage.setText("IMAGEN DEL JUEGO NO DISPONIBLE");
			mPreviewEmptyMessage.setVisible(true);
		}
	}
	else
	{
		mPreviewEmptyArt.setImage(themeRoot + "/_inc/retrobat-snes/media/empty-library-snes.png");
		mPreviewEmptyArt.setVisible(true);
		mPreviewScreenImage.setVisible(false);
		mPreviewEmptyMessage.setText("TODAVÍA NO HAS JUGADO\nEN ESTA CONSOLA");
		mPreviewEmptyMessage.setVisible(true);
	}

	mPreviewLibraryValue.setText(std::to_string(count));
	mPreviewFolderLabel.setText("JUGADOS");
	mPreviewFolderValue.setText(std::to_string(gamesPlayed) + "/" + std::to_string(count));
	mPreviewStatusLabel.setText("FAVORITOS");
	mPreviewStatusValue.setText(favorites == 0 ? "NINGUNO" : std::to_string(favorites));
	mPreviewLastLabel.setText("ÚLTIMO JUGADO");
	mPreviewLastValue.setText(formatPlayedDate(lastPlayed));
	mPreviewHint.setText("");
	updateFocusIndicator();
}

void GuiSnesVariantSelector::updateFocusIndicator()
{
	if (mMenu.getList() == nullptr)
		return;

	const std::string selectedId = mMenu.getList()->getSelectedUserData();
	const bool translationLaneFocused = selectedId == TRANSLATION_ROW_ID;
	const bool accessoryLaneFocused = selectedId == ACCESSORY_ROW_ID;
	const std::string focusTarget = translationLaneFocused
		? TRANSLATION_ROW_ID + ":" + std::to_string(mTranslationCursor)
		: accessoryLaneFocused
			? ACCESSORY_ROW_ID + ":" + std::to_string(mAccessoryCursor)
			: selectedId;
	for (auto& entry : mFocusIcons)
		entry.second->setVisible(entry.first == focusTarget);
	for (size_t index = 0; index < mTranslationCounts.size(); ++index)
		mTranslationCounts[index]->setColor(translationLaneFocused && static_cast<int>(index) == mTranslationCursor
			? 0xFFFFFFFF
			: 0xD7E4EAFF);
	for (size_t index = 0; index < mAccessoryCounts.size(); ++index)
		mAccessoryCounts[index]->setColor(accessoryLaneFocused && static_cast<int>(index) == mAccessoryCursor
			? 0xFFFFFFFF
			: 0xD7E4EAFF);

	const std::string focusKey = selectedId +
		(translationLaneFocused ? ":" + std::to_string(mTranslationCursor) :
			accessoryLaneFocused ? ":" + std::to_string(mAccessoryCursor) : std::string());
	if (focusKey != mFocusKey)
	{
		mFocusKey = focusKey;
		mFocusPulseRemaining = FOCUS_PULSE_DURATION_MS;
	}
}

void GuiSnesVariantSelector::updateFocusPulse(int deltaTime)
{
	if (mFocusPulseRemaining <= 0)
		return;

	mFocusPulseRemaining = std::max(0, mFocusPulseRemaining - std::max(0, deltaTime));
	const float progress = 1.0f - static_cast<float>(mFocusPulseRemaining) / FOCUS_PULSE_DURATION_MS;
	const float pulse = std::sin(progress * 3.14159265f);
	const float scale = 1.0f + 0.16f * pulse;
	const unsigned char opacity = static_cast<unsigned char>(190 + 65 * pulse);
	for (auto& entry : mFocusIcons)
	{
		entry.second->setScale(scale);
		entry.second->setOpacity(opacity);
	}
	if (mFocusPulseRemaining == 0)
	{
		for (auto& entry : mFocusIcons)
		{
			entry.second->setScale(1.0f);
			entry.second->setOpacity(255);
		}
	}
}

bool GuiSnesVariantSelector::input(InputConfig* config, Input input)
{
	// The theme maps the canonical Y action to the PlayStation square icon.
	// Use it as the direct "make default" action inside this modal; the
	// accessory folders remain selectable libraries but are never eligible.
	if (input.value != 0 && config->isMappedTo("y", input))
	{
		FolderData* folder = getSelectedFolder();
		if (isDefaultEligible(folder))
		{
			const bool saved = setDefaultVariant(mSystem, folder);
			mDefaultChanged = saved || mDefaultChanged;
			if (saved)
				updateDefaultBadge(true);
			updatePreview();
		}
		return true;
	}

	if (GuiComponent::input(config, input))
		return true;

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		closeSelector();
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiSnesVariantSelector::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, "VOLVER"));
	prompts.push_back(HelpPrompt(BUTTON_OK, "ABRIR"));
	if (isDefaultEligible(getSelectedFolder()))
		prompts.push_back(HelpPrompt("y", "PREDETERMINADA"));
	else if (mMenu.getList()->getSelectedUserData() == TRANSLATION_ROW_ID)
		prompts.push_back(HelpPrompt("left/right", "IDIOMA"));
	else if (mMenu.getList()->getSelectedUserData() == ACCESSORY_ROW_ID)
		prompts.push_back(HelpPrompt("left/right", "ACCESORIO"));
	return prompts;
}
