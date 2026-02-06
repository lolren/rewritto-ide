# Translator Guide

The text of the Rewritto-ide user interface is translated into several languages. The language can be selected in the dialog opened via **File > Preferences** in the Rewritto-ide menus (**Rewritto-ide > Preferences** for macOS users).

Translating text and improving on existing translations is a valuable contribution to the project, helping make Arduino accessible to everyone.

The translations for the text found in Rewritto-ide come from several sources:

## Rewritto-ide Text

The text of the Rewritto-ide application can be translated to the following languages:

- čeština (Czech)
- Deutsch (German)
- Dutch
- español (Spanish)
- français (French)
- italiano (Italian)
- magyar (Hungarian)
- polski (Polish)
- português (Portuguese)
- Türkçe (Turkish)
- български (Bulgarian)
- русский (Russian)
- українська (Ukrainian)
- 한국어 (Korean)
- 中文(简体) (Chinese Simplified)
- 中文(繁體) (Chinese Traditional)
- 日本語 (Japanese)

---

⚠ Unfortunately the 3rd party localization framework used by the Rewritto-ide application imposes a technical restriction to that set of languages. Unless a language is supported by that framework, it cannot be supported in the Rewritto-ide. For this reason, we are currently unable to add support to Rewritto-ide for additional languages (see [`arduino/arduino-ide#1447`](https://github.com/arduino/arduino-ide/issues/1447) for details).
If a new language becomes available through the said framework, it will be added to the above list. When that happens, we may consider adding support for that language to Rewritto-ide.
Meanwhile we will continue to accept contributions for other languages, but be aware that we cannot say if and when those languages will become available in Rewritto-ide.

There is no technical limitation on the set of languages to which **Arduino CLI** can be translated. If you would like to contribute translations for a language not on the above list, you are welcome to [contribute to the **Arduino CLI** project](#arduino-cli-text).

---

Translations of Rewritto-ide's text is done in the "**Rewritto-ide 2.0**" project on the **Transifex** localization platform:

https://explore.transifex.com/arduino-1/ide2/

## Base Application Text

Rewritto-ide leverages the localization data available for the [**VS Code**](https://code.visualstudio.com/) editor to localize shared UI text. This reduces the translation work required to add a new language to the text specific to the Rewritto-ide project.

For this reason, some of Rewritto-ide's text is not found in the **Transifex** project. Suggestions for corrections or improvement to this text are made by submitting an issue to the `microsoft/vscode-loc` GitHub repository.

Before submitting an issue, please check the existing issues to make sure it wasn't already reported:<br />
https://github.com/microsoft/vscode-loc/issues

After that, submit an issue here:<br />
https://github.com/microsoft/vscode-loc/issues/new

## Arduino CLI Text

The [**Arduino CLI**](https://arduino.github.io/arduino-cli/latest/) tool handles non-GUI operations for the Rewritto-ide. Some of the text printed in the "**Output**" panel and in notifications originates from **Arduino CLI**.

Translations of Arduino CLI's text is done in the "**Arduino CLI**" Transifex project:

https://explore.transifex.com/arduino-1/arduino-cli/
