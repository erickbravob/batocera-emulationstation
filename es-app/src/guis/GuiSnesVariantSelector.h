#pragma once
#ifndef ES_APP_GUIS_GUI_SNES_VARIANT_SELECTOR_H
#define ES_APP_GUIS_GUI_SNES_VARIANT_SELECTOR_H

#include "components/MenuComponent.h"
#include "components/ImageComponent.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"

#include <vector>
#include <utility>

class FolderData;
class SystemData;

class GuiSnesVariantSelector : public GuiComponent
{
public:
	GuiSnesVariantSelector(Window* window, SystemData* system);

	static bool hasVariants(SystemData* system);
	static FolderData* getDefaultVariant(SystemData* system);
	static void openVariant(SystemData* system, FolderData* folder);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	static bool isDefaultEligible(FolderData* folder);
	static bool setDefaultVariant(SystemData* system, FolderData* folder);

	void closeSelector();
	void updateDefaultBadge(bool animate);
	void updatePreview();
	void updateFocusIndicator();
	void updateFocusPulse(int deltaTime);
	FolderData* getSelectedFolder();

	MenuComponent mMenu;
	SystemData* mSystem;
	std::vector<FolderData*> mVariants;
	std::vector<std::pair<FolderData*, std::shared_ptr<TextComponent>>> mVariantBadges;
	std::vector<FolderData*> mTranslationFolders;
	std::vector<FolderData*> mAccessoryFolders;
	std::vector<std::shared_ptr<TextComponent>> mTranslationCounts;
	std::vector<std::shared_ptr<TextComponent>> mAccessoryCounts;
	std::vector<std::pair<std::string, std::shared_ptr<ImageComponent>>> mFocusIcons;
	std::vector<std::shared_ptr<ImageComponent>> mSectionAccentLines;
	std::vector<std::shared_ptr<ImageComponent>> mSectionPanels;
	std::vector<std::shared_ptr<GuiComponent>> mSectionLedRunners;
	std::vector<std::pair<std::string, int>> mSelectionRows;
	int mTranslationCursor = 0;
	int mAccessoryCursor = 0;
	int mFocusPulseRemaining = 0;
	std::string mFocusKey;
	bool mDefaultChanged = false;
	bool mAnimateDefaultBadges = false;

	ImageComponent mModalBackdrop;
	ImageComponent mCard;
	NinePatchComponent mCardInnerFrame;
	ImageComponent mPreviewBackdrop;
	ImageComponent mCenterFanart;
	ImageComponent mCenterFigurine;
	ImageComponent mPreviewPanel;
	ImageComponent mPreviewScreenFrame;
	ImageComponent mPreviewScreenSurface;
	std::vector<std::shared_ptr<ImageComponent>> mShowcaseScreens;
	std::vector<std::shared_ptr<ImageComponent>> mShowcaseHardware;
	ImageComponent mPreviewLogo;
	ImageComponent mPreviewGameArt;
	ImageComponent mPreviewRegionFlag;
	ImageComponent mPreviewScreenImage;
	ImageComponent mPreviewEmptyArt;
	ImageComponent mPreviewLibraryIcon;
	ImageComponent mPreviewFavoritesIcon;
	ImageComponent mPreviewPlayedIcon;
	ImageComponent mPreviewLastPlayedIcon;
	TextComponent mPreviewEmptyMessage;
	TextComponent mPreviewKind;
	TextComponent mPreviewTitle;
	TextComponent mPreviewDescription;
	TextComponent mPreviewLibraryLabel;
	TextComponent mPreviewLibraryValue;
	TextComponent mPreviewFolderLabel;
	TextComponent mPreviewFolderValue;
	TextComponent mPreviewStatusLabel;
	TextComponent mPreviewStatusValue;
	TextComponent mPreviewLastLabel;
	TextComponent mPreviewLastValue;
	TextComponent mPreviewHint;
	ImageComponent mPreviewGoldRule;
	ImageComponent mPreviewCyanRule;
};

#endif // ES_APP_GUIS_GUI_SNES_VARIANT_SELECTOR_H
