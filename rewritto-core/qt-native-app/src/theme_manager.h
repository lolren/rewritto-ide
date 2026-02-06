#pragma once

#include <QString>

class ThemeManager final {
 public:
  static void init();
  static void apply(const QString& theme);
};

