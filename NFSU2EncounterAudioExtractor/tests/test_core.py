from __future__ import annotations

import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from encounter_extractor.core import BigEntry, CATEGORY_BY_SELECTOR, ExtractionError, VoiceRecord, discover_languages, output_filename, parse_big4


class CoreTests(unittest.TestCase):
    def test_selector_categories_are_complete_and_disjoint(self) -> None:
        self.assertEqual(set(CATEGORY_BY_SELECTOR), set(range(8)))
        self.assertEqual({CATEGORY_BY_SELECTOR[i] for i in (0, 7)}, {"start"})
        self.assertEqual({CATEGORY_BY_SELECTOR[i] for i in (3, 4, 5)}, {"player_victory"})
        self.assertEqual({CATEGORY_BY_SELECTOR[i] for i in (1, 2, 6)}, {"player_defeat"})

    def test_parse_small_big4(self) -> None:
        name = b"speech/test.bin\0"
        header_size = 16 + 8 + len(name)
        payload = b"payload"
        body = b"BIG4" + struct.pack(">III", header_size + len(payload), 1, header_size)
        body += struct.pack(">II", header_size, len(payload)) + name + payload
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "sdat.viv"
            path.write_bytes(body)
            entries = parse_big4(path)
            self.assertEqual(entries[0].name, "speech/test.bin")
            self.assertEqual(entries[0].size, len(payload))

    def test_rejects_size_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bad.viv"
            path.write_bytes(b"BIG4" + struct.pack(">III", 999, 1, 16))
            with self.assertRaises(ExtractionError):
                parse_big4(path)

    def test_discovers_all_known_retail_language_names(self) -> None:
        entries = []
        offset = 1024
        for suffix in ("", "_FR", "_GR", "_IT", "_JP", "_SP"):
            stem = f"speech/UG2_Speech{suffix}"
            for extension in ("big", "idx", "evt"):
                entries.append(BigEntry(f"{stem}.{extension}", offset, 16))
                offset += 16
        with patch("encounter_extractor.core.parse_big4", return_value=entries):
            found = discover_languages(Path("unused.viv"))
        self.assertEqual([item.code for item in found], ["EN", "FR", "GR", "IT", "JP", "SP"])

    def test_native_free_roam_racers_filename_has_no_language(self) -> None:
        record = VoiceRecord(9, 2, 58, 1, 2, 123, 470, 0, 256, "player_defeat")
        self.assertEqual(
            output_filename(record),
            "player_defeat_speaker02_cue058_v01_class2_stream0470.wav",
        )
        self.assertNotIn("JP", output_filename(record))


if __name__ == "__main__":
    unittest.main()
