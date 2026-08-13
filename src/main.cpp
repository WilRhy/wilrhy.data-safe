#include <Geode/Geode.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <chrono>
#include <filesystem>

using namespace geode::prelude;

static std::chrono::steady_clock::time_point g_lastCloudBackup;
static const long long CLOUD_COOLDOWN_SECONDS = 15; 

bool notificationsEnabled() {
    return Mod::get()->getSettingValue<bool>("show-notifications");
}

bool cloudBackupEnabled() {
    return Mod::get()->getSettingValue<bool>("auto-cloud-backup");
}

void performLocalSnapshot() {
    try {
        std::filesystem::path localDir = Mod::get()->getSaveDir();
        std::filesystem::path saveFile = localDir / "CCGameManager.dat";
        std::filesystem::path backupFile = localDir / "CCGameManager_DataSafe.bak";

        if (std::filesystem::exists(saveFile)) {
            std::filesystem::copy_file(saveFile, backupFile, std::filesystem::copy_options::overwrite_existing);
            if (notificationsEnabled()) {
                Notification::create("Data Safe: Local backup secured!", NotificationIcon::Success, 1.5f)->show();
            }
        }
    } catch (const std::exception& e) {
        log::error("Data Safe error: {}", e.what());
    }
}

class $modify(DataSafeGameManager, GameManager) {
    void save() {
        GameManager::save();
        performLocalSnapshot();

        if (cloudBackupEnabled()) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_lastCloudBackup).count();
            
            if (elapsed >= CLOUD_COOLDOWN_SECONDS) {
                g_lastCloudBackup = now;
                
                if (notificationsEnabled()) {
                    Notification::create("Data Safe: Uploading to Boomlings...", NotificationIcon::Info, 2.0f)->show();
                }
                
                // Passed an empty string argument to satisfy backupAccount's expected string parameter signature
                if (auto am = GJAccountManager::sharedState()) {
                    am->backupAccount("");
                }
            }
        }
    }
};

class $modify(DataSafePlayLayer, PlayLayer) {
    void levelComplete() {
        PlayLayer::levelComplete();
        
        if (notificationsEnabled()) {
            Notification::create("Data Safe: Saving progress...", NotificationIcon::Info, 1.5f)->show();
        }
        
        if (auto gm = GameManager::get()) {
            gm->save();
        }
    }
};
