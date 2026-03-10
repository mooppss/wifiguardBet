#ifndef SETTINGS_H
#define SETTINGS_H

#include "Config.h"
#include "Types.h"

#define NVS_NAMESPACE "wguard"

class Settings {
public:
  void begin();
  void load(SettingsRecord& out) const;
  void save(const SettingsRecord& in);
  const SettingsRecord& get() const { return data_; }

private:
  SettingsRecord data_;
};

extern Settings settings;

#endif
