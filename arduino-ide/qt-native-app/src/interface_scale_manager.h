#pragma once

class UiScaleManager final {
 public:
  static void init();
  static double currentScale();
  static void apply(double scale);
};
