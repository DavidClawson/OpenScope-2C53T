#!/usr/bin/env python3
"""
YouTube Comment Sentiment Analyzer for FNIRSI oscilloscopes.
Fetches comments from top videos and identifies common complaints/praise.

Usage:
    python3 youtube_comments.py                    # default search terms
    python3 youtube_comments.py --terms "FNIRSI 2C53T review"
    python3 youtube_comments.py --video-id HR6_Lp7fH_c  # specific video
"""

import os
import sys
import json
import re
import argparse
from collections import Counter
from pathlib import Path
from urllib.request import urlopen, Request
from urllib.parse import urlencode
from datetime import datetime


def load_env():
    env_path = Path(__file__).parent / ".env"
    if env_path.exists():
        for line in env_path.read_text().splitlines():
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                key, _, value = line.partition("=")
                os.environ.setdefault(key.strip(), value.strip())


load_env()
API_KEY = os.environ.get("YOUTUBE_API_KEY")
BASE_URL = "https://www.googleapis.com/youtube/v3"

# Keywords that signal complaints or issues
COMPLAINT_KEYWORDS = [
    "bug", "buggy", "crash", "crashes", "freeze", "freezes", "frozen",
    "slow", "laggy", "lag", "glitch", "glitchy",
    "firmware", "update", "software",
    "inaccurate", "accuracy", "calibration", "calibrate",
    "noise", "noisy", "jitter",
    "screen", "display", "lcd", "flicker",
    "battery", "charge", "charging",
    "probe", "probes",
    "cheap", "junk", "garbage", "trash", "waste",
    "return", "returned", "refund",
    "broke", "broken", "dead", "defective", "faulty",
    "disappointed", "disappointing", "terrible", "horrible", "awful",
    "useless", "worthless",
    "problem", "issue", "issues", "problems",
    "doesn't work", "don't work", "not working", "stopped working",
    "wish", "should", "could have", "missing", "lacks", "lacking",
    "menu", "ui", "interface", "buttons", "button",
    "measure", "measurement", "measurements",
    "trigger", "triggering",
    "bandwidth", "sample rate", "sampling",
    "signal generator", "siggen", "awg",
    "multimeter", "dmm",
]

# Keywords that signal praise
PRAISE_KEYWORDS = [
    "love", "great", "amazing", "awesome", "excellent", "fantastic",
    "perfect", "impressed", "impressive", "best", "recommend",
    "worth", "value", "bargain", "deal",
    "accurate", "reliable", "solid", "sturdy",
    "upgrade", "improved", "better",
]

SEARCH_TERMS = [
    "FNIRSI 2C53T",
    "FNIRSI 2C53T review",
    "FNIRSI 2C53T problems",
    "FNIRSI 2C23T",
    "FNIRSI 2C23T review",
]


def api_get(endpoint, params):
    params["key"] = API_KEY
    url = f"{BASE_URL}/{endpoint}?{urlencode(params)}"
    req = Request(url)
    with urlopen(req) as resp:
        return json.loads(resp.read())


def search_videos(query, max_results=10):
    data = api_get("search", {
        "part": "snippet",
        "q": query,
        "type": "video",
        "maxResults": max_results,
        "order": "viewCount",
    })
    return data.get("items", [])


def get_comments(video_id, max_pages=3):
    """Fetch top-level comments for a video. Returns list of comment dicts."""
    comments = []
    page_token = None

    for _ in range(max_pages):
        params = {
            "part": "snippet",
            "videoId": video_id,
            "maxResults": 100,
            "order": "relevance",
            "textFormat": "plainText",
        }
        if page_token:
            params["pageToken"] = page_token

        try:
            data = api_get("commentThreads", params)
        except Exception as e:
            # Comments may be disabled
            break

        for item in data.get("items", []):
            snippet = item["snippet"]["topLevelComment"]["snippet"]
            comments.append({
                "text": snippet["textOriginal"],
                "likes": snippet.get("likeCount", 0),
                "author": snippet["authorDisplayName"],
                "date": snippet["publishedAt"][:10],
            })

        page_token = data.get("nextPageToken")
        if not page_token:
            break

    return comments


def classify_comment(text):
    """Classify a comment as complaint, praise, question, or neutral."""
    lower = text.lower()
    complaint_hits = [kw for kw in COMPLAINT_KEYWORDS if kw in lower]
    praise_hits = [kw for kw in PRAISE_KEYWORDS if kw in lower]
    is_question = "?" in text

    # Weight by keyword count
    complaint_score = len(complaint_hits)
    praise_score = len(praise_hits)

    # Negative modifiers boost complaint score
    negative_phrases = ["doesn't", "don't", "not ", "can't", "won't", "no "]
    for phrase in negative_phrases:
        if phrase in lower:
            complaint_score += 1

    if complaint_score > praise_score and complaint_score >= 2:
        return "complaint", complaint_hits
    elif praise_score > complaint_score and praise_score >= 1:
        return "praise", praise_hits
    elif is_question:
        return "question", []
    else:
        return "neutral", []


def extract_themes(comments):
    """Group complaints into themes."""
    themes = {
        "firmware/software": [],
        "accuracy/calibration": [],
        "display/UI": [],
        "build quality": [],
        "probes/accessories": [],
        "battery": [],
        "noise/signal quality": [],
        "triggering": [],
        "measurements": [],
        "signal generator": [],
        "general negative": [],
        "feature requests": [],
    }

    theme_keywords = {
        "firmware/software": ["firmware", "update", "software", "bug", "crash", "freeze", "slow", "laggy", "glitch"],
        "accuracy/calibration": ["inaccurate", "accuracy", "calibration", "calibrate"],
        "display/UI": ["screen", "display", "lcd", "flicker", "menu", "ui", "interface", "buttons", "button"],
        "build quality": ["cheap", "broke", "broken", "dead", "defective", "faulty", "junk", "garbage"],
        "probes/accessories": ["probe", "probes"],
        "battery": ["battery", "charge", "charging"],
        "noise/signal quality": ["noise", "noisy", "jitter"],
        "triggering": ["trigger", "triggering"],
        "measurements": ["measure", "measurement", "measurements", "multimeter", "dmm"],
        "signal generator": ["signal generator", "siggen", "awg"],
        "feature requests": ["wish", "should", "could have", "missing", "lacks", "lacking"],
    }

    for comment in comments:
        lower = comment["text"].lower()
        matched = False
        for theme, keywords in theme_keywords.items():
            if any(kw in lower for kw in keywords):
                themes[theme].append(comment)
                matched = True
        if not matched:
            cat, _ = classify_comment(comment["text"])
            if cat == "complaint":
                themes["general negative"].append(comment)

    return themes


def format_number(n):
    if n >= 1_000_000:
        return f"{n/1_000_000:.1f}M"
    if n >= 1_000:
        return f"{n/1_000:.1f}K"
    return str(n)


def main():
    parser = argparse.ArgumentParser(description="Analyze YouTube comments for FNIRSI feedback")
    parser.add_argument("--terms", nargs="+", help="Custom search terms")
    parser.add_argument("--video-id", nargs="+", help="Specific video IDs to analyze")
    parser.add_argument("--max-videos", type=int, default=5, help="Videos per search term (default: 5)")
    parser.add_argument("--max-pages", type=int, default=3, help="Comment pages per video (default: 3, ~300 comments)")
    parser.add_argument("--json", action="store_true", help="Save raw data to JSON")
    args = parser.parse_args()

    if not API_KEY:
        print("Error: Set YOUTUBE_API_KEY environment variable or add to scripts/.env")
        sys.exit(1)

    print(f"{'='*70}")
    print(f"  FNIRSI YouTube Comment Analysis — {datetime.now().strftime('%Y-%m-%d')}")
    print(f"{'='*70}\n")

    # Collect video IDs
    video_map = {}  # id -> {title, channel}

    if args.video_id:
        # Get info for specified videos
        for vid in args.video_id:
            video_map[vid] = {"title": vid, "channel": ""}
    else:
        terms = args.terms or SEARCH_TERMS
        for term in terms:
            print(f"Searching: \"{term}\"...")
            results = search_videos(term, args.max_videos)
            for item in results:
                vid = item["id"]["videoId"]
                if vid not in video_map:
                    video_map[vid] = {
                        "title": item["snippet"]["title"],
                        "channel": item["snippet"]["channelTitle"],
                    }
            print(f"  Found {len(results)} videos")

    print(f"\nTotal unique videos to analyze: {len(video_map)}\n")

    # Fetch comments
    all_comments = []
    for vid, info in video_map.items():
        title_short = info["title"][:60]
        print(f"Fetching comments: {title_short}...")
        comments = get_comments(vid, args.max_pages)
        for c in comments:
            c["video_id"] = vid
            c["video_title"] = info["title"]
        all_comments.extend(comments)
        print(f"  Got {len(comments)} comments")

    print(f"\nTotal comments collected: {len(all_comments)}\n")

    # Classify
    complaints = []
    praises = []
    questions = []
    neutrals = []

    for comment in all_comments:
        cat, keywords = classify_comment(comment["text"])
        comment["category"] = cat
        comment["matched_keywords"] = keywords
        if cat == "complaint":
            complaints.append(comment)
        elif cat == "praise":
            praises.append(comment)
        elif cat == "question":
            questions.append(comment)
        else:
            neutrals.append(comment)

    # Sort complaints by likes (most-liked complaints = most common pain points)
    complaints.sort(key=lambda c: c["likes"], reverse=True)
    praises.sort(key=lambda c: c["likes"], reverse=True)

    # Theme analysis
    themes = extract_themes(complaints)

    # Output
    print(f"{'='*70}")
    print(f"  SENTIMENT BREAKDOWN")
    print(f"{'='*70}")
    total = len(all_comments)
    print(f"  Complaints:  {len(complaints):>4} ({len(complaints)*100//total}%)")
    print(f"  Praise:      {len(praises):>4} ({len(praises)*100//total}%)")
    print(f"  Questions:   {len(questions):>4} ({len(questions)*100//total}%)")
    print(f"  Neutral:     {len(neutrals):>4} ({len(neutrals)*100//total}%)")

    print(f"\n{'='*70}")
    print(f"  COMPLAINT THEMES (ranked by frequency)")
    print(f"{'='*70}")
    sorted_themes = sorted(themes.items(), key=lambda x: len(x[1]), reverse=True)
    for theme, comments_list in sorted_themes:
        if comments_list:
            total_likes = sum(c["likes"] for c in comments_list)
            print(f"\n  [{len(comments_list)} comments, {total_likes} total likes] {theme.upper()}")
            # Show top 3 most-liked comments for this theme
            top = sorted(comments_list, key=lambda c: c["likes"], reverse=True)[:3]
            for c in top:
                text = c["text"].replace("\n", " ")[:120]
                like_str = f"[{c['likes']} likes]" if c["likes"] > 0 else ""
                print(f"    {like_str} \"{text}\"")

    print(f"\n{'='*70}")
    print(f"  TOP 20 MOST-LIKED COMPLAINTS (pain points)")
    print(f"{'='*70}")
    for i, c in enumerate(complaints[:20], 1):
        text = c["text"].replace("\n", " ")[:100]
        print(f"\n  {i:2}. [{c['likes']} likes] \"{text}\"")
        print(f"      Keywords: {', '.join(c['matched_keywords'])}")
        print(f"      Video: {c['video_title'][:50]}")

    print(f"\n{'='*70}")
    print(f"  TOP 10 MOST-LIKED PRAISE (what people love)")
    print(f"{'='*70}")
    for i, c in enumerate(praises[:10], 1):
        text = c["text"].replace("\n", " ")[:100]
        print(f"\n  {i:2}. [{c['likes']} likes] \"{text}\"")

    # Keyword frequency in complaints
    print(f"\n{'='*70}")
    print(f"  MOST COMMON COMPLAINT KEYWORDS")
    print(f"{'='*70}")
    all_kw = []
    for c in complaints:
        all_kw.extend(c["matched_keywords"])
    for kw, count in Counter(all_kw).most_common(25):
        bar = "#" * min(count, 40)
        print(f"  {kw:<20} {count:>4}  {bar}")

    # Feature requests / wishes
    wishes = [c for c in all_comments if any(w in c["text"].lower() for w in ["wish", "should have", "missing", "need", "please add", "hope they"])]
    if wishes:
        wishes.sort(key=lambda c: c["likes"], reverse=True)
        print(f"\n{'='*70}")
        print(f"  FEATURE REQUESTS / WISHES (top 10)")
        print(f"{'='*70}")
        for i, c in enumerate(wishes[:10], 1):
            text = c["text"].replace("\n", " ")[:120]
            print(f"\n  {i:2}. [{c['likes']} likes] \"{text}\"")

    if args.json:
        output = {
            "date": datetime.now().isoformat(),
            "total_comments": len(all_comments),
            "complaints": complaints,
            "praises": praises,
            "questions": questions,
            "themes": {k: v for k, v in sorted_themes if v},
        }
        out_path = "youtube_comments_results.json"
        with open(out_path, "w") as f:
            json.dump(output, f, indent=2, default=str)
        print(f"\nRaw data saved to {out_path}")

    # Actionable summary
    print(f"\n{'='*70}")
    print(f"  ACTIONABLE SUMMARY FOR FIRMWARE & CONTENT")
    print(f"{'='*70}")
    print(f"""
  Based on {len(all_comments)} comments across {len(video_map)} videos:

  FIRMWARE PRIORITIES (fix these = content gold):
  - Look at the complaint themes above ranked by frequency
  - Most-liked complaints show the biggest pain points
  - Feature requests show what people want added

  CONTENT IDEAS (each complaint = a potential Short):
  - "I fixed the [top complaint] on the FNIRSI 2C53T"
  - "The FNIRSI 2C53T [issue] is gone with custom firmware"
  - Before/after comparisons for each fix

  AFFILIATE ANGLE:
  - Comments asking "should I buy this?" or "is it worth it?" = your audience
  - Answer: "Yes, especially with the custom firmware" + affiliate link
""")


if __name__ == "__main__":
    main()
