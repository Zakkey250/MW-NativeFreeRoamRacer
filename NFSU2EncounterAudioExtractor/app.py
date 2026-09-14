from __future__ import annotations

import argparse
import json
import queue
import sys
import threading
from pathlib import Path

from encounter_extractor.core import CancelledError, ExtractionError, LANGUAGES, analyze, extract


def console_print(message: str, *, error: bool = False) -> None:
    stream = sys.stderr if error else sys.stdout
    if stream is not None:
        print(message, file=stream, flush=True)


def run_cli(args: argparse.Namespace) -> int:
    source = Path(args.sdat)
    if args.analyze:
        console_print(json.dumps(analyze(source), ensure_ascii=False, indent=2))
        return 0
    codes = None if args.language == "ALL" else [args.language]
    destination = extract(
        source,
        Path(args.output),
        codes,
        progress=lambda done, total, message: console_print(f"[{done}/{total}] {message}"),
    )
    console_print(f"OUTPUT={destination}")
    return 0


def run_gui() -> int:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk

    class Application:
        def __init__(self, root: tk.Tk) -> None:
            self.root = root
            self.root.title("NFSU2 Encounter Audio Extractor")
            self.root.geometry("720x470")
            self.root.minsize(650, 430)
            self.events: queue.Queue[tuple[str, object]] = queue.Queue()
            self.cancel_event = threading.Event()
            self.languages: list[str] = []
            self.sdat = tk.StringVar()
            self.output = tk.StringVar(value=str(Path.home() / "Desktop"))
            self.language = tk.StringVar(value="AUTO")
            self.status = tk.StringVar(value="Select sdat.viv, then Analyze. / sdat.viv を選んで解析してください。")
            self.progress = tk.DoubleVar(value=0)
            self._build(ttk)
            self.root.after(100, self._poll)

        def _build(self, ttk_module) -> None:
            frame = ttk_module.Frame(self.root, padding=16)
            frame.pack(fill="both", expand=True)
            ttk_module.Label(frame, text="NFSU2 Encounter Audio Extractor", font=("Segoe UI", 16, "bold")).pack(anchor="w")
            ttk_module.Label(frame, text="Extract encounter start, victory and defeat calls / 開始・勝利・敗北の通話音声を抽出", wraplength=670).pack(anchor="w", pady=(2, 16))

            ttk_module.Label(frame, text="Source sdat.viv / 元の sdat.viv").pack(anchor="w")
            row = ttk_module.Frame(frame)
            row.pack(fill="x", pady=(3, 10))
            ttk_module.Entry(row, textvariable=self.sdat).pack(side="left", fill="x", expand=True)
            ttk_module.Button(row, text="Browse / 参照", command=self._browse_sdat).pack(side="left", padx=(8, 0))

            ttk_module.Label(frame, text="Output folder / 出力先フォルダー").pack(anchor="w")
            row = ttk_module.Frame(frame)
            row.pack(fill="x", pady=(3, 10))
            ttk_module.Entry(row, textvariable=self.output).pack(side="left", fill="x", expand=True)
            ttk_module.Button(row, text="Browse / 参照", command=self._browse_output).pack(side="left", padx=(8, 0))

            ttk_module.Label(frame, text="Language bank / 言語バンク").pack(anchor="w")
            self.language_box = ttk_module.Combobox(frame, textvariable=self.language, state="readonly", values=["AUTO"])
            self.language_box.pack(fill="x", pady=(3, 10))

            actions = ttk_module.Frame(frame)
            actions.pack(fill="x", pady=(2, 10))
            self.analyze_button = ttk_module.Button(actions, text="Analyze / 解析", command=self._start_analyze)
            self.analyze_button.pack(side="left")
            self.extract_button = ttk_module.Button(actions, text="Extract WAV / WAV抽出", command=self._start_extract, state="disabled")
            self.extract_button.pack(side="left", padx=8)
            self.cancel_button = ttk_module.Button(actions, text="Cancel / 中止", command=self.cancel_event.set, state="disabled")
            self.cancel_button.pack(side="left")

            ttk_module.Progressbar(frame, variable=self.progress, maximum=100).pack(fill="x")
            ttk_module.Label(frame, textvariable=self.status, wraplength=670).pack(anchor="w", pady=(8, 8))
            self.log = tk.Text(frame, height=8, state="disabled", wrap="word", font=("Consolas", 9))
            self.log.pack(fill="both", expand=True)

        def _browse_sdat(self) -> None:
            value = filedialog.askopenfilename(title="Select sdat.viv / sdat.viv を選択", filetypes=[("sdat.viv", "sdat.viv"), ("VIV files", "*.viv"), ("All files", "*.*")])
            if value:
                self.sdat.set(value)

        def _browse_output(self) -> None:
            value = filedialog.askdirectory(title="Output folder / 出力先")
            if value:
                self.output.set(value)

        def _set_busy(self, busy: bool) -> None:
            self.analyze_button.configure(state="disabled" if busy else "normal")
            self.extract_button.configure(state="disabled" if busy or not self.languages else "normal")
            self.cancel_button.configure(state="normal" if busy else "disabled")

        def _worker(self, fn) -> None:
            try:
                fn()
            except Exception as error:
                self.events.put(("error", error))
            finally:
                self.events.put(("idle", None))

        def _start_analyze(self) -> None:
            source = Path(self.sdat.get().strip())
            if not source.is_file():
                messagebox.showerror("Error / エラー", "Select a valid sdat.viv. / 有効な sdat.viv を選択してください。")
                return
            self.cancel_event.clear()
            self._set_busy(True)
            self.status.set("Analyzing... / 解析中...")
            threading.Thread(target=self._worker, args=(lambda: self.events.put(("analysis", analyze(source))),), daemon=True).start()

        def _start_extract(self) -> None:
            source, destination = Path(self.sdat.get().strip()), Path(self.output.get().strip())
            if not source.is_file() or not destination.is_dir():
                messagebox.showerror("Error / エラー", "Check the source and output folders. / 入力と出力先を確認してください。")
                return
            selection = self.language.get().split(" ", 1)[0]
            codes = None if selection == "ALL" else [selection]
            self.cancel_event.clear()
            self.progress.set(0)
            self._set_busy(True)
            self.status.set("Extracting... / 抽出中...")

            def work() -> None:
                result = extract(source, destination, codes, progress=lambda d, t, m: self.events.put(("progress", (d, t, m))), cancelled=self.cancel_event.is_set)
                self.events.put(("complete", result))
            threading.Thread(target=self._worker, args=(work,), daemon=True).start()

        def _append(self, text: str) -> None:
            self.log.configure(state="normal")
            self.log.insert("end", text + "\n")
            self.log.see("end")
            self.log.configure(state="disabled")

        def _poll(self) -> None:
            try:
                while True:
                    kind, value = self.events.get_nowait()
                    if kind == "idle":
                        self._set_busy(False)
                    elif kind == "analysis":
                        info = value
                        self.languages = [item["code"] for item in info["languages"]]
                        values = (["ALL - All detected / 検出した全言語"] if len(self.languages) > 1 else []) + [f"{code} - {LANGUAGES[code]}" for code in self.languages]
                        self.language_box.configure(values=values)
                        self.language.set(values[0])
                        details = ", ".join(f"{item['code']}: {item['count']}" for item in info["languages"])
                        self.status.set(f"Ready / 準備完了 — {details}")
                        self._append(f"Detected / 検出: {details}")
                    elif kind == "progress":
                        done, total, message = value
                        self.progress.set(done * 100 / total)
                        self.status.set(str(message))
                    elif kind == "complete":
                        path = Path(value)
                        self.progress.set(100)
                        self.status.set(f"Complete / 完了: {path}")
                        self._append(f"Complete / 完了: {path}")
                        messagebox.showinfo("Complete / 完了", f"Extraction completed. / 抽出が完了しました。\n\n{path}")
                    elif kind == "error":
                        error = value
                        self.status.set(str(error))
                        self._append(f"ERROR: {error}")
                        if not isinstance(error, CancelledError):
                            messagebox.showerror("Error / エラー", str(error))
            except queue.Empty:
                pass
            self.root.after(100, self._poll)

    root = tk.Tk()
    Application(root)
    root.mainloop()
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract NFSU2 encounter call audio from sdat.viv")
    parser.add_argument("--sdat")
    parser.add_argument("--output")
    parser.add_argument("--language", choices=["ALL", *LANGUAGES], default="ALL")
    parser.add_argument("--analyze", action="store_true")
    args = parser.parse_args()
    if args.sdat:
        if not args.analyze and not args.output:
            parser.error("--output is required unless --analyze is used")
        try:
            return run_cli(args)
        except (ExtractionError, OSError) as error:
            console_print(f"ERROR: {error}", error=True)
            return 1
    return run_gui()


if __name__ == "__main__":
    raise SystemExit(main())
