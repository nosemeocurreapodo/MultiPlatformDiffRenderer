#!/usr/bin/env python3
# invdepth_to_depth.py
import argparse, math
from pathlib import Path
import numpy as np
import cv2
import glob

def safe_reciprocal(inv, zero_policy="inf", nodata=None, nodata_out=np.nan):
    """
    inv: numpy array (inverse depth 1/z), any dtype; converted to float32
    zero_policy: what to write where inv == 0 or non-finite: 'inf' | 'zero' | 'nan'
    nodata: specific input value to treat as missing (e.g., -1 or 0); optional
    nodata_out: output value for those missing pixels (default NaN)
    """
    inv = inv.astype(np.float32, copy=False)
    out = np.empty_like(inv, dtype=np.float32)

    valid = np.isfinite(inv)
    if nodata is not None:
        valid &= (inv != nodata)

    nonzero = (inv != 0.0) & valid

    # z = 1 / inv_depth
    out[nonzero] = 1.0 / inv[nonzero]

    # places that are valid but inv==0 or non-finite
    bad = (~nonzero) & valid
    if zero_policy == "inf":
        out[bad] = np.float32(np.inf)
    elif zero_policy == "zero":
        out[bad] = 0.0
    else:
        out[bad] = np.float32(np.nan)

    # nodata in → nodata_out out
    if nodata is not None:
        out[~valid] = np.float32(nodata_out)

    return out

def main():
    ap = argparse.ArgumentParser(description="Convert inverse-depth images (1/z) to depth (z).")
    ap.add_argument("inputs", nargs="+",
                    help="Input file(s) or glob(s). Example: 'inv/*.tiff' or file.exr")
    ap.add_argument("--out-dir", default="depth_out", help="Directory to write outputs.")
    ap.add_argument("--suffix", default="_depth", help="Suffix added to filename stem.")
    ap.add_argument("--ext", default=".tiff",
                    help="Output extension (default .tiff). Use .exr if you prefer EXR.")
    ap.add_argument("--nodata", type=float, default=None,
                    help="Input nodata value to treat as missing (e.g., -1 or 0).")
    ap.add_argument("--nodata-out", type=float, default=np.nan,
                    help="Output value for nodata (default NaN).")
    ap.add_argument("--zero-policy", choices=["inf","zero","nan"], default="inf",
                    help="What to write where inv==0 or non-finite (default: inf).")
    args = ap.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    # Expand globs
    files = []
    for pat in args.inputs:
        matches = glob.glob(pat)
        if matches:
            files.extend(matches)
        else:
            # if it's an explicit path that doesn't glob, include as-is
            files.append(pat)
    if not files:
        print("No input files found.")
        return

    for f in files:
        path = Path(f)
        try:
            img = cv2.imread(path)
        except Exception as e:
            print(f"[SKIP] Failed to read {path}: {e}")
            continue

        # If the image has channels, assume first channel is inverse depth.
        #if img.ndim == 3:
        #    img = img[..., 0]

        depth = safe_reciprocal(img, zero_policy=args.zero_policy,
                                nodata=args.nodata, nodata_out=args.nodata_out)
        depth_normalized = cv2.normalize(depth, None, 0, 255.0, cv2.NORM_MINMAX)
        out_path = out_dir / (path.stem + args.suffix + args.ext)
        try:
            # imageio will write float32 TIFF/EXR without scaling.
            cv2.imwrite(out_path, depth_normalized.astype(np.uint8))
            print(f"[OK] {path.name} -> {out_path.name}")
        except Exception as e:
            print(f"[SKIP] Failed to write {out_path}: {e}")

if __name__ == "__main__":
    main()