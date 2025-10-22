#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable

from PIL import Image
from pillow_heif import register_heif_opener

# Enable HEIC/HEIF reading in Pillow
register_heif_opener()

HEIC_EXTS = {".heic", ".heif"}

def iter_heic_files(root: Path, recursive: bool) -> Iterable[Path]:
    it = root.rglob("*") if recursive else root.glob("*")
    for p in it:
        if p.is_file() and p.suffix.lower() in HEIC_EXTS:
            yield p

def convert_one(
    src: Path,
    dst: Path,
    compress_level: int,
    overwrite: bool,
    quiet: bool,
) -> bool:
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists() and not overwrite:
        if not quiet:
            print(f"SKIP (exists): {dst}")
        return False

    try:
        with Image.open(src) as im:
            # Preserve metadata when possible:
            exif = im.info.get("exif")          # PNG supports eXIf chunk in Pillow 9.1+
            icc = im.info.get("icc_profile")

            im.save(
                dst,
                format="PNG",
                compress_level=compress_level,  # 0 (fast/big) .. 9 (small/slow); default 6
                exif=exif if exif else None,
                icc_profile=icc if icc else None,
            )
        if not quiet:
            print(f"OK  : {src}  -->  {dst}")
        return True
    except Exception as e:
        print(f"FAIL: {src} ({e})")
        return False

def main():
    parser = argparse.ArgumentParser(
        description="Convert all .heic/.heif images in a folder to .png."
    )
    parser.add_argument("input", type=Path, help="Input folder containing HEIC/HEIF images.")
    parser.add_argument("output", type=Path, help="Output folder for PNG files.")
    parser.add_argument(
        "-r", "--recursive", action="store_true", help="Recurse into subfolders."
    )
    parser.add_argument(
        "--keep-structure",
        action="store_true",
        help="Mirror subfolder structure under OUTPUT (implies --recursive).",
    )
    parser.add_argument(
        "--overwrite", action="store_true", help="Overwrite existing PNGs if present."
    )
    parser.add_argument(
        "--delete-originals",
        action="store_true",
        help="Delete source .heic/.heif after successful conversion.",
    )
    parser.add_argument(
        "--compress-level",
        type=int,
        default=6,
        choices=range(0, 10),
        metavar="[0-9]",
        help="PNG compression level: 0 (fast/large) to 9 (small/slow). Default: 6.",
    )
    parser.add_argument(
        "--quiet", action="store_true", help="Reduce output (only errors)."
    )

    args = parser.parse_args()

    if args.keep_structure:
        args.recursive = True

    if not args.input.exists() or not args.input.is_dir():
        parser.error(f"Input path must be a directory: {args.input}")

    converted = 0
    skipped = 0
    failed = 0

    for src in iter_heic_files(args.input, args.recursive):
        if args.keep_structure:
            rel = src.relative_to(args.input).with_suffix(".png")
            dst = args.output / rel
        else:
            dst = args.output / f"{src.stem}.png"

        result = convert_one(
            src=src,
            dst=dst,
            compress_level=args.compress_level,
            overwrite=args.overwrite,
            quiet=args.quiet,
        )
        if result:
            converted += 1
            if args.delete_originals:
                try:
                    src.unlink()
                except Exception as e:
                    print(f"Could not delete {src}: {e}")
        else:
            # Distinguish skips vs failures by checking if file exists and overwrite is False
            if dst.exists() and not args.overwrite:
                skipped += 1
            else:
                failed += 1

    total = converted + skipped + failed
    if not args.quiet:
        print("\nSummary")
        print("-------")
        print(f"Total found   : {total}")
        print(f"Converted     : {converted}")
        print(f"Skipped       : {skipped}")
        print(f"Failed        : {failed}")

if __name__ == "__main__":
    main()
