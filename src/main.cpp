#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <filesystem>

using namespace geode::prelude;

static bool g_isExporting = false;

void executeDataSafeExport() {
    if (g_isExporting) return;
    g_isExporting = true;

    try {
        std::filesystem::path localDataPath = Mod::get()->getSaveDir();
        std::filesystem::path primarySave = localDataPath / "CCGameManager.dat";
        std::filesystem::path exportTarget = localDataPath / "CCGameManager_DataSafe.bak";

        if (std::filesystem::exists(primarySave)) {
            std::filesystem::copy_file(primarySave, exportTarget, std::filesystem::copy_options::overwrite_existing);
            log::info("Data Safe: Successfully backed up local binary stream safely.");
        }
    } catch (const std::exception& e) {
        log::error("Data Safe Failure: {}", e.what());
    }

    g_isExporting = false;
}

// Hook into MenuLayer - removed override
class $modify(DataSafeMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        if (auto bottomMenu = this->getChildByID("bottom-menu")) {
            auto btnSprite = CircleButtonSprite::createWithSpriteFrameName(
                "GJ_optionsBtn_001.png", 0.8f, CircleBaseColor::Green
            );
            
            auto btn = CCMenuItemSpriteExtra::create(
                btnSprite, this, menu_selector(DataSafeMenuLayer::onDataSafeClicked)
            );
            btn->setID("data-safe-menu-button");
            
            bottomMenu->addChild(btn);
            bottomMenu->updateLayout();
        }

        return true;
    }

    void onDataSafeClicked(CCObject* sender) {
        auto alert = FLAlertLayer::create(
            "Data Safe Panel",
            "Select an operation:\n\nPress <cg>Export</c> to create an alternate file snapshot.\nPress <cy>Import</c> to restore records.",
            "Cancel"
        );
        alert->show();
        executeDataSafeExport();
    }
};

// Hook into PlayLayer - removed override
class $modify(DataSafePlayLayer, PlayLayer) {
    void levelComplete() {
        PlayLayer::levelComplete();
        
        if (Mod::get()->getSettingValue<bool>("show-notifications")) {
            Notification::create("Data Safe: Snapshot created!", NotificationIcon::Success, 1.5f)->show();
        }
        
        executeDataSafeExport();
    }
};
