import streamlit as st
import pandas as pd
from pipeline import run_pipeline, get_shortlist

st.set_page_config(page_title="Tigmamanukan - Analyst Dashboard", layout="wide")

st.title("Tigmamanukan — Analyst Review Dashboard (Prototype)")
st.caption("Runs on synthetic sample data covering Tagalog, Cebuano, Ilocano, Hiligaynon, and Waray. Stage 1 keyword trigger, Stage 2 context scoring, Stage 3 human review below.")

df=run_pipeline("sample_posts.csv")

col1, col2, col3=st.columns(3)
col1.metric("Total posts processed", len(df))
col2.metric("Stage 1 flagged", int(df["stage1_flagged"].sum()))
col3.metric("Final shortlist (Stage 3)", len(get_shortlist(df)))

st.divider()

threshold=st.slider("Minimum risk score to reach analyst review", 0, 3, 1)
shortlist=get_shortlist(df, min_score=threshold)

st.subheader(f"Analyst/ Review Queue ({len(shortlist)} items)")

if len(shortlist)==0:
    st.info("No items meet the current threshold.")
else:
    for _, row in shortlist.iterrows():
        with st.container(border=True):
            c1, c2=st.columns([4,1])
            with c1:
                st.markdown(f"**@{row['username']}** — {row['dialect']} — {row['timestamp']}")
                st.write(row["text"])
                st.caption(f"Categories matched: {', '.join(row['categories_hit'])}")
            with c2:
                st.metric("Risk score", row["risk_score"])
                st.selectbox("Analyst decision", ["Pending","Dismiss","Monitor","Escalate"], key=f"decision_{row['id']}")

st.divider()
st.subheader("Full Dataset (for transparency)")
st.dataframe(df[["username","dialect","text","stage1_flagged","categories_hit","risk_score"]], use_container_width=True)
st.caption("This is a non-functional prototype for demonstration purposes only. It uses synthetic sample data and a simplified rule-based scoring system. No real accounts, real posts, or real individuals are involved.")
