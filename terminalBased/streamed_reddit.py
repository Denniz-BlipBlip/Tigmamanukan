import argparse
import csv
import hashlib
import os
import sys
import time

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

BASE_DIR = os.path.dirname(__file__)
RAW_LOG_PATH = os.path.join(BASE_DIR, "data", "stream_raw_log.csv")
SHORTLIST_PATH = os.path.join(BASE_DIR, "data", "posts.csv")
TRIGGER_TERMS = {
    "grievance": [
        "ayoko na", "sawa na ako", "grabe silang lahat", "sinisiraan nila ako",
        "pinapahirapan nila ako", "walang nakikinig sa akin", "sila ang dahilan",
        "gagawin ko rin sa kanila", "hindi na ito patas", "sila ang may kasalanan",
    ],
    "farewell_leakage": [
        "paalam na", "huling post ko", "wag nyo na akong hanapin",
        "di ko na kayang tiisin", "last message ko na to", "goodbye na sa lahat",
        "hindi na ako babalik", "wala nang bukas para sa akin",
    ],
    "isolation": [
        "walang nakakaintindi sa akin", "mag-isa na lang ako", "wala akong kaibigan",
        "iniwan nila ako", "hindi ako kabilang", "lagi na lang akong nag-iisa",
    ],
}

BENIGN_TERMS = [
    "chorus", "lyrics", "spotify", "ml rank", "grand finals", "pba", "uaap",
    "sinehan", "kwento lang", "movie ba to", "sabi sa kanta", "vibe check",
    "joke lang", "chika lang po", "trip lang",
]

CATEGORY_WEIGHT = {"farewell_leakage": 1.6, "grievance": 1.0, "isolation": 0.8}
SHORTLIST_THRESHOLD = 0.5


def hash_author(name) -> str:
    name = str(name) if name else ""
    if not name or name == "None":
        return "anon_deleted"
    return "anon_" + hashlib.sha256(name.encode("utf-8")).hexdigest()[:12]


def score_text(text: str):
    lower = text.lower()
    matched_categories = []
    for cat, terms in TRIGGER_TERMS.items():
        for term in terms:
            if term in lower:
                matched_categories.append(cat)

    score = sum(CATEGORY_WEIGHT.get(c, 0.5) for c in matched_categories)

    benign_hits = [t for t in BENIGN_TERMS if t in lower]
    score -= 1.2 * len(benign_hits)

    return score, matched_categories, benign_hits


def ensure_csv_headers():
    os.makedirs(os.path.join(BASE_DIR, "data"), exist_ok=True)
    if not os.path.exists(RAW_LOG_PATH):
        with open(RAW_LOG_PATH, "w", newline="", encoding="utf-8") as f:
            csv.writer(f).writerow(["id", "subreddit", "author", "text", "created_utc", "score"])
    if not os.path.exists(SHORTLIST_PATH):
        with open(SHORTLIST_PATH, "w", newline="", encoding="utf-8") as f:
            csv.writer(f).writerow(["id", "subreddit", "author", "text", "created_utc"])


def append_row(path, row):
    with open(path, "a", newline="", encoding="utf-8") as f:
        csv.writer(f).writerow(row)


def get_reddit_client():
    client_id = os.environ.get("REDDIT_CLIENT_ID")
    client_secret = os.environ.get("REDDIT_CLIENT_SECRET")
    user_agent = os.environ.get("REDDIT_USER_AGENT", "tigmamanukan-poc/0.1 by u/your_username")

    if not client_id or not client_secret:
        print(
            "Missing credentials. Set these first:\n"
            "  export REDDIT_CLIENT_ID=...\n"
            "  export REDDIT_CLIENT_SECRET=...\n"
            "  export REDDIT_USER_AGENT=...\n"
            "See README.md for how to create a free Reddit script app."
        )
        sys.exit(1)

    return praw.Reddit(client_id=client_id, client_secret=client_secret, user_agent=user_agent)


def main():
    parser = argparse.ArgumentParser(description="Real-time Tigmamanukan stream.")
    parser.add_argument("--subreddits", nargs="*", default=DEFAULT_SUBREDDITS)
    args = parser.parse_args()

    ensure_csv_headers()
    reddit = get_reddit_client()
    multi = reddit.subreddit("+".join(args.subreddits))

    print(f"Streaming live submissions from: {', '.join(args.subreddits)}")
    print("Ctrl+C to stop. New shortlist hits print inline below.\n")

    try:
        for submission in multi.stream.submissions(skip_existing=True):
            text = (submission.title or "").strip()
            if submission.selftext:
                text = f"{text} - {submission.selftext.strip()}"
            text = text.replace("\n", " ").replace("\r", " ")[:2000]
            if not text:
                continue

            score, categories, benign_hits = score_text(text)
            author_hash = hash_author(submission.author)

            append_row(RAW_LOG_PATH, [
                submission.id, submission.subreddit.display_name, author_hash,
                text, submission.created_utc, f"{score:.2f}",
            ])

            if score >= SHORTLIST_THRESHOLD:
                append_row(SHORTLIST_PATH, [
                    submission.id, submission.subreddit.display_name, author_hash,
                    text, submission.created_utc,
                ])
                ts = time.strftime("%H:%M:%S", time.localtime())
                print(f"[{ts}] SHORTLISTED score={score:.2f} categories={categories} "
                      f"r/{submission.subreddit.display_name} id={submission.id}")
            else:
                pass

    except KeyboardInterrupt:
        print("\nStream stopped by user.")


if __name__ == "__main__":
    main()
