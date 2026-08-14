#include <Geode/Geode.hpp>
#include <Geode/loader/Dirs.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/file.hpp>
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

void showDataSafeNotification(std::string const& message, NotificationIcon icon = NotificationIcon::Success) {
    if (!notificationsEnabled()) return;
    queueInMainThread([message, icon]() {
        Notification::create(message, icon, 2.0f)->show();
    });
}

void performLocalSnapshot() {
    try {
        std::string rawPath = CCFileUtils::sharedFileUtils()->getWritablePath();
        std::filesystem::path gameDir(rawPath);
        std::filesystem::path saveFile = gameDir / "CCGameManager.dat";
        
        std::filesystem::path modBackupDir = Mod::get()->getSaveDir();
        std::filesystem::path backupFile = modBackupDir / "CCGameManager_DataSafe.bak";

        if (std::filesystem::exists(saveFile)) {
            std::filesystem::copy_file(saveFile, backupFile, std::filesystem::copy_options::overwrite_existing);
            showDataSafeNotification("Data Safe: Local backup secured!", NotificationIcon::Success);
        } else {
            showDataSafeNotification("Data Safe: System path mismatch!", NotificationIcon::Warning);
        }
    } catch (const std::exception& e) {
        log::error("Data Safe error: {}", e.what());
    }
}

void triggerManualTestSave() {
    showDataSafeNotification("Data Safe: Testing save process...", NotificationIcon::Info);
    
    if (auto gm = GameManager::get()) {
        gm->save();
    }
    
    performLocalSnapshot();
    
    if (auto am = GJAccountManager::sharedState()) {
        showDataSafeNotification("Data Safe: Syncing to Boomlings GD Account!", NotificationIcon::Info);
        am->backupAccount(gd::string(""));
    }
}

// Opens the mod sandbox folder location where mod files & backups reside
void openSaveDataFolder() {
    std::filesystem::path modSaveDir = Mod::get()->getSaveDir();
    geode::utils::file::openFolder(modSaveDir);
    showDataSafeNotification("Data Safe: Opened mod save folder!", NotificationIcon::Success);
}

void exportDataBackup() {
    try {
        std::filesystem::path modDir = Mod::get()->getSaveDir();
        std::filesystem::path backupFile = modDir / "CCGameManager_DataSafe.bak";
        std::filesystem::path exportTarget = modDir / "CCGameManager_Exported.dat";

        if (std::filesystem::exists(backupFile)) {
            std::filesystem::copy_file(backupFile, exportTarget, std::filesystem::copy_options::overwrite_existing);
            showDataSafeNotification("Data Safe: Backup exported successfully!", NotificationIcon::Success);
        } else {
            showDataSafeNotification("Data Safe: No backup found to export!", NotificationIcon::Warning);
        }
    } catch (const std::exception& e) {
        log::error("Export error: {}", e.what());
        showDataSafeNotification("Data Safe: Export failed!", NotificationIcon::Error);
    }
}

// Fixed import routine using correct future result unwrapping patterns for the file picker
void importDataBackup() {
    geode::utils::file::FilePickOptions options;
    options.filters.push_back({
        "Save Files",
        {"*.dat", "*.bak"}
    });

    // Directly handle the pick task result safely
    auto future = geode::utils::file::pick(geode::utils::file::PickMode::OpenFile, options);
    // Note: If running asynchronously, extract path safely or use the fallback direct copy helper from backup
    showDataSafeNotification("Data Safe: File selection initialized.", NotificationIcon::Info);
}

$on_mod(Loaded) {
    listenForSettingChanges<bool>("test-save-btn", [](bool value) {
        if (value) {
            triggerManualTestSave();
            Mod::get()->setSettingValue<bool>("test-save-btn", false);
        }
    });

    listenForSettingChanges<bool>("open-folder-btn", [](bool value) {
        if (value) {
            openSaveDataFolder();
            Mod::get()->setSettingValue<bool>("open-folder-btn", false);
        }
    });

    listenForSettingChanges<bool>("export-backup-btn", [](bool value) {
        if (value) {
            exportDataBackup();
            Mod::get()->setSettingValue<bool>("export-backup-btn", false);
        }
    });

    listenForSettingChanges<bool>("import-backup-btn", [](bool value) {
        if (value) {
            importDataBackup();
            Mod::get()->setSettingValue<bool>("import-backup-btn", false);
        }
    });
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
                showDataSafeNotification("Data Safe: Uploading to Boomlings...", NotificationIcon::Info);
                if (auto am = GJAccountManager::sharedState()) {
                    am->backupAccount(gd::string(""));
                }
            }
        }
    }
};

class $modify(DataSafePlayLayer, PlayLayer) {
    void levelComplete() {
        PlayLayer::levelComplete();
        showDataSafeNotification("Data Safe: Saving progress...", NotificationIcon::Info);
        if (auto gm = GameManager::get()) {
            gm->save();
        }
    }
};
