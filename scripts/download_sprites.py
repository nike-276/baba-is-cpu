#!/usr/bin/env python3
"""
Download Baba Is You object sprites from the Baba Is You wiki (Fandom).

Usage:
    python3 scripts/download_sprites.py

Requirements:
    pip install Pillow requests

Output:
    assets/sprites/objects/{noun}.png   — object sprites
    assets/sprites/text/{noun}.png      — text-tile sprites

Sprites are fetched via the MediaWiki API and converted to PNG (first frame).
Run this from the repo root.
"""

import io
import json
import os
import sys
import time
import urllib.parse

try:
    import requests
except ImportError:
    sys.exit("ERROR: 'requests' not installed. Run: pip install requests")

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False
    print("WARNING: Pillow not installed — GIFs saved as-is. "
          "Run: pip install Pillow")

WIKI_API = "https://babaiswiki.fandom.com/api.php"
SESSION   = requests.Session()
SESSION.headers["User-Agent"] = "baba-is-true-sprite-downloader/1.0"

# All nouns in the simulator (must match kind.cpp kTable names).
NOUNS = [
    "algae", "arm", "arrow", "baba", "badbad", "banana", "bat", "bean", "bed",
    "bee", "belt", "bird", "blob", "boat", "boba", "bog", "bolt", "bomb", "bone",
    "book", "bottle", "box", "brain", "brick", "bubble", "bucket", "bug", "bunny",
    "burger", "cactus", "cake", "car", "cart", "cash", "cat", "chair", "cheese",
    "chili", "circle", "cliff", "clock", "cloud", "cog", "crab", "crystal", "cup",
    "dog", "donut", "door", "dot", "drink", "drum", "dust", "ear", "egg", "eye",
    "fence", "fire", "fish", "flag", "flower", "fofo", "foliage", "foot", "fort",
    "fox", "frog", "fruit", "fungi", "fungus", "gate", "gem", "ghost", "grass",
    "guitar", "hand", "hedge", "hihat", "hotdog", "house", "husk", "husks", "ice",
    "it", "jelly", "jiji", "keke", "key", "knight", "ladder", "lamp", "lava",
    "leaf", "lever", "lift", "lily", "line", "lizard", "lock", "love", "me",
    "mirror", "monitor", "monster", "moon", "no", "nose", "orb", "palm", "pants",
    "paper", "pawn", "piano", "pillar", "pipe", "pixel", "pizza", "plane", "planet",
    "plank", "potato", "pumpkin", "reed", "ring", "road", "robot", "rock", "rocket",
    "rose", "rubble", "sax", "scissors", "seed", "shell", "shirt", "shovel", "sign",
    "skull", "snail", "spike", "sprout", "square", "star", "statue", "stick", "stump",
    "sun", "sword", "table", "teeth", "tile", "tower", "track", "train", "tree",
    "trees", "triangle", "trumpet", "turnip", "turtle", "ufo", "vase", "vine",
    "wall", "water", "what", "wind", "worm", "yes",
]

# Wiki file candidates for each noun (tried in order).
# Pattern discovered: ALGAE_0.gif / Text_ALGAE_0.gif (all uppercase).
def _candidates(noun):
    up  = noun.upper()
    cap = noun.capitalize()
    return [
        f"{up}_0.gif",       # primary: e.g. BABA_0.gif
        f"{cap}_0.gif",      # fallback capitalised
        f"{up}.gif",
        f"{cap}.gif",
        f"{up}.png",
        f"{cap}.png",
    ]

def _text_candidates(noun):
    up  = noun.upper()
    cap = noun.capitalize()
    return [
        f"Text_{up}_0.gif",  # primary: e.g. Text_BABA_0.gif
        f"Text_{cap}_0.gif",
        f"Text_{up}.gif",
        f"Text_{cap}.gif",
        f"Text_{up}.png",
    ]


def query_image_url(filename: str) -> str | None:
    params = {
        "action": "query",
        "titles": f"File:{filename}",
        "prop": "imageinfo",
        "iiprop": "url",
        "format": "json",
    }
    try:
        r = SESSION.get(WIKI_API, params=params, timeout=15)
        r.raise_for_status()
        data = r.json()
        pages = data["query"]["pages"]
        page = next(iter(pages.values()))
        if "imageinfo" in page:
            return page["imageinfo"][0]["url"]
    except Exception:
        pass
    return None


def find_url(candidates):
    for name in candidates:
        url = query_image_url(name)
        if url:
            return url, name
    return None, None


def save_image(url: str, out_path: str) -> bool:
    try:
        r = SESSION.get(url, timeout=20)
        r.raise_for_status()
        raw = r.content
        if HAS_PIL:
            img = Image.open(io.BytesIO(raw))
            if hasattr(img, "n_frames") and img.n_frames > 1:
                img.seek(0)  # first frame only
            img = img.convert("RGBA")
            img.save(out_path, "PNG")
        else:
            ext = url.split(".")[-1].split("?")[0]
            alt = out_path.replace(".png", f".{ext}")
            with open(alt, "wb") as f:
                f.write(raw)
        return True
    except Exception as e:
        print(f"         download error: {e}")
        return False


def process(noun, out_dir, candidate_fn, label):
    out_path = os.path.join(out_dir, f"{noun}.png")
    if os.path.exists(out_path):
        print(f"  skip {noun} ({label})")
        return "skip"

    url, filename = find_url(candidate_fn(noun))
    if not url:
        print(f"  MISS {noun} ({label})")
        return "miss"

    if save_image(url, out_path):
        print(f"  OK   {noun} ({label}) <- {filename}")
        time.sleep(0.2)  # be polite to the wiki server
        return "ok"
    return "err"


def main():
    obj_dir  = "assets/sprites/objects"
    text_dir = "assets/sprites/text"
    os.makedirs(obj_dir,  exist_ok=True)
    os.makedirs(text_dir, exist_ok=True)

    counts = {"ok": 0, "skip": 0, "miss": 0, "err": 0}

    print("=== Downloading object sprites ===")
    for noun in NOUNS:
        result = process(noun, obj_dir, _candidates, "obj")
        counts[result] += 1

    print("\n=== Downloading text-tile sprites ===")
    for noun in NOUNS:
        result = process(noun, text_dir, _text_candidates, "txt")
        counts[result] += 1

    print(f"\nDone: {counts['ok']} downloaded, {counts['skip']} skipped, "
          f"{counts['miss']} not found, {counts['err']} errors")
    if counts["miss"] > 0:
        print("Tip: 'miss' means the wiki doesn't have that sprite file — normal for some nouns.")


if __name__ == "__main__":
    main()
