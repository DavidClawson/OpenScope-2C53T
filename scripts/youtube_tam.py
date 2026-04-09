#!/usr/bin/env python3
"""
YouTube TAM Research Tool
Searches YouTube for FNIRSI oscilloscope content and aggregates view counts
to help quantify the total addressable market.

Usage:
    python3 youtube_tam.py          # loads key from .env automatically
    YOUTUBE_API_KEY="..." python3 youtube_tam.py  # or pass via env

Get a free API key at: https://console.cloud.google.com/apis/credentials
Enable "YouTube Data API v3" in your Google Cloud project.
Free tier: 10,000 units/day (search=100 units, videos.list=1 unit)
"""

import os
import sys
import json
import argparse
from pathlib import Path
from urllib.request import urlopen, Request
from urllib.parse import urlencode
from datetime import datetime


def load_env():
    """Load variables from .env file next to this script."""
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

SEARCH_TERMS = [
    "FNIRSI 2C53T",
    "FNIRSI 2C53T review",
    "FNIRSI 2C53T oscilloscope",
    "FNIRSI 2C53T teardown",
    "FNIRSI 2C53T firmware",
    "FNIRSI oscilloscope",
    "FNIRSI multimeter oscilloscope",
    "cheap oscilloscope 2024",
    "cheap oscilloscope 2025",
    "handheld oscilloscope review",
    "budget oscilloscope",
    "FNIRSI 1014D",
    "FNIRSI 1013D",
    "FNIRSI DPOX180H",
]


def api_get(endpoint, params):
    params["key"] = API_KEY
    url = f"{BASE_URL}/{endpoint}?{urlencode(params)}"
    req = Request(url)
    with urlopen(req) as resp:
        return json.loads(resp.read())


def search_videos(query, max_results=20):
    """Search YouTube and return video IDs + basic info."""
    data = api_get("search", {
        "part": "snippet",
        "q": query,
        "type": "video",
        "maxResults": max_results,
        "order": "viewCount",
    })
    return data.get("items", [])


def get_video_stats(video_ids):
    """Get view counts and other stats for a batch of video IDs."""
    if not video_ids:
        return []
    data = api_get("videos", {
        "part": "statistics,snippet,contentDetails",
        "id": ",".join(video_ids),
    })
    return data.get("items", [])


def format_number(n):
    if n >= 1_000_000:
        return f"{n/1_000_000:.1f}M"
    if n >= 1_000:
        return f"{n/1_000:.1f}K"
    return str(n)


def analyze_term(query, max_results=20):
    """Search a term and return aggregated stats."""
    results = search_videos(query, max_results)
    if not results:
        return None

    video_ids = [item["id"]["videoId"] for item in results]
    stats = get_video_stats(video_ids)

    videos = []
    for item in stats:
        s = item["statistics"]
        videos.append({
            "title": item["snippet"]["title"],
            "channel": item["snippet"]["channelTitle"],
            "published": item["snippet"]["publishedAt"][:10],
            "views": int(s.get("viewCount", 0)),
            "likes": int(s.get("likeCount", 0)),
            "comments": int(s.get("commentCount", 0)),
            "video_id": item["id"],
        })

    videos.sort(key=lambda v: v["views"], reverse=True)

    total_views = sum(v["views"] for v in videos)
    avg_views = total_views // len(videos) if videos else 0

    return {
        "query": query,
        "video_count": len(videos),
        "total_views": total_views,
        "avg_views": avg_views,
        "top_views": videos[0]["views"] if videos else 0,
        "videos": videos,
    }


def main():
    parser = argparse.ArgumentParser(description="YouTube TAM research for FNIRSI oscilloscopes")
    parser.add_argument("--terms", nargs="+", help="Custom search terms (default: built-in list)")
    parser.add_argument("--max-results", type=int, default=20, help="Videos per search term (default: 20)")
    parser.add_argument("--json", action="store_true", help="Output raw JSON")
    parser.add_argument("--top", type=int, default=5, help="Show top N videos per term (default: 5)")
    args = parser.parse_args()

    if not API_KEY:
        print("Error: Set YOUTUBE_API_KEY environment variable")
        print("Get one at: https://console.cloud.google.com/apis/credentials")
        print("Enable 'YouTube Data API v3' in your project")
        sys.exit(1)

    terms = args.terms or SEARCH_TERMS
    all_results = []
    grand_total_views = 0
    unique_videos = {}

    print(f"{'='*70}")
    print(f"  FNIRSI YouTube TAM Research — {datetime.now().strftime('%Y-%m-%d')}")
    print(f"{'='*70}\n")

    for term in terms:
        print(f"Searching: \"{term}\"...")
        result = analyze_term(term, args.max_results)
        if not result:
            print(f"  No results found.\n")
            continue

        all_results.append(result)

        print(f"  Videos found: {result['video_count']}")
        print(f"  Total views:  {format_number(result['total_views'])}")
        print(f"  Avg views:    {format_number(result['avg_views'])}")
        print(f"  Top video:    {format_number(result['top_views'])} views")
        print()

        for v in result["videos"][:args.top]:
            print(f"    {format_number(v['views']):>7}  {v['published']}  {v['title'][:55]}")
            print(f"            {v['channel']}  https://youtube.com/watch?v={v['video_id']}")
        print()

        # Track unique videos across all searches
        for v in result["videos"]:
            if v["video_id"] not in unique_videos:
                unique_videos[v["video_id"]] = v

    # Summary
    grand_total_views = sum(v["views"] for v in unique_videos.values())
    top_channels = {}
    for v in unique_videos.values():
        ch = v["channel"]
        top_channels[ch] = top_channels.get(ch, 0) + v["views"]

    sorted_channels = sorted(top_channels.items(), key=lambda x: x[1], reverse=True)

    print(f"\n{'='*70}")
    print(f"  SUMMARY")
    print(f"{'='*70}")
    print(f"  Search terms analyzed:  {len(all_results)}")
    print(f"  Unique videos found:    {len(unique_videos)}")
    print(f"  Total views (deduped):  {format_number(grand_total_views)}")
    print(f"\n  Top channels by total views:")
    for ch, views in sorted_channels[:15]:
        print(f"    {format_number(views):>7}  {ch}")

    # Rough TAM estimate
    print(f"\n  --- Rough TAM Reasoning ---")
    print(f"  If {format_number(grand_total_views)} views across {len(unique_videos)} videos,")
    print(f"  and ~1-3% of viewers are potential buyers,")
    low = int(grand_total_views * 0.01)
    high = int(grand_total_views * 0.03)
    print(f"  estimated interested buyers: {format_number(low)} - {format_number(high)}")
    print(f"  At $75 avg sale, 4% affiliate commission ($3/sale):")
    print(f"  TAM value: ${low * 3:,} - ${high * 3:,} in potential affiliate revenue")
    print()

    if args.json:
        output = {
            "date": datetime.now().isoformat(),
            "results": all_results,
            "summary": {
                "unique_videos": len(unique_videos),
                "total_views": grand_total_views,
                "top_channels": sorted_channels[:15],
            }
        }
        with open("youtube_tam_results.json", "w") as f:
            json.dump(output, f, indent=2)
        print("Raw data saved to youtube_tam_results.json")


if __name__ == "__main__":
    main()
