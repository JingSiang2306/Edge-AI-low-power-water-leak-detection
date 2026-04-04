"""
DL_Logger_v2.4.py — STM32 UART Data Logger (Late Fusion + YAMNet Export)

What this script does:
- Connects to the STM32 over a selected COM port and listens for UART markers:
    ---BEGIN_SAMPLE---
    ---BEGIN_AUDIO_CSV--- / ---END_AUDIO_CSV---
    ---BEGIN_VIB_CSV---   / ---END_VIB_CSV---
- For each recording, saves the raw data sequentially into logged_data:
    <base>_NNN_acoustic.csv   (1 column, one audio sample per line @ 16 kHz)
    <base>_NNN_vibration.csv  (1 column, one vibration sample per line @ ~6.6 kHz)

Dataset outputs (automatic):
1) Late-fusion buffer CSVs (for NanoEdge AI training style):
   - Converts raw CSV into 512-sample windows and appends into:
       dataset_latefusion/acoustic/<label>/<base>_XXX_acoustic.csv
       dataset_latefusion/vibration/<label>/<base>_YYY_vibration.csv

2) YAMNet-friendly dataset export:
   - Converts the raw audio CSV into a 16 kHz mono WAV:
       dataset_YAMnet/acoustic/<label>/<base>_NNN.wav
   - Copies the raw vibration CSV (1-column) into:
       dataset_YAMnet/vibration/<label>/<base>_NNN.csv

Viewer features:
- Displays the latest raw acoustic/vibration CSV paths + WAV path.
- Lets you browse recordings using < / > navigation (updates all three fields together).
- Plots acoustic and vibration in separate axes using time (seconds) on the x-axis.
  Plot range is controlled by Start time (s) and End time (s), defaulting to the full file length.
- Plays the selected WAV via the OS default audio player.

Safety/Usability:
- During capture, key UI inputs are locked to prevent mid-run changes.
- Stop button is disabled when capture is not running.
"""

import os
import re
import queue
import threading
import time
from datetime import datetime
from pathlib import Path
import csv
import sys
import subprocess
import json
import traceback
import shutil
import wave

import numpy as np
import serial
from serial.tools import list_ports

import tkinter as tk
from tkinter import ttk, filedialog, messagebox

from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk


# -----------------------------------------------------------------------------
# Paths / settings
# -----------------------------------------------------------------------------

SCRIPT_DIR = Path(__file__).resolve().parent
SETTINGS_FILE = SCRIPT_DIR / "DL_Logger_settings.json"
LOG_FILE = SCRIPT_DIR / "DL_Logger_log.txt"


def ensure_dir(path: Path) -> Path:
    path.mkdir(parents=True, exist_ok=True)
    return path


def load_settings() -> dict:
    try:
        if SETTINGS_FILE.is_file():
            with SETTINGS_FILE.open("r", encoding="utf-8") as f:
                d = json.load(f)
                return d if isinstance(d, dict) else {}
    except Exception:
        pass
    return {}


def save_settings(d: dict) -> None:
    try:
        with SETTINGS_FILE.open("w", encoding="utf-8") as f:
            json.dump(d, f, indent=2)
    except Exception:
        pass


SETTINGS = load_settings()

# -----------------------------------------------------------------------------
# Config
# -----------------------------------------------------------------------------

ACOUSTIC_SAMPLE_RATE_HZ = 16000.0
VIB_SAMPLE_RATE_HZ = 6667.0
# Accept vibration sample count within a range around expected.
VIB_ACCEPT_MIN_RATIO = 0.95
VIB_ACCEPT_MAX_RATIO = 1.05

LATEFUSION_BUFFER_LEN = 512
LATEFUSION_MAX_ROWS = 10000

LATEFUSION_DIRNAME = "dataset_latefusion"
YAMNET_DIRNAME = "dataset_YAMnet"

BEGIN_SAMPLE = "---BEGIN_SAMPLE---"
BEGIN_AUDIO = "---BEGIN_AUDIO_CSV---"
END_AUDIO = "---END_AUDIO_CSV---"
BEGIN_VIB = "---BEGIN_VIB_CSV---"
END_VIB = "---END_VIB_CSV---"

FIRMWARE_IGNORE_PREFIXES = ["[BTN", "[LOG", "[NEAI]", "[TIME", "[VIEW", "[UI", "[INIT", "[WARN", "[ERR"]

# -----------------------------------------------------------------------------
# Windows sound notification
# -----------------------------------------------------------------------------

try:
    import winsound
    _HAS_WINSOUND = True
except Exception:
    _HAS_WINSOUND = False

def notify_sound(kind: str = "done"):
    """
    kind: "dataset", "detect", "all_done", "error"
    Plays a short Windows system sound (async).
    Falls back to terminal bell if winsound isn't available.
    """
    if _HAS_WINSOUND:
        # Different sound types
        if kind == "error":
            winsound.MessageBeep(winsound.MB_ICONHAND)
        elif kind == "all_done":
            winsound.MessageBeep(winsound.MB_ICONEXCLAMATION)
        else:
            # dataset/detect/done
            winsound.MessageBeep(winsound.MB_ICONASTERISK)
    else:
        # Fallback
        sys.stdout.write("\a")
        sys.stdout.flush()

# -----------------------------------------------------------------------------
# CSV helpers
# -----------------------------------------------------------------------------

def read_1col_csv_as_floats(path: Path) -> list[float]:
    vals: list[float] = []
    with path.open("r", newline="") as f:
        for line in f:
            s = line.strip()
            if not s:
                continue
            try:
                vals.append(float(s))
            except ValueError:
                continue
    return vals


def read_1col_csv_as_int16(path: Path) -> np.ndarray:
    xs = read_1col_csv_as_floats(path)
    if not xs:
        return np.zeros((0,), dtype=np.int16)
    arr = np.asarray(xs, dtype=np.float64)
    arr = np.clip(np.rint(arr), -32768, 32767).astype(np.int16)
    return arr


def make_latefusion_buffer_csv(raw_path: Path, out_path: Path, buffer_len: int, log_fn):
    samples = read_1col_csv_as_floats(raw_path)
    if not samples:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text("", encoding="utf-8")
        log_fn(f"[BUF ] SKIP empty: {raw_path.name}")
        return

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", newline="") as f:
        w = csv.writer(f)
        idx = 0
        nwin = 0
        while idx + buffer_len <= len(samples):
            w.writerow([f"{v:.6f}" for v in samples[idx: idx + buffer_len]])
            idx += buffer_len
            nwin += 1

    log_fn(f"[LOG ] BUFFER → wrote {nwin} windows × {buffer_len} samples to {out_path.name}")


def append_buffer_to_dataset(buf_path: Path, dataset_root: Path, modality: str, label: str, base: str, log_fn):
    assert modality in ("acoustic", "vibration")
    out_dir = ensure_dir(dataset_root / modality / label)

    pat = re.compile(rf"^{re.escape(base)}_(\d{{3}})_{modality}\.csv$")
    existing = []
    for p in out_dir.glob(f"{base}_*_{modality}.csv"):
        m = pat.match(p.name)
        if m:
            existing.append((int(m.group(1)), p))
    existing.sort(key=lambda x: x[0])

    if existing:
        idx, out_path = existing[-1]
    else:
        idx, out_path = 1, out_dir / f"{base}_{1:03d}_{modality}.csv"

    current_rows = 0
    if out_path.exists():
        with out_path.open("r", newline="") as f:
            current_rows = sum(1 for _ in f)

    with buf_path.open("r", newline="") as f:
        rows = list(csv.reader(f))

    if not rows:
        log_fn(f"[DATA] SKIP append empty buffer: {buf_path.name}")
        return out_path

    if current_rows + len(rows) > LATEFUSION_MAX_ROWS:
        idx += 1
        out_path = out_dir / f"{base}_{idx:03d}_{modality}.csv"

    with out_path.open("a", newline="") as f:
        w = csv.writer(f)
        for r in rows:
            w.writerow(r)

    return out_path


def write_wav_16k_mono_from_audio_csv(audio_raw_csv: Path, wav_out: Path, log_fn):
    pcm = read_1col_csv_as_int16(audio_raw_csv)
    if pcm.size == 0:
        raise RuntimeError("audio raw CSV empty")

    wav_out.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(wav_out), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(int(ACOUSTIC_SAMPLE_RATE_HZ))
        wf.writeframes(pcm.tobytes())

    log_fn(f"[YAM ] WAV    → \n\t{wav_out}")


def copy_vib_raw_csv_to_yamnet(vib_raw_csv: Path, vib_out: Path, log_fn):
    vib_out.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(vib_raw_csv, vib_out)
    log_fn(f"[YAM ] VIBCSV → \n\t{vib_out}")


def find_next_record_index(outdir: Path, base: str) -> int:
    base = base.strip().lower() or "test"
    pat = re.compile(rf"^{re.escape(base)}_(\d{{3}})_acoustic\.csv$")
    nums = []
    for p in outdir.glob(f"{base}_*_acoustic.csv"):
        m = pat.match(p.name)
        if m:
            nums.append(int(m.group(1)))
    return (max(nums) + 1) if nums else 1


# -----------------------------------------------------------------------------
# Capture Worker
# -----------------------------------------------------------------------------

class CaptureWorker(threading.Thread):
    def __init__(self, app, port: str, baud: int, outdir: Path, dataset_dir: Path, label: str, base: str, echo: bool):
        super().__init__(daemon=True)
        self.app = app
        self.port = port
        self.baud = baud
        self.outdir = outdir
        self.dataset_dir = dataset_dir
        self.label = (label.strip().lower() or "unlabeled")
        self.base = (base.strip().lower() or "test")
        self.echo = echo

        self.stop_event = threading.Event()
        self.ser: serial.Serial | None = None

        self.latefusion_root = self.dataset_dir / LATEFUSION_DIRNAME
        self.yamnet_root = self.dataset_dir / YAMNET_DIRNAME

        self.mode = "idle"
        self.csv_kind: str | None = None
        self.csv_lines: list[str] = []

        self.record_idx = find_next_record_index(self.outdir, self.base) - 1
        self.pending_audio: Path | None = None
        self.pending_vib: Path | None = None

        self.expected_audio_samples: int | None = None
        self.expected_vib_samples: int | None = None
        self.last_audio_samples: int | None = None
        self.last_vib_samples: int | None = None

        # --- Timing stats (monotonic seconds) ---
        self._ds_active = False
        self._ds_start_t: float | None = None
        self._ds_audio_start_t: float | None = None
        self._ds_audio_end_t: float | None = None
        self._ds_vib_start_t: float | None = None
        self._ds_vib_end_t: float | None = None

        self._all_active = False
        self._all_start_t: float | None = None
        self._all_audio_total_s: float = 0.0
        self._all_vib_total_s: float = 0.0
        self._all_delay_total_s: float = 0.0

        self._delay_active = False
        self._delay_start_t: float | None = None

        self._det_active = False
        self._det_start_t: float | None = None
        self._det_record_end_t: float | None = None

    def request_stop(self):
        self.stop_event.set()

    def log(self, msg: str):
        self.app.enqueue_log(msg)

    @staticmethod
    def _fmt_s(seconds: float | None) -> str:
        if seconds is None:
            return "N/A"
        return f"{seconds:.2f}s"

    @staticmethod
    def _dur(start_t: float | None, end_t: float | None) -> float | None:
        if start_t is None or end_t is None:
            return None
        return max(0.0, end_t - start_t)

    def _open_serial(self):
        self.ser = serial.Serial(self.port, self.baud, timeout=0.2)
        self.ser.reset_input_buffer()

    def _close_serial(self):
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
        self.ser = None

    def _raw_path_for(self, kind: str) -> Path:
        suffix = "acoustic" if kind == "audio" else "vibration"
        return self.outdir / f"{self.base}_{self.record_idx:03d}_{suffix}.csv"

    def _yam_wav_path_for(self) -> Path:
        return self.yamnet_root / "acoustic" / self.label / f"{self.base}_{self.record_idx:03d}.wav"

    def _yam_vib_raw_path_for(self) -> Path:
        return self.yamnet_root / "vibration" / self.label / f"{self.base}_{self.record_idx:03d}.csv"

    def _register_pair_if_ready(self):
        if self.pending_audio is None or self.pending_vib is None:
            return

        if (self.expected_audio_samples is not None and self.last_audio_samples is not None
                and self.last_audio_samples < self.expected_audio_samples):
            self.log(f"[DATA] SKIP record: audio samples too few: {self.last_audio_samples} < {self.expected_audio_samples}")
            self.pending_audio = None
            self.pending_vib = None
            return

        if self.expected_vib_samples is not None and self.last_vib_samples is not None:
            vib_min = int(self.expected_vib_samples * VIB_ACCEPT_MIN_RATIO)
            vib_max = int(self.expected_vib_samples * VIB_ACCEPT_MAX_RATIO)

            if not (vib_min <= self.last_vib_samples <= vib_max):
                self.log(f"[DATA] SKIP record: vibration samples out of range: "
                        f"{self.last_vib_samples} not in [{vib_min}, {vib_max}] "
                        f"(expected {self.expected_vib_samples})")
                self.pending_audio = None
                self.pending_vib = None
                return

        try:
            audio_raw = self.pending_audio
            vib_raw = self.pending_vib

            ac_buf = self.outdir / f"{self.base}_{self.record_idx:03d}_512buffer_acoustic.csv"
            vb_buf = self.outdir / f"{self.base}_{self.record_idx:03d}_512buffer_vibration.csv"

            make_latefusion_buffer_csv(audio_raw, ac_buf, LATEFUSION_BUFFER_LEN, self.log)
            make_latefusion_buffer_csv(vib_raw, vb_buf, LATEFUSION_BUFFER_LEN, self.log)

            lf_root = ensure_dir(self.latefusion_root)
            ac_dataset_path = append_buffer_to_dataset(ac_buf, lf_root, "acoustic", self.label, self.base, self.log)
            vb_dataset_path = append_buffer_to_dataset(vb_buf, lf_root, "vibration", self.label, self.base, self.log)

            wav_path = self._yam_wav_path_for()
            vib_yam_raw = self._yam_vib_raw_path_for()
            write_wav_16k_mono_from_audio_csv(audio_raw, wav_path, self.log)
            copy_vib_raw_csv_to_yamnet(vib_raw, vib_yam_raw, self.log)

            self.log(
                "[DATA] OUTPUTS → "
                f"\n\tlabel           = {self.label}"
                f"\n\tbase            = {self.base}"
                f"\n\trecord_idx      = {self.record_idx:03d}"
                f"\n\taudio_raw       = {audio_raw}"
                f"\n\tvib_raw         = {vib_raw}"
                f"\n\tlatefusion_ac   = {ac_dataset_path}"
                f"\n\tlatefusion_vib  = {vb_dataset_path}"
                f"\n\tyamnet_wav      = {wav_path}"
                f"\n\tyamnet_vib_csv  = {vib_yam_raw}"
            )

            self.app.update_viewer_fields(audio_raw, vib_raw, wav_path)

        except Exception as e:
            self.log(f"[ERR ] dataset build failed: {e}")
            self.log(traceback.format_exc())
        finally:
            self.pending_audio = None
            self.pending_vib = None

    def run(self):
        try:
            ensure_dir(self.outdir)
            ensure_dir(self.dataset_dir)
            ensure_dir(self.latefusion_root)
            ensure_dir(self.yamnet_root)
            self._open_serial()
        except Exception as e:
            self.log(f"[ERR ] startup failed: {e}")
            # self.log(traceback.format_exc())
            self.app.on_worker_finished()
            return

        self.log(
            "[INFO] Waiting for STM32 markers…"
            f"\n[INFO] logged_data   = {self.outdir}"
            f"\n[INFO] dataset_dir   = {self.dataset_dir}"
            f"\n[INFO] label         = {self.label}"
            f"\n[INFO] base          = {self.base}"
        )

        try:
            while not self.stop_event.is_set():
                b = self.ser.readline()
                if not b:
                    continue
                line = b.decode("utf-8", errors="replace").rstrip("\r\n")

                if self.echo or line.startswith("[") : #or line.startswith("---")
                    self.log(line)

                # -----------------------------------------------------------------
                # Timing hooks (dataset + detection)
                # -----------------------------------------------------------------
                now = time.monotonic()

                # Intermediate delay between datasets (optional firmware feature)
                if line.startswith("[BTN ] Intermediate delay started"):
                    # Close any previous delay (shouldn't happen, but be safe)
                    if self._delay_active and self._delay_start_t is not None:
                        self._all_delay_total_s += (now - self._delay_start_t)
                    self._delay_active = True
                    self._delay_start_t = now

                if any(line.startswith(p) for p in (
                    "[BTN ] Intermediate delay ended",
                    "[BTN ] Intermediate delay done",
                    "[BTN ] Intermediate delay completed",
                )):
                    if self._delay_active and self._delay_start_t is not None:
                        self._all_delay_total_s += (now - self._delay_start_t)
                    self._delay_active = False
                    self._delay_start_t = None

                # Dataset start (per-dataset and "all datasets" timers)
                if line.startswith("[BTN ] Logging") and " now" in line:
                    # If a delay was active, end it at the moment the next dataset starts
                    if self._delay_active and self._delay_start_t is not None:
                        self._all_delay_total_s += (now - self._delay_start_t)
                        self._delay_active = False
                        self._delay_start_t = None

                    # Start "all datasets" timer on the first dataset
                    if not self._all_active:
                        self._all_active = True
                        self._all_start_t = now
                        self._all_audio_total_s = 0.0
                        self._all_vib_total_s = 0.0
                        self._all_delay_total_s = 0.0

                    # Start per-dataset timer
                    self._ds_active = True
                    self._ds_start_t = now
                    self._ds_audio_start_t = None
                    self._ds_audio_end_t = None
                    self._ds_vib_start_t = None
                    self._ds_vib_end_t = None

                # Dataset completion (print per-dataset timing)
                if line.startswith("[BTN ] Logging completed"):
                    ds_total = self._dur(self._ds_start_t, now)
                    ds_audio = self._dur(self._ds_audio_start_t, self._ds_audio_end_t)
                    ds_vib = self._dur(self._ds_vib_start_t, self._ds_vib_end_t)

                    if ds_audio is not None:
                        self._all_audio_total_s += ds_audio
                    if ds_vib is not None:
                        self._all_vib_total_s += ds_vib

                    self.log(
                        "[TIME] Acoustic   : " + self._fmt_s(ds_audio) +
                        "\n       Vibration  : " + self._fmt_s(ds_vib) +
                        "\n       Total time : " + self._fmt_s(ds_total)
                    )

                    self._ds_active = False
                    self._ds_start_t = None

                # All datasets completion (print totals)
                if line.startswith("[BTN ] All Logging completed"):
                    # If delay is still active, close it here
                    if self._delay_active and self._delay_start_t is not None:
                        self._all_delay_total_s += (now - self._delay_start_t)
                        self._delay_active = False
                        self._delay_start_t = None

                    all_total = self._dur(self._all_start_t, now)

                    m = re.search(r"Total\s*=\s*(\d+)\s*datasets", line)
                    n_datasets = int(m.group(1)) if m else 0
                    audio_avg_s = (self._all_audio_total_s / n_datasets) if n_datasets > 0 else 0.0
                    vib_avg_s = (self._all_vib_total_s / n_datasets) if n_datasets > 0 else 0.0
                    delay_avg_s = (self._all_delay_total_s / n_datasets) if n_datasets > 0 else 0.0
                    total_avg_s = (all_total / n_datasets) if n_datasets > 0 else 0.0

                    self.log(
                        "[TIME] Total Acoustic : " + self._fmt_s(self._all_audio_total_s) + "\t(Avg: " + self._fmt_s(audio_avg_s) + ")" +
                        "\n       Total Vibration: " + self._fmt_s(self._all_vib_total_s) + "\t(Avg: " + self._fmt_s(vib_avg_s) + ")" +
                        "\n       Total delay    : " + self._fmt_s(self._all_delay_total_s) + "\t (Avg: " + self._fmt_s(delay_avg_s) + ")" +
                        "\n       Total time     : " + self._fmt_s(all_total) + "\t(Avg: " + self._fmt_s(total_avg_s) + ")"
                    )

                    notify_sound("all_done")

                    self._all_active = False
                    self._all_start_t = None

                # Detection timing (recording + inference)
                if line.startswith("[BTN ] Recording") and ("for detection now" in line):
                    self._det_active = True
                    self._det_start_t = now
                    self._det_record_end_t = None

                if self._det_active and self._det_start_t is not None and self._det_record_end_t is None:
                    if line.startswith("[BTN ] Acoustic done") or line.startswith("[BTN ] Capture done"):
                        self._det_record_end_t = now

                if line.startswith("[BTN ] NEAI sliding detection"):
                    det_total = self._dur(self._det_start_t, now)
                    det_record = self._dur(self._det_start_t, self._det_record_end_t)
                    det_infer = self._dur(self._det_record_end_t, now) if self._det_record_end_t is not None else None

                    self.log(
                        "[TIME] Recording  : " + self._fmt_s(det_record) +
                        "\n       Inference  : " + self._fmt_s(det_infer) +
                        "\n       Total time : " + self._fmt_s(det_total)
                    )

                    notify_sound("detect")

                    self._det_active = False
                    self._det_start_t = None
                    self._det_record_end_t = None

                # -----------------------------------------------------------------
                # Existing parsing logic
                # -----------------------------------------------------------------
                if line.startswith("[BTN ] Logging"):
                    m = re.search(r"Logging\s+(\d+)\s*s", line)
                    if m:
                        secs = int(m.group(1))
                        self.expected_audio_samples = int(ACOUSTIC_SAMPLE_RATE_HZ * secs)
                        self.expected_vib_samples = int(VIB_SAMPLE_RATE_HZ * secs)
                    else:
                        self.expected_audio_samples = None
                        self.expected_vib_samples = None
                    self.last_audio_samples = None
                    self.last_vib_samples = None

                if line.strip() == BEGIN_SAMPLE:
                    self.record_idx += 1
                    self.pending_audio = None
                    self.pending_vib = None
                    self.mode = "idle"
                    continue

                if self.mode == "idle":
                    if line.strip() == BEGIN_AUDIO:
                        self.csv_kind = "audio"
                        self.csv_lines = []
                        self.mode = "csv"
                        self.log(f"[LOG ] BEGIN  → \n\t{self._raw_path_for('audio')}")

                        # Timing: open audio sub-timer for this dataset
                        if self._ds_active and self._ds_audio_start_t is None:
                            self._ds_audio_start_t = now
                        continue

                    if line.strip() == BEGIN_VIB:
                        self.csv_kind = "vib"
                        self.csv_lines = []
                        self.mode = "csv"
                        self.log(f"[LOG ] BEGIN  → \n\t{self._raw_path_for('vib')}")

                        # Timing: open vibration sub-timer for this dataset
                        if self._ds_active and self._ds_vib_start_t is None:
                            self._ds_vib_start_t = now
                        continue

                if self.mode == "csv":
                    if (self.csv_kind == "audio" and line.strip() == END_AUDIO) or (self.csv_kind == "vib" and line.strip() == END_VIB):
                        outpath = self._raw_path_for(self.csv_kind)
                        with outpath.open("w", newline="") as f:
                            for s in self.csv_lines:
                                f.write(s + "\n")

                        n = len(self.csv_lines)
                        self.log(f"[LOG ] END    → \n\twrote {n} lines to\n\t{outpath}")

                        # Timing: close audio/vibration sub-timers for this dataset
                        if self._ds_active:
                            if self.csv_kind == "audio" and self._ds_audio_end_t is None:
                                self._ds_audio_end_t = now
                            if self.csv_kind == "vib" and self._ds_vib_end_t is None:
                                self._ds_vib_end_t = now

                        if self.csv_kind == "audio":
                            self.last_audio_samples = n
                            self.pending_audio = outpath
                        else:
                            self.last_vib_samples = n
                            self.pending_vib = outpath

                        self.mode = "idle"
                        self.csv_kind = None
                        self.csv_lines = []
                        self._register_pair_if_ready()
                        continue

                    if any(line.startswith(p) for p in FIRMWARE_IGNORE_PREFIXES):
                        continue
                    if line.startswith("---BEGIN_") or line.startswith("---END_") or line.startswith("---"):
                        continue

                    s = line.strip()
                    if s:
                        self.csv_lines.append(s)
                    continue

        except Exception as e:
            self.log(f"[ERR ] runtime error: {e}")
            self.log(traceback.format_exc())
        finally:
            self._close_serial()
            self.log("[INFO] Capture stopped.")
            self.app.on_worker_finished()


# -----------------------------------------------------------------------------
# UI App
# -----------------------------------------------------------------------------

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("DL Logger v9 (Late Fusion + YAMNet Export)")
        self.geometry("1200x820")
        self.minsize(1100, 740)

        self.log_queue: queue.Queue[str] = queue.Queue()
        self.worker: CaptureWorker | None = None
        self.viewer_visible = True

        self._build_ui()
        self.after(100, self._drain_logs)

        # initial states
        self.btn_stop.configure(state="disabled")

    # ---------------- UI build ----------------
    def _build_ui(self):
        self.frm_top = ttk.Frame(self)
        self.frm_top.pack(side=tk.TOP, fill=tk.X, padx=10, pady=8)

        ttk.Label(self.frm_top, text="COM Port:").grid(row=0, column=0, sticky="w")
        self.port_var = tk.StringVar(value=SETTINGS.get("port", ""))
        self.port_combo = ttk.Combobox(self.frm_top, textvariable=self.port_var, width=16, state="readonly")
        self.port_combo.grid(row=0, column=1, sticky="w", padx=6)

        self.btn_refresh = ttk.Button(self.frm_top, text="Refresh", command=self.refresh_ports)
        self.btn_refresh.grid(row=0, column=2, sticky="w", padx=6)

        ttk.Label(self.frm_top, text="Baud:").grid(row=0, column=3, sticky="w")
        self.baud_var = tk.IntVar(value=int(SETTINGS.get("baud", 921600)))
        self.ent_baud = ttk.Entry(self.frm_top, textvariable=self.baud_var, width=10)
        self.ent_baud.grid(row=0, column=4, sticky="w", padx=6)

        ttk.Label(self.frm_top, text="Label:").grid(row=0, column=5, sticky="w")
        self.label_var = tk.StringVar(value=SETTINGS.get("label", "testing"))
        self.ent_label = ttk.Entry(self.frm_top, textvariable=self.label_var, width=14)
        self.ent_label.grid(row=0, column=6, sticky="w", padx=6)

        ttk.Label(self.frm_top, text="Base:").grid(row=0, column=7, sticky="w")
        self.base_var = tk.StringVar(value=SETTINGS.get("base", "test"))
        # (Req #3) make Base entry closer to label: reduce padx, slightly smaller width
        self.ent_base = ttk.Entry(self.frm_top, textvariable=self.base_var, width=12)
        self.ent_base.grid(row=0, column=8, sticky="w", padx=2)

        ttk.Label(self.frm_top, text="logged_data dir:").grid(row=1, column=0, sticky="w", pady=(6, 0))
        self.outdir_var = tk.StringVar(value=SETTINGS.get("outdir", str(SCRIPT_DIR / "logged_data")))
        self.ent_outdir = ttk.Entry(self.frm_top, textvariable=self.outdir_var, width=80)
        self.ent_outdir.grid(row=1, column=1, columnspan=7, sticky="we", padx=6, pady=(6, 0))
        self.btn_browse_out = ttk.Button(self.frm_top, text="Browse", command=self.browse_outdir)
        self.btn_browse_out.grid(row=1, column=8, sticky="e", padx=6, pady=(6, 0))

        ttk.Label(self.frm_top, text="Dataset dir:").grid(row=2, column=0, sticky="w", pady=(6, 0))
        self.datasetdir_var = tk.StringVar(value=SETTINGS.get("datasetdir", str(SCRIPT_DIR / "dataset_dir")))
        self.ent_dataset = ttk.Entry(self.frm_top, textvariable=self.datasetdir_var, width=80)
        self.ent_dataset.grid(row=2, column=1, columnspan=7, sticky="we", padx=6, pady=(6, 0))
        self.btn_browse_dataset = ttk.Button(self.frm_top, text="Browse", command=self.browse_datasetdir)
        self.btn_browse_dataset.grid(row=2, column=8, sticky="e", padx=6, pady=(6, 0))

        self.echo_var = tk.BooleanVar(value=bool(SETTINGS.get("echo", False)))
        self.chk_echo = ttk.Checkbutton(self.frm_top, text="Echo all UART lines", variable=self.echo_var)
        self.chk_echo.grid(row=3, column=1, sticky="w", pady=(6, 0))

        # (Req #2) Move buttons one column LEFT: from col=7.. to col=6..
        frm_btn = ttk.Frame(self.frm_top)
        frm_btn.grid(row=3, column=6, columnspan=3, sticky="e", pady=(6, 0))

        self.btn_start = ttk.Button(frm_btn, text="Start", command=self.on_start)
        self.btn_start.pack(side=tk.LEFT, padx=6)

        self.btn_stop = ttk.Button(frm_btn, text="Stop", command=self.on_stop)
        self.btn_stop.pack(side=tk.LEFT, padx=6)

        self.btn_clear = ttk.Button(frm_btn, text="Clear", command=self.on_clear_terminal)
        self.btn_clear.pack(side=tk.LEFT, padx=6)

        self.btn_toggle_viewer = ttk.Button(frm_btn, text="Toggle Viewer", command=self.toggle_viewer)
        self.btn_toggle_viewer.pack(side=tk.LEFT, padx=6)

        self.frm_top.grid_columnconfigure(1, weight=1)

        # ---------------- Paned layout ----------------
        self.paned = ttk.PanedWindow(self, orient=tk.VERTICAL)
        self.paned.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=10, pady=8)

        # Terminal frame
        self.frm_log = ttk.LabelFrame(self.paned, text="Terminal")
        self.txt = tk.Text(self.frm_log, height=18, wrap=tk.WORD)
        self.txt.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scr = ttk.Scrollbar(self.frm_log, command=self.txt.yview)
        scr.pack(side=tk.RIGHT, fill=tk.Y)
        self.txt.config(yscrollcommand=scr.set)

        # Viewer frame
        self.frm_view = ttk.LabelFrame(self.paned, text="Viewer (raw CSV + WAV)")
        topv = ttk.Frame(self.frm_view)
        topv.pack(side=tk.TOP, fill=tk.X, padx=8, pady=6)

        ttk.Label(topv, text="Acoustic (csv):").grid(row=0, column=0, sticky="w")
        self.view_ac_csv = tk.StringVar()
        self.ent_view_ac = ttk.Entry(topv, textvariable=self.view_ac_csv, width=70)
        self.ent_view_ac.grid(row=0, column=1, sticky="we", padx=6)
        self.btn_browse_ac = ttk.Button(topv, text="Browse", command=self.browse_view_ac_csv)
        self.btn_browse_ac.grid(row=0, column=2, padx=6)

        self.nav_frame = ttk.Frame(topv)
        self.nav_frame.grid(row=0, column=3, rowspan=3, padx=10, sticky="n")
        self.btn_prev = ttk.Button(self.nav_frame, text="<", width=4, command=lambda: self.nav_step(-1))
        self.btn_prev.pack(side=tk.TOP, pady=2)
        self.btn_next = ttk.Button(self.nav_frame, text=">", width=4, command=lambda: self.nav_step(+1))
        self.btn_next.pack(side=tk.TOP, pady=2)

        ttk.Label(topv, text="Vibration (csv):").grid(row=1, column=0, sticky="w")
        self.view_vib_csv = tk.StringVar()
        self.ent_view_vib = ttk.Entry(topv, textvariable=self.view_vib_csv, width=70)
        self.ent_view_vib.grid(row=1, column=1, sticky="we", padx=6)
        self.btn_browse_vib = ttk.Button(topv, text="Browse", command=self.browse_view_vib_csv)
        self.btn_browse_vib.grid(row=1, column=2, padx=6)

        ttk.Label(topv, text="Acoustic (wav):").grid(row=2, column=0, sticky="w")
        self.view_wav = tk.StringVar()
        self.ent_view_wav = ttk.Entry(topv, textvariable=self.view_wav, width=70)
        self.ent_view_wav.grid(row=2, column=1, sticky="we", padx=6)
        self.btn_browse_wav = ttk.Button(topv, text="Browse", command=self.browse_view_wav)
        self.btn_browse_wav.grid(row=2, column=2, padx=6)

        # Plot/Play buttons on right
        btnv = ttk.Frame(topv)
        btnv.grid(row=0, column=4, rowspan=3, padx=10, sticky="n")
        self.btn_plot = ttk.Button(btnv, text="Plot", command=self.on_plot)
        self.btn_plot.pack(side=tk.TOP, fill=tk.X, pady=2)
        self.btn_play = ttk.Button(btnv, text="Play", command=self.on_play)
        self.btn_play.pack(side=tk.TOP, fill=tk.X, pady=2)

        # (Req #1) Replace Plot(s) with Start/End time fields
        self.plot_t0_var = tk.DoubleVar(value=0.0)
        self.plot_t1_var = tk.DoubleVar(value=0.0)  # will auto-update to file length

        ttk.Label(topv, text="Start time (s):").grid(row=3, column=3, sticky="e", pady=(6, 0))
        self.ent_t0 = ttk.Entry(topv, textvariable=self.plot_t0_var, width=10)
        self.ent_t0.grid(row=4, column=3, sticky="e", pady=(2, 0))

        ttk.Label(topv, text="End time (s):").grid(row=3, column=4, sticky="w", pady=(6, 0))
        self.ent_t1 = ttk.Entry(topv, textvariable=self.plot_t1_var, width=10)
        self.ent_t1.grid(row=4, column=4, sticky="w", pady=(2, 0))

        topv.grid_columnconfigure(1, weight=1)

        # Plot area
        self.fig = Figure(figsize=(10, 4), dpi=100)
        self.ax_audio = self.fig.add_subplot(211)
        self.ax_vib = self.fig.add_subplot(212)

        self.canvas = FigureCanvasTkAgg(self.fig, master=self.frm_view)
        self.canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)
        NavigationToolbar2Tk(self.canvas, self.frm_view)

        self.paned.add(self.frm_log, weight=2)
        self.paned.add(self.frm_view, weight=1)

        # Capture-lock widgets
        self.lock_widgets = [
            self.port_combo, self.btn_refresh,
            self.ent_baud, self.ent_label, self.ent_base,
            self.ent_outdir, self.btn_browse_out,
            self.ent_dataset, self.btn_browse_dataset,
            self.chk_echo, self.btn_start
        ]

        self.refresh_ports()

    # ---------------- Terminal helpers ----------------
    def enqueue_log(self, msg: str):
        self.log_queue.put(msg)

    def _drain_logs(self):
        try:
            while True:
                msg = self.log_queue.get_nowait()
                self.txt.insert(tk.END, msg + "\n")
                self.txt.see(tk.END)
                with LOG_FILE.open("a", encoding="utf-8") as f:
                    f.write(msg + "\n")
        except queue.Empty:
            pass
        self.after(100, self._drain_logs)

    def on_clear_terminal(self):
        self.txt.delete("1.0", tk.END)

    # ---------------- Enable/Disable controls ----------------
    def set_capture_lock(self, locked: bool):
        for w in self.lock_widgets:
            try:
                if isinstance(w, ttk.Combobox):
                    w.configure(state="disabled" if locked else "readonly")
                else:
                    w.configure(state="disabled" if locked else "normal")
            except Exception:
                pass

        # Stop enabled only during capture
        self.btn_stop.configure(state="normal" if locked else "disabled")

        # Clear/Toggle always enabled
        self.btn_clear.configure(state="normal")
        self.btn_toggle_viewer.configure(state="normal")

    def on_worker_finished(self):
        self.after(0, lambda: self.set_capture_lock(False))

    # ---------------- Browse / ports ----------------
    def refresh_ports(self):
        ports = [p.device for p in list_ports.comports()]
        self.port_combo["values"] = ports
        if self.port_var.get() not in ports and ports:
            self.port_var.set(ports[0])

    def browse_outdir(self):
        d = filedialog.askdirectory(initialdir=self.outdir_var.get() or str(SCRIPT_DIR))
        if d:
            self.outdir_var.set(d)

    def browse_datasetdir(self):
        d = filedialog.askdirectory(initialdir=self.datasetdir_var.get() or str(SCRIPT_DIR))
        if d:
            self.datasetdir_var.set(d)

    def browse_view_ac_csv(self):
        p = filedialog.askopenfilename(filetypes=[("CSV", "*.csv")])
        if p:
            self.view_ac_csv.set(p)
            self._sync_other_fields_from_acoustic(Path(p))
            self._update_time_defaults_from_files()

    def browse_view_vib_csv(self):
        p = filedialog.askopenfilename(filetypes=[("CSV", "*.csv")])
        if p:
            self.view_vib_csv.set(p)
            self._sync_other_fields_from_vibration(Path(p))
            self._update_time_defaults_from_files()

    def browse_view_wav(self):
        p = filedialog.askopenfilename(filetypes=[("WAV", "*.wav")])
        if p:
            self.view_wav.set(p)

    # ---------------- Start/Stop ----------------
    def on_start(self):
        if self.worker and self.worker.is_alive():
            messagebox.showinfo("DL Logger", "Capture already running.")
            return

        port = self.port_var.get().strip()
        if not port:
            messagebox.showerror("DL Logger", "Select COM port")
            return

        try:
            baud = int(self.baud_var.get())
        except Exception:
            messagebox.showerror("DL Logger", "Invalid baud")
            return

        outdir = Path(self.outdir_var.get()).expanduser().resolve()
        dataset_dir = Path(self.datasetdir_var.get()).expanduser().resolve()
        label = self.label_var.get().strip()
        base = self.base_var.get().strip()
        echo = bool(self.echo_var.get())

        ensure_dir(outdir)
        ensure_dir(dataset_dir)

        SETTINGS.update({
            "port": port,
            "baud": baud,
            "outdir": str(outdir),
            "datasetdir": str(dataset_dir),
            "label": label,
            "base": base,
            "echo": echo,
        })
        save_settings(SETTINGS)

        ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.enqueue_log(f"===== Capture start @ {ts} =====")

        self.worker = CaptureWorker(
            app=self,
            port=port,
            baud=baud,
            outdir=outdir,
            dataset_dir=dataset_dir,
            label=label,
            base=base,
            echo=echo,
        )
        self.worker.start()
        self.enqueue_log(f"[UI  ] Started capture → {port} @ {baud}")

        self.set_capture_lock(True)

    def on_stop(self):
        if self.worker:
            self.worker.request_stop()
            self.enqueue_log("[UI  ] Stop requested.")

    # ---------------- Viewer sync & navigation ----------------
    def update_viewer_fields(self, audio_csv: Path, vib_csv: Path, wav_path: Path):
        def _do():
            self.view_ac_csv.set(str(audio_csv))
            self.view_vib_csv.set(str(vib_csv))
            self.view_wav.set(str(wav_path))
            self._update_time_defaults_from_files()
        self.after(0, _do)

    def _parse_base_idx_from_acoustic(self, p: Path):
        m = re.match(r"^(.*)_(\d{3})_acoustic\.csv$", p.name)
        if not m:
            return None, None
        return m.group(1), int(m.group(2))

    def _parse_base_idx_from_vibration(self, p: Path):
        m = re.match(r"^(.*)_(\d{3})_vibration\.csv$", p.name)
        if not m:
            return None, None
        return m.group(1), int(m.group(2))

    def _yamnet_wav_for(self, base: str, idx: int) -> Path:
        dataset_dir = Path(self.datasetdir_var.get()).expanduser().resolve()
        label = (self.label_var.get().strip().lower() or "unlabeled")
        return dataset_dir / YAMNET_DIRNAME / "acoustic" / label / f"{base}_{idx:03d}.wav"

    def _sync_other_fields_from_acoustic(self, ac_path: Path):
        base, idx = self._parse_base_idx_from_acoustic(ac_path)
        if base is None:
            return
        vib_path = ac_path.with_name(f"{base}_{idx:03d}_vibration.csv")
        wav_path = self._yamnet_wav_for(base, idx)
        self.view_vib_csv.set(str(vib_path))
        self.view_wav.set(str(wav_path))

    def _sync_other_fields_from_vibration(self, vib_path: Path):
        base, idx = self._parse_base_idx_from_vibration(vib_path)
        if base is None:
            return
        ac_path = vib_path.with_name(f"{base}_{idx:03d}_acoustic.csv")
        wav_path = self._yamnet_wav_for(base, idx)
        self.view_ac_csv.set(str(ac_path))
        self.view_wav.set(str(wav_path))

    def nav_step(self, step: int):
        cur = Path(self.view_ac_csv.get()).expanduser()
        if not cur.exists():
            messagebox.showerror("Nav", "Acoustic (csv) path is not valid.")
            return

        base, idx = self._parse_base_idx_from_acoustic(cur)
        if base is None:
            messagebox.showerror("Nav", "Acoustic (csv) filename must be <base>_NNN_acoustic.csv")
            return

        folder = cur.parent
        files = sorted(folder.glob(f"{base}_*_acoustic.csv"))

        valid = []
        for p in files:
            b2, i2 = self._parse_base_idx_from_acoustic(p)
            if b2 == base and i2 is not None:
                valid.append((i2, p))
        valid.sort(key=lambda x: x[0])
        if not valid:
            messagebox.showerror("Nav", "No matching acoustic files found in folder.")
            return

        idx_list = [i for i, _ in valid]
        pos = idx_list.index(idx) if idx in idx_list else 0
        new_pos = max(0, min(pos + step, len(valid) - 1))
        new_idx, new_ac = valid[new_pos]

        self.view_ac_csv.set(str(new_ac))
        self.view_vib_csv.set(str(new_ac.with_name(f"{base}_{new_idx:03d}_vibration.csv")))
        self.view_wav.set(str(self._yamnet_wav_for(base, new_idx)))
        self._update_time_defaults_from_files()

    # ---------------- Time defaults (Req #1/#5) ----------------
    def _update_time_defaults_from_files(self):
        """
        Default: start=0, end=full file length.
        We use max(audio_duration, vib_duration) so the selected end covers both plots.
        """
        try:
            ac_path = Path(self.view_ac_csv.get()).expanduser().resolve()
            vb_path = Path(self.view_vib_csv.get()).expanduser().resolve()

            ac_T = 0.0
            vb_T = 0.0

            if ac_path.is_file():
                ac_len = len(read_1col_csv_as_floats(ac_path))
                ac_T = ac_len / ACOUSTIC_SAMPLE_RATE_HZ if ac_len > 0 else 0.0

            if vb_path.is_file():
                vb_len = len(read_1col_csv_as_floats(vb_path))
                vb_T = vb_len / VIB_SAMPLE_RATE_HZ if vb_len > 0 else 0.0

            end_T = max(ac_T, vb_T)

            # Always set start to 0.0 if user hasn't typed something else negative
            if self.plot_t0_var.get() < 0:
                self.plot_t0_var.set(0.0)

            # Update end time to file length (overwrite)
            self.plot_t1_var.set(round(end_T, 6))
        except Exception:
            # don't crash UI if file is huge or temporarily missing
            pass

    # ---------------- Plot / Play ----------------
    def _downsample_for_plot(self, t: np.ndarray, x: np.ndarray, max_points: int = 20000):
        if len(x) <= max_points:
            return t, x
        step = max(1, len(x) // max_points)
        return t[::step], x[::step]

    def on_plot(self):
        try:
            ac_path = Path(self.view_ac_csv.get()).expanduser().resolve()
            vb_path = Path(self.view_vib_csv.get()).expanduser().resolve()

            if not ac_path.is_file() or not vb_path.is_file():
                messagebox.showerror("Plot", "Select valid Acoustic (csv) and Vibration (csv) files.")
                return

            ac = read_1col_csv_as_floats(ac_path)
            vb = read_1col_csv_as_floats(vb_path)
            if len(ac) == 0 or len(vb) == 0:
                messagebox.showerror("Plot", "One of the files is empty.")
                return

            ac_T = len(ac) / ACOUSTIC_SAMPLE_RATE_HZ
            vb_T = len(vb) / VIB_SAMPLE_RATE_HZ

            t0 = float(self.plot_t0_var.get())
            t1 = float(self.plot_t1_var.get())

            if t0 < 0:
                t0 = 0.0
            # clamp t1 to max length (covers both)
            maxT = max(ac_T, vb_T)
            if t1 <= 0 or t1 > maxT:
                t1 = maxT

            if t1 <= t0:
                messagebox.showerror("Plot", "End time must be greater than Start time.")
                return

            # per-signal clamp
            ac_t0 = max(0.0, min(t0, ac_T))
            ac_t1 = max(0.0, min(t1, ac_T))
            vb_t0 = max(0.0, min(t0, vb_T))
            vb_t1 = max(0.0, min(t1, vb_T))

            ac_i0 = int(round(ac_t0 * ACOUSTIC_SAMPLE_RATE_HZ))
            ac_i1 = int(round(ac_t1 * ACOUSTIC_SAMPLE_RATE_HZ))
            vb_i0 = int(round(vb_t0 * VIB_SAMPLE_RATE_HZ))
            vb_i1 = int(round(vb_t1 * VIB_SAMPLE_RATE_HZ))

            ac_seg = np.asarray(ac[ac_i0:ac_i1], dtype=np.float64)
            vb_seg = np.asarray(vb[vb_i0:vb_i1], dtype=np.float64)

            t_ac = (np.arange(ac_i0, ac_i0 + len(ac_seg), dtype=np.float64) / ACOUSTIC_SAMPLE_RATE_HZ)
            t_vb = (np.arange(vb_i0, vb_i0 + len(vb_seg), dtype=np.float64) / VIB_SAMPLE_RATE_HZ)

            t_ac, ac_seg = self._downsample_for_plot(t_ac, ac_seg)
            t_vb, vb_seg = self._downsample_for_plot(t_vb, vb_seg)

            self.ax_audio.clear()
            self.ax_vib.clear()

            self.ax_audio.set_title(ac_path.name)
            self.ax_vib.set_title(vb_path.name)

            self.ax_audio.set_xlabel("Time (s)")
            self.ax_audio.set_ylabel("Amplitude")
            self.ax_vib.set_xlabel("Time (s)")
            self.ax_vib.set_ylabel("Amplitude")

            if len(ac_seg) > 0:
                self.ax_audio.plot(t_ac, ac_seg)
            if len(vb_seg) > 0:
                self.ax_vib.plot(t_vb, vb_seg)

            self.fig.tight_layout()
            self.canvas.draw()

        except Exception as e:
            messagebox.showerror("Plot error", str(e))

    def on_play(self):
        try:
            wav_path = Path(self.view_wav.get()).expanduser().resolve()
            if not wav_path.is_file():
                messagebox.showerror("Play", "Acoustic (wav) path is not valid.")
                return

            if sys.platform.startswith("win"):
                os.startfile(str(wav_path))  # noqa
            elif sys.platform == "darwin":
                subprocess.Popen(["afplay", str(wav_path)])
            else:
                subprocess.Popen(["aplay", str(wav_path)])
        except Exception as e:
            messagebox.showerror("Play", str(e))

    # ---------------- Viewer collapse/expand ----------------
    def toggle_viewer(self):
        if self.viewer_visible:
            try:
                self.paned.forget(self.frm_view)
            except Exception:
                pass
            self.viewer_visible = False
        else:
            try:
                self.paned.add(self.frm_view, weight=1)
            except Exception:
                pass
            self.viewer_visible = True


def main():
    if not LOG_FILE.exists():
        LOG_FILE.write_text("", encoding="utf-8")
    app = App()
    app.mainloop()


if __name__ == "__main__":
    main()
