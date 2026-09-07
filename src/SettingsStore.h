#pragma once

#include "ofMain.h"

namespace SettingsStore {

inline std::string directoryPath() {
    return ofFilePath::join(
        ofFilePath::getUserHomeDir(),
        "Library/Application Support/PartituraDelJuego");
}

inline std::string settingsPath() {
    return ofFilePath::join(directoryPath(), "settings.json");
}

inline std::string bundledDefaultsPath() {
    return ofToDataPath("settings.json", true);
}

inline bool ensureUserSettings(std::string* error = nullptr) {
    const std::string directory = directoryPath();
    if (!ofDirectory::doesDirectoryExist(directory) &&
        !ofDirectory::createDirectory(directory, true, true)) {
        if (error) *error = "Could not create " + directory;
        return false;
    }

    const std::string destination = settingsPath();
    if (ofFile::doesFileExist(destination)) return true;

    const std::string source = bundledDefaultsPath();
    const ofBuffer defaults = ofBufferFromFile(source);
    if (defaults.size() == 0) {
        if (error) *error = "Bundled settings are missing or empty: " + source;
        return false;
    }
    if (!ofBufferToFile(destination, defaults)) {
        if (error) *error = "Could not create " + destination;
        return false;
    }
    return true;
}

inline void mergeObject(ofJson& destination, const ofJson& overrides) {
    if (!destination.is_object() || !overrides.is_object()) {
        destination = overrides;
        return;
    }
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        if (destination.contains(it.key()) &&
            destination[it.key()].is_object() && it.value().is_object()) {
            mergeObject(destination[it.key()], it.value());
        } else {
            destination[it.key()] = it.value();
        }
    }
}

inline ofJson load(std::string* error = nullptr) {
    ofJson settings = ofLoadJson(bundledDefaultsPath());
    if (!settings.is_object())
        settings = ofJson::object();
    if (ensureUserSettings(error)) {
        const ofJson userSettings = ofLoadJson(settingsPath());
        if (userSettings.is_object()) {
            mergeObject(settings, userSettings);
            return settings;
        }
        if (error)
            *error = "User settings are invalid: " + settingsPath();
    }

    if (settings.is_object())
        return settings;
    if (error && error->empty())
        *error = "Bundled settings are missing or invalid";
    return ofJson::object();
}

inline bool save(const ofJson& settings, std::string* error = nullptr) {
    if (!ensureUserSettings(error)) return false;

    ofBuffer serialized;
    serialized.set(settings.dump(4) + "\n");
    if (!ofBufferToFile(settingsPath(), serialized)) {
        if (error) *error = "Could not write " + settingsPath();
        return false;
    }
    return true;
}

}  // namespace SettingsStore
