from __future__ import annotations

import csv
import hashlib
import json
import re
import shutil
import struct
import subprocess
import tempfile
import wave
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Callable, Iterable


LANGUAGES = {
    "EN": "English",
    "FR": "Français / French",
    "GR": "Deutsch / German",
    "IT": "Italiano / Italian",
    "JP": "日本語 / Japanese",
    "SP": "Español / Spanish",
}
SUFFIX_TO_LANGUAGE = {"": "EN", "_FR": "FR", "_GR": "GR", "_IT": "IT", "_JP": "JP", "_SP": "SP"}
CATEGORY_BY_SELECTOR = {
    0: "start",
    7: "start",
    3: "player_victory",
    4: "player_victory",
    5: "player_victory",
    1: "player_defeat",
    2: "player_defeat",
    6: "player_defeat",
}
CATEGORY_LABELS = {
    "start": "Encounter start / エンカウンター開始",
    "player_victory": "Player victory / プレイヤー勝利",
    "player_defeat": "Player defeat / プレイヤー敗北",
}
OUTRUN_GROUPS = range(9, 14)
Progress = Callable[[int, int, str], None]


class ExtractionError(RuntimeError):
    pass


class CancelledError(ExtractionError):
    pass


@dataclass(frozen=True)
class BigEntry:
    name: str
    offset: int
    size: int


@dataclass(frozen=True)
class LanguageBank:
    code: str
    bank: BigEntry
    idx: BigEntry
    evt: BigEntry


@dataclass(frozen=True)
class VoiceRecord:
    group_index: int
    speaker_id: int
    cue_id: int
    variant_index: int
    selector_class: int
    duration_units: int
    stream_index: int
    bank_offset: int
    bank_end: int
    category: str


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def parse_big4(path: Path) -> list[BigEntry]:
    path = path.resolve()
    actual_size = path.stat().st_size
    with path.open("rb") as source:
        header = source.read(16)
        if len(header) != 16 or header[:4] not in (b"BIG4", b"BIGF"):
            raise ExtractionError("The selected file is not an EA BIG4/BIGF archive. / EA BIG4/BIGF 形式ではありません。")
        declared_size, count, header_size = struct.unpack(">III", header[4:16])
        if declared_size != actual_size:
            raise ExtractionError(f"Archive size mismatch / アーカイブサイズ不一致: {declared_size} != {actual_size}")
        if count == 0 or count > 100000 or header_size < 16 or header_size > actual_size:
            raise ExtractionError("Invalid BIG archive header. / BIG ヘッダーが不正です。")
        entries: list[BigEntry] = []
        for _ in range(count):
            raw = source.read(8)
            if len(raw) != 8:
                raise ExtractionError("Truncated BIG file table. / BIG ファイル表が途中で切れています。")
            offset, size = struct.unpack(">II", raw)
            name_bytes = bytearray()
            while True:
                item = source.read(1)
                if not item:
                    raise ExtractionError("Unterminated BIG filename. / BIG 内ファイル名が不正です。")
                if item == b"\0":
                    break
                name_bytes.extend(item)
                if len(name_bytes) > 1024:
                    raise ExtractionError("BIG filename is too long. / BIG 内ファイル名が長すぎます。")
            name = name_bytes.decode("ascii", errors="strict").replace("\\", "/")
            if offset < header_size or size < 0 or offset + size > actual_size:
                raise ExtractionError(f"BIG entry is outside the archive / 範囲外の項目: {name}")
            entries.append(BigEntry(name, offset, size))
        if source.tell() > header_size:
            raise ExtractionError("BIG file table exceeds its header. / BIG ファイル表がヘッダー範囲を超えています。")
    return entries


def discover_languages(path: Path) -> list[LanguageBank]:
    entries = {entry.name.lower(): entry for entry in parse_big4(path)}
    found: list[LanguageBank] = []
    pattern = re.compile(r"^speech/ug2_speech(_fr|_gr|_it|_jp|_sp)?\.big$", re.IGNORECASE)
    for entry in entries.values():
        match = pattern.match(entry.name)
        if not match:
            continue
        suffix = (match.group(1) or "").upper()
        code = SUFFIX_TO_LANGUAGE.get(suffix)
        if not code:
            continue
        stem = entry.name[:-4].lower()
        idx = entries.get(stem + ".idx")
        evt = entries.get(stem + ".evt")
        if idx and evt:
            found.append(LanguageBank(code, entry, idx, evt))
    found.sort(key=lambda item: list(LANGUAGES).index(item.code))
    if not found:
        raise ExtractionError("No supported UG2 speech bank was found. / 対応する UG2 音声バンクが見つかりません。")
    return found


def _read_entry(archive: Path, entry: BigEntry) -> bytes:
    with archive.open("rb") as source:
        source.seek(entry.offset)
        data = source.read(entry.size)
    if len(data) != entry.size:
        raise ExtractionError(f"Could not read {entry.name}. / 読み込みに失敗しました。")
    return data


def _stream_offsets(bank: bytes) -> list[int]:
    offsets: list[int] = []
    position = 0
    while True:
        position = bank.find(b"SCHl", position)
        if position < 0:
            break
        if position % 0x100 == 0 and position + 8 <= len(bank):
            header_size = struct.unpack_from("<I", bank, position + 4)[0]
            if 8 <= header_size <= 0x100 and position + header_size <= len(bank):
                offsets.append(position)
        position += 4
    if not offsets or offsets[0] != 0:
        raise ExtractionError("Speech bank has no valid SCHl stream at offset zero. / 音声バンク先頭に SCHl がありません。")
    return offsets


def _idx_groups(idx: bytes) -> list[dict[str, int]]:
    if len(idx) < 0x58:
        raise ExtractionError("Speech IDX is too short. / 音声 IDX が短すぎます。")
    count = struct.unpack_from("<I", idx, 0x54)[0]
    if count == 0 or count > 4096 or 0x58 + count * 16 > len(idx):
        raise ExtractionError("Invalid speech IDX group table. / 音声 IDX グループ表が不正です。")
    return [
        dict(zip(("event_id", "record_size", "record_offset", "bank_offset"), struct.unpack_from("<IIII", idx, 0x58 + i * 16)))
        for i in range(count)
    ]


def analyze_bank(archive: Path, language: LanguageBank) -> tuple[bytes, list[VoiceRecord]]:
    bank = _read_entry(archive, language.bank)
    idx = _read_entry(archive, language.idx)
    evt = _read_entry(archive, language.evt)
    if b"Outrun\0" not in evt and b"OUTRUN\0" not in evt.upper():
        raise ExtractionError("Outrun event marker was not found. / Outrun イベント識別子が見つかりません。")
    offsets = _stream_offsets(bank)
    groups = _idx_groups(idx)
    if len(groups) < 14:
        raise ExtractionError("Speech IDX does not contain the expected Outrun groups. / 必要な Outrun グループがありません。")
    offset_set = set(offsets)
    for group_index in OUTRUN_GROUPS:
        if groups[group_index]["bank_offset"] not in offset_set:
            raise ExtractionError(f"Outrun group {group_index} does not start on an SCHl stream. / SCHl 境界と一致しません。")

    records: list[VoiceRecord] = []
    variants: dict[tuple[int, int], int] = {}
    for group_index in OUTRUN_GROUPS:
        group = groups[group_index]
        group_start = group["bank_offset"]
        group_end = groups[group_index + 1]["bank_offset"] if group_index + 1 < len(groups) else len(bank)
        group_streams = [offset for offset in offsets if group_start <= offset < group_end]
        payload_start = group["record_offset"]
        payload_end = payload_start + group["record_size"]
        if payload_start < 0 or payload_end > len(idx):
            raise ExtractionError(f"Outrun group {group_index} record block is outside IDX. / IDX 範囲外です。")
        payload = idx[payload_start:payload_end]
        required = 16 + len(group_streams) * 5 + 16
        if len(payload) < required:
            raise ExtractionError(f"Outrun group {group_index} record block is too short. / レコードが不足しています。")
        for local_index, stream_offset in enumerate(group_streams):
            speaker_id, cue_id, duration_units, selector_class = struct.unpack_from("<BBHB", payload, 16 + local_index * 5)
            category = CATEGORY_BY_SELECTOR.get(selector_class)
            if category is None:
                raise ExtractionError(f"Unknown Outrun selector class {selector_class}. / 未知の選択クラスです。")
            # NativeFreeRoamRacers numbers variants per speaker/group and cue,
            # including records that use a different selector class.
            key = (group_index, cue_id)
            variant = variants.get(key, 0)
            variants[key] = variant + 1
            global_index = offsets.index(stream_offset) + 1
            bank_end = offsets[global_index] if global_index < len(offsets) else len(bank)
            records.append(VoiceRecord(group_index, speaker_id, cue_id, variant, selector_class, duration_units, global_index, stream_offset, bank_end, category))
    if not records:
        raise ExtractionError("No encounter voices were mapped. / エンカウンター音声を特定できませんでした。")
    return bank, records


def analyze(path: Path) -> dict[str, object]:
    path = path.resolve()
    languages = discover_languages(path)
    result: dict[str, object] = {"archive": str(path), "archive_size": path.stat().st_size, "languages": []}
    summaries = result["languages"]
    assert isinstance(summaries, list)
    for language in languages:
        _, records = analyze_bank(path, language)
        counts = {category: sum(record.category == category for record in records) for category in CATEGORY_LABELS}
        summaries.append({"code": language.code, "name": LANGUAGES[language.code], "count": len(records), "categories": counts})
    return result


def _tool_path(explicit: Path | None = None) -> Path:
    if explicit:
        path = explicit.resolve()
    else:
        import sys
        base = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else Path(__file__).resolve().parents[1]
        path = base / "tools" / "vgmstream" / "vgmstream-cli.exe"
    if not path.is_file():
        raise ExtractionError(f"vgmstream-cli.exe was not found / 見つかりません: {path}")
    return path


def output_filename(record: VoiceRecord) -> str:
    """Return the language-neutral filename consumed by NativeFreeRoamRacers."""
    return (
        f"{record.category}_speaker{record.speaker_id:02d}_cue{record.cue_id:03d}_"
        f"v{record.variant_index:02d}_class{record.selector_class}_stream{record.stream_index:04d}.wav"
    )


def extract(
    archive: Path,
    output_parent: Path,
    language_codes: Iterable[str] | None = None,
    tool: Path | None = None,
    progress: Progress | None = None,
    cancelled: Callable[[], bool] | None = None,
) -> Path:
    archive = archive.resolve()
    output_parent = output_parent.resolve()
    selected = set(language_codes or [])
    banks = [item for item in discover_languages(archive) if not selected or item.code in selected]
    if not banks:
        raise ExtractionError("The selected language is not in this archive. / 選択言語がアーカイブ内にありません。")
    decoder = _tool_path(tool)
    analyses: list[tuple[LanguageBank, bytes, list[VoiceRecord]]] = []
    for item in banks:
        bank, records = analyze_bank(archive, item)
        analyses.append((item, bank, records))
    total = sum(len(records) for _, _, records in analyses)
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    run_root = output_parent / f"NFSU2_Encounter_Audio_{timestamp}"
    counter = 2
    while run_root.exists():
        run_root = output_parent / f"NFSU2_Encounter_Audio_{timestamp}_{counter}"
        counter += 1
    run_root.mkdir(parents=True)
    completed = 0
    manifest_rows: list[dict[str, object]] = []
    try:
        with tempfile.TemporaryDirectory(prefix="nfsu2_encounter_") as temp_name:
            temp = Path(temp_name)
            for language, bank, records in analyses:
                content_root = run_root if len(analyses) == 1 else run_root / language.code
                language_root = content_root / "NativeFreeRoamRacers" / "encounter"
                for category in CATEGORY_LABELS:
                    (language_root / category).mkdir(parents=True, exist_ok=True)
                for record in records:
                    if cancelled and cancelled():
                        raise CancelledError("Extraction cancelled. / 抽出をキャンセルしました。")
                    temp_name = (
                        f"{language.code}_g{record.group_index:02d}_speaker{record.speaker_id:02d}_"
                        f"cue{record.cue_id:03d}_v{record.variant_index:02d}_class{record.selector_class}_"
                        f"stream{record.stream_index:04d}"
                    )
                    source_stream = temp / (temp_name + ".asf")
                    source_stream.write_bytes(bank[record.bank_offset:record.bank_end])
                    target = language_root / record.category / output_filename(record)
                    process = subprocess.run(
                        [str(decoder), "-o", str(target), str(source_stream)],
                        cwd=decoder.parent,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                        encoding="utf-8",
                        errors="replace",
                        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
                    )
                    source_stream.unlink(missing_ok=True)
                    if process.returncode != 0 or not target.is_file():
                        raise ExtractionError(f"vgmstream failed for stream {record.stream_index}:\n{process.stdout[-1200:]}")
                    with wave.open(str(target), "rb") as wav:
                        channels, rate, frames = wav.getnchannels(), wav.getframerate(), wav.getnframes()
                    manifest_rows.append({
                        "language": language.code,
                        "category": record.category,
                        "group_index": record.group_index,
                        "speaker_id": record.speaker_id,
                        "cue_id": record.cue_id,
                        "variant_index": record.variant_index,
                        "selector_class": record.selector_class,
                        "duration_units": record.duration_units,
                        "stream_index": record.stream_index,
                        "bank_offset": f"0x{record.bank_offset:08X}",
                        "file": str(target.relative_to(run_root)).replace("\\", "/"),
                        "channels": channels,
                        "sample_rate": rate,
                        "frames": frames,
                        "sha256": sha256_file(target),
                    })
                    completed += 1
                    if progress:
                        progress(completed, total, f"{language.code}: {record.category} ({completed}/{total})")
        manifest_path = run_root / "manifest.csv"
        with manifest_path.open("w", newline="", encoding="utf-8-sig") as target:
            writer = csv.DictWriter(target, fieldnames=list(manifest_rows[0]))
            writer.writeheader()
            writer.writerows(manifest_rows)
        counts = {
            code: {category: sum(row["language"] == code and row["category"] == category for row in manifest_rows) for category in CATEGORY_LABELS}
            for code in sorted({str(row["language"]) for row in manifest_rows})
        }
        summary = {
            "application": "NFSU2 Encounter Audio Extractor",
            "version": "1.1.0",
            "source_file": str(archive),
            "source_size": archive.stat().st_size,
            "source_sha256": sha256_file(archive),
            "created_at": datetime.now().astimezone().isoformat(timespec="seconds"),
            "decoder": "vgmstream-cli",
            "languages": counts,
            "wav_count": len(manifest_rows),
            "classification_basis": {
                "start": [0, 7],
                "player_victory": [3, 4, 5],
                "player_defeat": [1, 2, 6],
                "idx_groups": [9, 10, 11, 12, 13],
            },
        }
        (run_root / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        return run_root
    except Exception:
        shutil.rmtree(run_root, ignore_errors=True)
        raise
