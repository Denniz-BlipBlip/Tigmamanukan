import argparse
import csv
import hashlib
import os
import sys

try:
    import praw
except ImportError:
    print("praw not installed. Run: pip install praw")
    sys.exit(1)
DEFAULT_SUBREDDITS = [
    "Philippines",
    "CasualPH",
    "adultingph",
    "phinvest",
    "peyups",
]

OUTPUT_PATH = os.path.join(os.path.dirname(__file__), "data", "posts.csv")


def hash_author(name: str) -> str:
    if not name or name == "[deleted]":
        return "anon_deleted"
    return "anon_" + hashlib.sha256(name.encode("utf-8")).hexdigest()[:12]


def get_reddit_client() -> "praw.Reddit":
    client_id = os.environ.get("REDDIT_CLIENT_ID")
    client_secret = os.environ.get("REDDIT_CLIENT_SECRET")
    user_agent = os.environ.get("REDDIT_USER_AGENT", "tigmamanukan-poc/0.1 by u/your_username")

    if not client_id or not client_secret:
        print(
            "Missing credentials. Set these environment variables first:\n"
            "  REDDIT_CLIENT_ID\n"
            "  REDDIT_CLIENT_SECRET\n"
            "  REDDIT_USER_AGENT (optional but recommended)\n\n"
            "See README.md for how to create a free Reddit 'script' app."
        )
        sys.exit(1)

    return praw.Reddit(
        client_id=client_id,
        client_secret=client_secret,
        user_agent=user_agent,
    )


def fetch_posts(reddit: "praw.Reddit", subreddits, limit_per_sub: int):
    rows = []
    seen_ids = set()

    for sub_name in subreddits:
        print(f"Fetching r/{sub_name} (up to {limit_per_sub} posts)...")
        try:
            subreddit = reddit.subreddit(sub_name)
            for submission in subreddit.new(limit=limit_per_sub):
                if submission.id in seen_ids:
                    continue
                seen_ids.add(submission.id)

                text = (submission.title or "").strip()
                if submission.selftext:
                    text = f"{text} - {submission.selftext.strip()}"
                text = text.replace("\n", " ").replace("\r", " ")[:2000]

                if not text:
                    continue

                rows.append({
                    "id": submission.id,
                    "subreddit": sub_name,
                    "author": hash_author(str(submission.author)),
                    "text": text,
                    "created_utc": submission.created_utc,
                })
        except Exception as e:
            print(f"  Skipped r/{sub_name}: {e}")

    return rows


def write_csv(rows, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["id", "subreddit", "author", "text", "created_utc"])
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
    print(f"\nWrote {len(rows)} posts to {path}")


def main():
    parser = argparse.ArgumentParser(description="Fetch public Reddit posts for Tigmamanukan.")
    parser.add_argument("--limit", type=int, default=100, help="Posts to fetch per subreddit")
    parser.add_argument("--subreddits", nargs="*", default=DEFAULT_SUBREDDITS,
                         help="Override the default subreddit list")
    parser.add_argument("--output", default=OUTPUT_PATH, help="Output CSV path")
    args = parser.parse_args()

    reddit = get_reddit_client()
    rows = fetch_posts(reddit, args.subreddits, args.limit)

    if not rows:
        print("No posts fetched. Check your credentials and subreddit list.")
        sys.exit(1)

    write_csv(rows, args.output)
    print("Done. Now run the C++ engine against this file, e.g.:")
    print(f"  ./tigmamanukan {args.output}")


if __name__ == "__main__":
    main()
