#!/usr/bin/env python3
"""Static guardrails for ES-X's lightweight Font Awesome metadata file picker."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MediaPickerTTFTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.gui = (ROOT / "es-app/src/guis/GuiFileBrowser.cpp").read_text()
        cls.header = (ROOT / "es-app/src/guis/GuiFileBrowser.h").read_text()
        cls.editor = (ROOT / "es-app/src/guis/GuiMetaDataEd.cpp").read_text()
        cls.cmake = (ROOT / "es-app/CMakeLists.txt").read_text()

    def test_reuses_existing_fontawesome_without_new_image_icons(self):
        self.assertIn("TaskGlyphs::FONT_PATH", self.gui)
        for glyph in (r"\uf07c", r"\uf03e", r"\uf03d", r"\uf062"):
            self.assertIn(glyph, self.gui)
        self.assertNotIn("folder.png", self.gui)
        self.assertNotIn("image.png", self.gui)
        self.assertIn("GuiFileBrowser.cpp", self.cmake)
        self.assertIn("GuiFileBrowser.h", self.cmake)

    def test_only_metadata_media_types_use_browser(self):
        self.assertIn("case MD_PATH:", self.editor)
        self.assertIn("GuiFileBrowser::MediaKind::VIDEO", self.editor)
        self.assertIn("GuiFileBrowser::MediaKind::IMAGE", self.editor)
        self.assertIn("case MD_MULTILINE_STRING:", self.editor)
        self.assertIn("new GuiFileBrowser(", self.editor)

    def test_no_extra_file_management_or_shell_commands(self):
        self.assertNotIn("std::system(", self.gui)
        self.assertNotIn("popen(", self.gui)
        self.assertNotIn("removeFile(", self.gui)
        self.assertNotIn("copyFile(", self.gui)

    def test_native_extension_filter_and_navigation(self):
        self.assertIn("getDirContent(mCurrentDirectory)", self.gui)
        self.assertIn("Utils::String::toLower(Utils::FileSystem::getExtension(path))", self.gui)
        for ext in (".png", ".jpeg", ".jpg", ".gif", ".svg", ".mp4", ".webm"):
            self.assertIn('"' + ext + '"', self.gui)
        self.assertIn("createRelativePath(path, romRoot, false, true)", self.editor)
        self.assertIn("mCurrentDirectory", self.header)
        self.assertIn('config->isMappedTo("x", input)', self.gui)
        self.assertIn('config->isMappedTo("y", input)', self.gui)

    def test_callbacks_defer_list_mutation_until_after_input(self):
        self.assertIn("mPendingDirectory = parent", self.gui)
        self.assertIn("mPendingDirectory = folder", self.gui)
        self.assertIn("mHasPendingChoice = true", self.gui)
        self.assertIn("const bool handled = mList.input(config, input)", self.gui)
        self.assertIn("choose(mPendingFile)", self.gui)
        self.assertIn("navigate(directory)", self.gui)
        self.assertIn("mHasPendingChoice = false", self.gui)
        self.assertIn("mPendingDirectory", self.header)

    def test_locales_include_new_controls(self):
        for name in ("en", "es", "de", "fr", "it", "pt", "ru", "ja"):
            text = (ROOT / "lang" / (name + ".ini")).read_text()
            for key in ("SELECT IMAGE", "SELECT VIDEO", "PARENT FOLDER", "RESET", "NO COMPATIBLE FILES"):
                self.assertIn(key + "=", text, (name, key))


if __name__ == "__main__":
    unittest.main()
