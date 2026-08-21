import pandas as pd
from categories import categories, benign_context_hints

def stage1_keyword_match(df):
    matches=[]
    categories_hit=[]
    for text in df["text"]:
        text_lower=text.lower()
        hits=[]
        for category, terms in categories.items():
            for term in terms:
                if term in text_lower:
                    hits.append(category)
                    break
        matches.append(len(hits)>0)
        categories_hit.append(hits)
    df=df.copy()
    df["stage1_flagged"]=matches
    df["categories_hit"]=categories_hit
    return df

def stage2_context_score(df):
    scores=[]
    for _, row in df.iterrows():
        if not row["stage1_flagged"]:
            scores.append(0)
            continue
        score=len(row["categories_hit"])
        text_lower=row["text"].lower()
        if any(hint in text_lower for hint in benign_context_hints):
            score-=2
        scores.append(max(score,0))
    df=df.copy()
    df["risk_score"]=scores
    return df

def run_pipeline(csv_path):
    df=pd.read_csv(csv_path)
    df=stage1_keyword_match(df)
    df=stage2_context_score(df)
    return df

def get_shortlist(df, min_score=1):
    return df[df["risk_score"]>=min_score].sort_values("risk_score", ascending=False)

if __name__=="__main__":
    result=run_pipeline("sample_posts.csv")
    print(f"Total posts processed: {len(result)}")
    print(f"Stage 1 flagged: {result['stage1_flagged'].sum()}")
    shortlist=get_shortlist(result)
    print(f"Final shortlist for human review: {len(shortlist)}\n")
    print(shortlist[["username","dialect","text","categories_hit","risk_score"]].to_string(index=False))
