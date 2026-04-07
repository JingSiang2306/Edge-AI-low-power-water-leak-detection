import os
import re
import json
import shutil
import random
import datetime
from pathlib import Path

import numpy as np
import pandas as pd
import tensorflow as tf
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import seaborn as sns

# ============================================================
# TF_vibration_v2.py
# STM32-friendly vibration pipeline:
# CSV vibration samples -> overlapping raw windows -> 1D CNN -> TFLite
# ============================================================

# -----------------------------
# USER CONFIG
# -----------------------------
DATASET_ROOT = "./dataset_YAMnet/vibration"   # must contain /background and /leak
WORKSPACE_PREFIX = "YAMnet_tflite_vibration_"
RANDOM_SEED = 42

# Split ratio
TRAIN_RATIO = 0.70
VAL_RATIO = 0.15
TEST_RATIO = 0.15

# Vibration window settings
# Approx based on your earlier vibration capture context (~6.6 kHz).
# 0.96 s window keeps it conceptually similar to audio_v3.
VIB_FS = 6645
PATCH_SECONDS = 0.96
PATCH_SAMPLES = int(round(VIB_FS * PATCH_SECONDS))     # ~6379
PATCH_HOP_SECONDS = 0.48                               # 50% overlap
PATCH_HOP_SAMPLES = int(round(VIB_FS * PATCH_HOP_SECONDS))

# Training settings
BATCH_SIZE = 32
EPOCHS = 300
LEARNING_RATE = 1e-3

# Quantization
REP_DATASET_SAMPLES = 100

# Validation visualization
HIGHLIGHT_THRESHOLD = 0.5

# Labels
CLASS_MAPPING = {
    "background": 0,
    "leak": 1
}
INV_CLASS_MAPPING = {v: k for k, v in CLASS_MAPPING.items()}

# -----------------------------
# REPRODUCIBILITY
# -----------------------------
random.seed(RANDOM_SEED)
np.random.seed(RANDOM_SEED)
tf.random.set_seed(RANDOM_SEED)


def ask_version():
    pattern = re.compile(r"^v\d+(?:\.\d+)*$")
    while True:
        version = input("Enter version number (e.g. v1.0, v1.5, v2.12): ").strip()
        if pattern.match(version):
            return version
        print("Invalid version format. Example valid formats: v1.0, v1.5, v2.12, v3")


def make_workspace(version):
    workspace_dir = Path(f"./{WORKSPACE_PREFIX}{version}")
    split_dir = workspace_dir / "dataset_split"
    logs_dir = workspace_dir / "logs"
    model_dir = workspace_dir / "models"
    meta_dir = workspace_dir / "metadata"
    test_validation_img_dir = meta_dir / "test validation image"

    if workspace_dir.exists():
        print(f"\nWorkspace folder already exists: {workspace_dir}")
        choice = input("Overwrite existing folder? (y/n): ").strip().lower()
        if choice != "y":
            raise SystemExit("Stopped by user.")
        shutil.rmtree(workspace_dir)

    split_dir.mkdir(parents=True, exist_ok=True)
    logs_dir.mkdir(parents=True, exist_ok=True)
    model_dir.mkdir(parents=True, exist_ok=True)
    meta_dir.mkdir(parents=True, exist_ok=True)
    test_validation_img_dir.mkdir(parents=True, exist_ok=True)

    return workspace_dir, split_dir, logs_dir, model_dir, meta_dir, test_validation_img_dir


def list_csv_files(folder):
    folder = Path(folder)
    if not folder.exists():
        raise FileNotFoundError(f"Folder not found: {folder}")
    return sorted([p for p in folder.iterdir() if p.is_file() and p.suffix.lower() == ".csv"])


def split_file_list(file_list, train_ratio, val_ratio, test_ratio):
    assert abs((train_ratio + val_ratio + test_ratio) - 1.0) < 1e-9

    shuffled = file_list[:]
    random.shuffle(shuffled)

    n = len(shuffled)
    n_train = max(1, int(round(n * train_ratio))) if n >= 3 else max(1, n - 2)
    n_val = max(1, int(round(n * val_ratio))) if n >= 3 else 1
    n_test = n - n_train - n_val

    if n_test < 1 and n >= 3:
        n_test = 1
        if n_train > n_val:
            n_train -= 1
        else:
            n_val -= 1

    if n < 3:
        if n == 1:
            return shuffled, [], []
        elif n == 2:
            return [shuffled[0]], [shuffled[1]], []

    train_files = shuffled[:n_train]
    val_files = shuffled[n_train:n_train + n_val]
    test_files = shuffled[n_train + n_val:]

    return train_files, val_files, test_files


def copy_split_files(src_files, dest_root, split_name, class_name):
    dest_dir = Path(dest_root) / split_name / class_name
    dest_dir.mkdir(parents=True, exist_ok=True)

    copied_paths = []
    for src in src_files:
        dst = dest_dir / src.name
        shutil.copy2(src, dst)
        copied_paths.append(dst)
    return copied_paths


def create_dataset_split(dataset_root, split_dir):
    manifest = {}

    for class_name in CLASS_MAPPING.keys():
        class_folder = Path(dataset_root) / class_name
        files = list_csv_files(class_folder)

        if len(files) == 0:
            raise RuntimeError(f"No .csv files found in {class_folder}")

        train_files, val_files, test_files = split_file_list(
            files, TRAIN_RATIO, VAL_RATIO, TEST_RATIO
        )

        copied_train = copy_split_files(train_files, split_dir, "train", class_name)
        copied_val = copy_split_files(val_files, split_dir, "val", class_name)
        copied_test = copy_split_files(test_files, split_dir, "test", class_name)

        manifest[class_name] = {
            "original_count": len(files),
            "train_count": len(copied_train),
            "val_count": len(copied_val),
            "test_count": len(copied_test),
            "train_files": [str(p) for p in copied_train],
            "val_files": [str(p) for p in copied_val],
            "test_files": [str(p) for p in copied_test],
        }

    return manifest


def load_vibration_csv(path):
    """
    Robust CSV loader for 1-column vibration files.
    Uses header=None so the first numeric row is not lost.
    If multiple columns exist, it keeps the first numeric column.
    """
    df = pd.read_csv(path, header=None)

    # Convert all columns to numeric where possible
    df = df.apply(pd.to_numeric, errors="coerce")

    # Drop columns that are fully NaN
    df = df.dropna(axis=1, how="all")

    if df.shape[1] == 0:
        raise RuntimeError(f"No numeric data found in file: {path}")

    # Keep the first numeric column
    values = df.iloc[:, 0].dropna().to_numpy(dtype=np.float32)

    if values.size == 0:
        raise RuntimeError(f"No valid samples found in file: {path}")

    return values


def preprocess_vibration_signal(x):
    """
    Light preprocessing only:
    - remove DC offset
    - scale by max abs to reduce file-to-file amplitude explosion
    """
    x = x.astype(np.float32)
    x = x - np.mean(x)

    max_abs = np.max(np.abs(x))
    if max_abs > 1e-8:
        x = x / max_abs

    return x.astype(np.float32)


def extract_patches_from_file(file_path):
    """
    Returns multiple overlapping raw windows from one CSV file.
    If file is shorter than one patch, pad it to one patch.
    Output patch shape: (PATCH_SAMPLES, 1)
    """
    signal = load_vibration_csv(file_path)
    signal = preprocess_vibration_signal(signal)

    patches = []

    if len(signal) < PATCH_SAMPLES:
        padded = np.pad(signal, (0, PATCH_SAMPLES - len(signal)), mode="constant")
        patches.append(padded[..., np.newaxis].astype(np.float32))
        return patches

    start = 0
    while start + PATCH_SAMPLES <= len(signal):
        chunk = signal[start:start + PATCH_SAMPLES]
        patches.append(chunk[..., np.newaxis].astype(np.float32))
        start += PATCH_HOP_SAMPLES

    # include tail if not aligned exactly
    if start < len(signal):
        tail = signal[-PATCH_SAMPLES:]
        if len(tail) == PATCH_SAMPLES:
            patches.append(tail[..., np.newaxis].astype(np.float32))

    return patches


def extract_patches_with_times_from_signal(signal):
    signal = signal.astype(np.float32)
    patches = []
    intervals = []

    if len(signal) < PATCH_SAMPLES:
        padded = np.pad(signal, (0, PATCH_SAMPLES - len(signal)), mode="constant")
        patches.append(padded[..., np.newaxis].astype(np.float32))
        intervals.append((0.0, PATCH_SECONDS))
        return patches, intervals

    start = 0
    while start + PATCH_SAMPLES <= len(signal):
        chunk = signal[start:start + PATCH_SAMPLES]
        patches.append(chunk[..., np.newaxis].astype(np.float32))
        intervals.append((start / VIB_FS, (start + PATCH_SAMPLES) / VIB_FS))
        start += PATCH_HOP_SAMPLES

    if start < len(signal):
        tail_start = max(0, len(signal) - PATCH_SAMPLES)
        tail = signal[tail_start:tail_start + PATCH_SAMPLES]
        if len(tail) == PATCH_SAMPLES:
            tail_interval = (tail_start / VIB_FS, (tail_start + PATCH_SAMPLES) / VIB_FS)
            if not intervals or abs(intervals[-1][0] - tail_interval[0]) > 1e-9:
                patches.append(tail[..., np.newaxis].astype(np.float32))
                intervals.append(tail_interval)

    return patches, intervals


def build_dataset_from_split(split_root):
    X = []
    y = []
    stats = {
        "files_per_class": {},
        "patches_per_class": {}
    }

    for class_name, label in CLASS_MAPPING.items():
        class_dir = Path(split_root) / class_name
        csv_files = list_csv_files(class_dir)

        file_count = 0
        patch_count = 0

        for csv_file in csv_files:
            patches = extract_patches_from_file(csv_file)
            for patch in patches:
                X.append(patch)
                y.append(label)
                patch_count += 1
            file_count += 1

        stats["files_per_class"][class_name] = file_count
        stats["patches_per_class"][class_name] = patch_count

    X = np.asarray(X, dtype=np.float32)
    y = np.asarray(y, dtype=np.float32)

    return X, y, stats


def compute_normalization_stats(X_train):
    mean = float(np.mean(X_train))
    std = float(np.std(X_train))
    if std < 1e-8:
        std = 1.0
    return mean, std


def normalize_dataset(X, mean, std):
    return ((X - mean) / std).astype(np.float32)


def build_model():
    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(PATCH_SAMPLES, 1), name="vibration_input"),

        tf.keras.layers.Conv1D(16, kernel_size=9, strides=1, padding="same", activation="relu"),
        tf.keras.layers.MaxPooling1D(pool_size=2),

        tf.keras.layers.Conv1D(24, kernel_size=7, strides=1, padding="same", activation="relu"),
        tf.keras.layers.MaxPooling1D(pool_size=2),

        tf.keras.layers.Conv1D(32, kernel_size=5, strides=1, padding="same", activation="relu"),
        tf.keras.layers.MaxPooling1D(pool_size=2),

        tf.keras.layers.Conv1D(48, kernel_size=5, strides=1, padding="same", activation="relu"),
        tf.keras.layers.GlobalAveragePooling1D(),

        tf.keras.layers.Dense(24, activation="relu"),
        tf.keras.layers.Dropout(0.2),
        tf.keras.layers.Dense(1, activation="sigmoid")
    ], name="vibration_1dcnn")

    optimizer = tf.keras.optimizers.Adam(learning_rate=LEARNING_RATE)
    model.compile(
        optimizer=optimizer,
        loss="binary_crossentropy",
        metrics=[
            "accuracy",
            tf.keras.metrics.Precision(name="precision"),
            tf.keras.metrics.Recall(name="recall")
        ]
    )
    return model


def make_callbacks(logs_dir, model_dir, version):
    timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")

    tensorboard_log_dir = Path(logs_dir) / f"run_{version}_{timestamp}"
    checkpoint_path = Path(model_dir) / f"best_model_{version}.keras"

    callbacks = [
        tf.keras.callbacks.TensorBoard(
            log_dir=str(tensorboard_log_dir),
            histogram_freq=1
        ),
        tf.keras.callbacks.ModelCheckpoint(
            filepath=str(checkpoint_path),
            monitor="val_accuracy",
            save_best_only=True,
            mode="max",
            verbose=1
        ),
        tf.keras.callbacks.EarlyStopping(
            monitor="val_loss",
            patience=10,
            restore_best_weights=True,
            verbose=1
        ),
        tf.keras.callbacks.ReduceLROnPlateau(
            monitor="val_loss",
            factor=0.5,
            patience=4,
            min_lr=1e-6,
            verbose=1
        )
    ]

    return callbacks, checkpoint_path, tensorboard_log_dir


def evaluate_and_save_metrics(model, X_test, y_test, meta_dir, version):
    results = model.evaluate(X_test, y_test, verbose=1, return_dict=True)

    y_prob = model.predict(X_test, verbose=0).reshape(-1)
    y_pred = (y_prob >= 0.5).astype(np.int32)
    y_true = y_test.astype(np.int32)
    # Plot confusion matrix
    plot_confusion_matrix(y_true, y_pred, meta_dir, version)

    tp = int(np.sum((y_true == 1) & (y_pred == 1)))
    tn = int(np.sum((y_true == 0) & (y_pred == 0)))
    fp = int(np.sum((y_true == 0) & (y_pred == 1)))
    fn = int(np.sum((y_true == 1) & (y_pred == 0)))

    specificity = tn / (tn + fp) if (tn + fp) > 0 else 0.0
    precision = tp / (tp + fp) if (tp + fp) > 0 else 0.0
    recall = tp / (tp + fn) if (tp + fn) > 0 else 0.0
    f1 = (2 * precision * recall / (precision + recall)) if (precision + recall) > 0 else 0.0
    balanced_accuracy = (recall + specificity) / 2.0

    report = {
        "version": version,
        "test_loss": float(results.get("loss", 0.0)),
        "test_accuracy": float(results.get("accuracy", 0.0)),
        "test_precision": float(results.get("precision", 0.0)),
        "test_recall": float(results.get("recall", 0.0)),
        "specificity": float(specificity),
        "f1_score": float(f1),
        "balanced_accuracy": float(balanced_accuracy),
        "confusion_matrix": {
            "TP": tp,
            "TN": tn,
            "FP": fp,
            "FN": fn
        }
    }

    metrics_path = Path(meta_dir) / f"test_metrics_{version}.json"
    with open(metrics_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=4)

    print("\n===== TEST RESULTS =====")
    for k, v in report.items():
        if k != "confusion_matrix":
            print(f"{k}: {v}")
    print("confusion_matrix:", report["confusion_matrix"])

    return report, metrics_path

def plot_confusion_matrix(y_true, y_pred, meta_dir, version):
    from sklearn.metrics import confusion_matrix

    cm = confusion_matrix(y_true, y_pred)

    plt.figure()
    sns.heatmap(cm, annot=True, fmt="d", cmap="Blues",
                xticklabels=["Background", "Leak"],
                yticklabels=["Background", "Leak"])

    plt.xlabel("Predicted")
    plt.ylabel("Actual")
    plt.title("Confusion Matrix")

    save_path = Path(meta_dir) / f"confusion_matrix_{version}.png"
    plt.savefig(save_path)
    plt.close()

    print(f"Confusion matrix saved to: {save_path}")

def save_normalization_stats(mean, std, meta_dir, version):
    stats = {
        "mean": mean,
        "std": std,
        "vib_fs": VIB_FS,
        "patch_seconds": PATCH_SECONDS,
        "patch_samples": PATCH_SAMPLES,
        "patch_hop_seconds": PATCH_HOP_SECONDS,
        "patch_hop_samples": PATCH_HOP_SAMPLES
    }

    stats_path = Path(meta_dir) / f"normalization_stats_{version}.json"
    with open(stats_path, "w", encoding="utf-8") as f:
        json.dump(stats, f, indent=4)

    return stats_path


def save_split_manifest(manifest, meta_dir, version):
    manifest_path = Path(meta_dir) / f"split_manifest_{version}.json"
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=4)
    return manifest_path


def make_representative_dataset(X_train):
    sample_count = min(REP_DATASET_SAMPLES, len(X_train))
    indices = np.random.choice(len(X_train), size=sample_count, replace=False)

    def representative_data_gen():
        for idx in indices:
            sample = X_train[idx:idx + 1].astype(np.float32)
            yield [sample]

    return representative_data_gen


def export_tflite_models(model, X_train, model_dir, version):
    float_tflite_path = Path(model_dir) / f"YAMnet_vibration_model_{version}_float32.tflite"
    int8_tflite_path = Path(model_dir) / f"YAMnet_vibration_model_{version}_int8.tflite"

    print("\nExporting float32 TFLite...")
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS]
    tflite_float = converter.convert()

    with open(float_tflite_path, "wb") as f:
        f.write(tflite_float)

    print(f"Saved: {float_tflite_path}")

    print("\nExporting int8 TFLite...")
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = make_representative_dataset(X_train)
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    tflite_int8 = converter.convert()

    with open(int8_tflite_path, "wb") as f:
        f.write(tflite_int8)

    print(f"Saved: {int8_tflite_path}")

    return float_tflite_path, int8_tflite_path


def save_model_summary(model, meta_dir, version):
    summary_path = Path(meta_dir) / f"model_summary_{version}.txt"
    with open(summary_path, "w", encoding="utf-8") as f:
        model.summary(print_fn=lambda x: f.write(x + "\n"))
    return summary_path


def save_training_history(history, meta_dir, version):
    history_path = Path(meta_dir) / f"training_history_{version}.json"
    with open(history_path, "w", encoding="utf-8") as f:
        json.dump(history.history, f, indent=4)
    return history_path


def plot_training_curves(history, meta_dir, version):
    hist = history.history
    epochs = range(1, len(hist.get("accuracy", [])) + 1)
    if not hist or not list(epochs):
        return None

    fig, ax = plt.subplots(figsize=(8, 5))
    if "accuracy" in hist:
        ax.plot(epochs, hist["accuracy"], label="Train Accuracy")
    if "val_accuracy" in hist:
        ax.plot(epochs, hist["val_accuracy"], label="Validation Accuracy")
    ax.set_xlabel("Epoch")
    ax.set_ylabel("Accuracy")
    ax.set_title(f"Training Accuracy vs Epoch ({version})")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()

    out_path = Path(meta_dir) / f"epoch_vs_accuracy_{version}.png"
    fig.savefig(out_path, dpi=200)
    plt.close(fig)
    return out_path


def sanitize_name(name):
    return re.sub(r'[^A-Za-z0-9._-]+', '_', name)


def merge_intervals(intervals):
    if not intervals:
        return []
    intervals = sorted(intervals, key=lambda x: x[0])
    merged = [intervals[0]]
    for start, end in intervals[1:]:
        last_start, last_end = merged[-1]
        if start <= last_end:
            merged[-1] = (last_start, max(last_end, end))
        else:
            merged.append((start, end))
    return merged


def visualize_test_file_predictions(model, file_path, true_class_name, mean, std, out_dir):
    signal = preprocess_vibration_signal(load_vibration_csv(file_path))
    duration = len(signal) / VIB_FS if len(signal) > 0 else PATCH_SECONDS

    patches, intervals = extract_patches_with_times_from_signal(signal)
    X = np.asarray(patches, dtype=np.float32)
    Xn = normalize_dataset(X, mean, std)
    probs = model.predict(Xn, verbose=0).reshape(-1)
    pred_label = int(np.mean(probs) >= HIGHLIGHT_THRESHOLD)
    pred_class_name = INV_CLASS_MAPPING[pred_label]
    highlight_intervals = merge_intervals([
        interval for interval, prob in zip(intervals, probs) if prob >= HIGHLIGHT_THRESHOLD
    ])

    waveform_time = np.arange(len(signal)) / VIB_FS
    patch_centers = np.array([(s + e) / 2.0 for s, e in intervals], dtype=np.float32)

    fig, axes = plt.subplots(3, 1, figsize=(14, 10), sharex=False)

    ax = axes[0]
    ax.plot(waveform_time, signal, linewidth=0.8)
    for s, e in highlight_intervals:
        ax.axvspan(s, e, alpha=0.25)
    ax.set_xlim(0, max(duration, PATCH_SECONDS))
    ax.set_title(
        f"Vibration test validation: {Path(file_path).name}\n"
        f"True={true_class_name}, Pred={pred_class_name}, MeanLeakProb={float(np.mean(probs)):.3f}, MaxLeakProb={float(np.max(probs)):.3f}"
    )
    ax.set_ylabel("Amplitude")
    ax.grid(True, alpha=0.3)

    ax = axes[1]
    _, _, _, im = ax.specgram(signal, NFFT=256, Fs=VIB_FS, noverlap=128)
    for s, e in highlight_intervals:
        ax.axvspan(s, e, alpha=0.20)
    ax.set_ylabel("Frequency (Hz)")
    ax.set_title("Full-file spectrogram (for interpretation)")
    fig.colorbar(im, ax=ax, pad=0.01, label="Intensity (dB)")

    ax = axes[2]
    if len(probs) > 0:
        ax.plot(patch_centers, probs, marker='o', linewidth=1.2)
        ax.scatter(patch_centers, probs, s=18)
    ax.axhline(HIGHLIGHT_THRESHOLD, linestyle='--', linewidth=1.0, label=f"Threshold = {HIGHLIGHT_THRESHOLD:.2f}")
    for s, e in highlight_intervals:
        ax.axvspan(s, e, alpha=0.20)
    ax.set_xlim(0, max(duration, PATCH_SECONDS))
    ax.set_ylim(0, 1.05)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Leak probability")
    ax.set_title("Patch-level leak probability over time")
    ax.grid(True, alpha=0.3)
    ax.legend()

    fig.tight_layout()
    out_path = Path(out_dir) / f"{sanitize_name(true_class_name)}_{sanitize_name(Path(file_path).stem)}_validation.png"
    fig.savefig(out_path, dpi=200)
    plt.close(fig)
    return out_path


def run_test_validation_visualization(model, split_dir, mean, std, out_dir):
    saved_files = []
    for class_name in CLASS_MAPPING.keys():
        class_dir = Path(split_dir) / "test" / class_name
        csv_files = list_csv_files(class_dir)
        for csv_file in csv_files:
            out_path = visualize_test_file_predictions(model, csv_file, class_name, mean, std, out_dir)
            saved_files.append(str(out_path))
    return saved_files


def main():
    print("===========================================================")
    print(" TF_vibration_v3.py - STM32-friendly vibration CNN trainer ")
    print("===========================================================\n")

    version = ask_version()
    workspace_dir, split_dir, logs_dir, model_dir, meta_dir, test_validation_img_dir = make_workspace(version)

    print(f"\nWorkspace: {workspace_dir}")
    print(f"Original dataset root (untouched): {DATASET_ROOT}")

    # --------------------------------------------
    # Step 1: Copy dataset into split folders
    # --------------------------------------------
    print("\n[1/8] Creating dataset split in workspace...")
    manifest = create_dataset_split(DATASET_ROOT, split_dir)
    manifest_path = save_split_manifest(manifest, meta_dir, version)
    print(f"Split manifest saved to: {manifest_path}")

    # --------------------------------------------
    # Step 2: Build train / val / test patches
    # --------------------------------------------
    print("\n[2/8] Extracting train patches...")
    X_train, y_train, train_stats = build_dataset_from_split(split_dir / "train")

    print("[3/8] Extracting val patches...")
    X_val, y_val, val_stats = build_dataset_from_split(split_dir / "val")

    print("[4/8] Extracting test patches...")
    X_test, y_test, test_stats = build_dataset_from_split(split_dir / "test")

    print("\nPatch statistics:")
    print("Train:", train_stats)
    print("Val  :", val_stats)
    print("Test :", test_stats)

    print(f"\nX_train shape: {X_train.shape}, y_train shape: {y_train.shape}")
    print(f"X_val   shape: {X_val.shape}, y_val   shape: {y_val.shape}")
    print(f"X_test  shape: {X_test.shape}, y_test  shape: {y_test.shape}")

    if len(X_train) == 0 or len(X_val) == 0 or len(X_test) == 0:
        raise RuntimeError("One of train/val/test patch sets is empty. Please check dataset size.")

    # --------------------------------------------
    # Step 3: Normalize using train set only
    # --------------------------------------------
    print("\n[5/8] Computing normalization stats from training set...")
    mean, std = compute_normalization_stats(X_train)
    stats_path = save_normalization_stats(mean, std, meta_dir, version)
    print(f"Normalization stats saved to: {stats_path}")
    print(f"mean = {mean:.6f}, std = {std:.6f}")

    X_train = normalize_dataset(X_train, mean, std)
    X_val = normalize_dataset(X_val, mean, std)
    X_test = normalize_dataset(X_test, mean, std)

    # --------------------------------------------
    # Step 4: Build and train model
    # --------------------------------------------
    print("\n[6/8] Building and training model...")
    model = build_model()
    summary_path = save_model_summary(model, meta_dir, version)
    print(f"Model summary saved to: {summary_path}")

    callbacks, checkpoint_path, tensorboard_log_dir = make_callbacks(logs_dir, model_dir, version)

    print("\nStarting training...")
    history = model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        epochs=EPOCHS,
        batch_size=BATCH_SIZE,
        callbacks=callbacks,
        verbose=1
    )

    history_path = save_training_history(history, meta_dir, version)
    epoch_acc_plot_path = plot_training_curves(history, meta_dir, version)
    print(f"Training history saved to: {history_path}")
    if epoch_acc_plot_path is not None:
        print(f"Epoch-vs-accuracy plot saved to: {epoch_acc_plot_path}")
    print(f"\nBest model checkpoint: {checkpoint_path}")
    print(f"TensorBoard logs: {tensorboard_log_dir}")

    # --------------------------------------------
    # Step 5: Evaluate on test set
    # --------------------------------------------
    print("\n[7/8] Evaluating on test set...")
    _, metrics_path = evaluate_and_save_metrics(model, X_test, y_test, meta_dir, version)
    print(f"Test metrics saved to: {metrics_path}")

    validation_images = run_test_validation_visualization(model, split_dir, mean, std, test_validation_img_dir)
    print(f"Test validation images saved: {len(validation_images)}")

    # --------------------------------------------
    # Step 6: Export TFLite
    # --------------------------------------------
    print("\n[8/8] Exporting TFLite models...")
    float_tflite_path, int8_tflite_path = export_tflite_models(model, X_train, model_dir, version)

    print("\n========================================")
    print("Done.")
    print("========================================")
    print(f"Workspace folder       : {workspace_dir}")
    print(f"Dataset split folder   : {split_dir}")
    print(f"Float32 TFLite         : {float_tflite_path}")
    print(f"Int8 TFLite            : {int8_tflite_path}")
    print(f"Normalization stats    : {stats_path}")
    print(f"Split manifest         : {manifest_path}")
    print(f"Training history       : {history_path}")
    if epoch_acc_plot_path is not None:
        print(f"Epoch-vs-accuracy plot : {epoch_acc_plot_path}")
    print(f"Test metrics           : {metrics_path}")
    print(f"Validation image dir   : {test_validation_img_dir}")
    print("Original dataset folder remains untouched.")


if __name__ == "__main__":
    main()
