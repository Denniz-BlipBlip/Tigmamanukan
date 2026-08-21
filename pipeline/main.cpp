#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "categories.hpp"

struct post
{
  std::string id,username,text,timestamp,dialect;
  std::vector<std::string> categories_hit;
  int risk_score=0;
  bool flagged=false;
};

static std::vector<std::string> parse_csv_line(const std::string& line)
{
  std::vector<std::string> fields;
  std::string field;
  bool in_quotes=false;
  for(size_t i=0;i<line.size();++i)
  {
    char c=line[i];
    if(c=='"'){in_quotes=!in_quotes;}
    else if(c==','&&!in_quotes)
    {
      fields.push_back(field);
      field.clear();
    }
    else field+=c;
  }
  fields.push_back(field);
  return fields;
}

static std::string to_lower(const std::string& s)
{
  std::string out=s;
  std::transform(out.begin(),out.end(),out.begin(),[](unsigned char c){return std::tolower(c);});
  return out;
}

static bool contains(const std::string& haystack,const std::string& needle)
{
  return haystack.find(needle)!=std::string::npos;
}

std::vector<post> load_posts(const std::string& path)
{
  std::vector<post> posts;
  std::ifstream file(path);
  if(!file.is_open())
  {
    std::cerr<<"Could not open "<<path<<"\n";
    return posts;
  }
  std::string line;
  std::getline(file,line); // skip header
  while(std::getline(file,line))
  {
    if(line.empty()) continue;
    auto fields=parse_csv_line(line);
    if(fields.size()<5) continue;
    post p;
    p.id=fields[0];
    p.username=fields[1];
    p.text=fields[2];
    p.timestamp=fields[3];
    p.dialect=fields[4];
    posts.push_back(p);
  }
  return posts;
}

// Stage 1: keyword/term match
void stage1_keyword_match(post& p)
{
  std::string lower_text=to_lower(p.text);
  for(const auto& [category,terms]:categories)
  {
    for(const auto& term:terms)
    {
      if(contains(lower_text,to_lower(term)))
      {
        if(std::find(p.categories_hit.begin(),p.categories_hit.end(),category)==p.categories_hit.end())
          p.categories_hit.push_back(category);
        break; // one hit per category is enough
      }
    }
  }
  p.flagged=!p.categories_hit.empty();
}

// Stage 2: lightweight context scoring
void stage2_context_score(post& p)
{
  if(!p.flagged)
  {
    p.risk_score=0;
    return;
  }
  int score=static_cast<int>(p.categories_hit.size());
  std::string lower_text=to_lower(p.text);
  for(const auto& hint:benign_context_hints)
  {
    if(contains(lower_text,to_lower(hint)))
    {
      score-=2;
      break;
    }
  }
  p.risk_score=std::max(score,0);
}

std::string join_categories(const std::vector<std::string>& cats)
{
  std::string out;
  for(size_t i=0;i<cats.size();++i)
  {
    out+=cats[i];
    if(i+1<cats.size()) out+=", ";
  }
  return out;
}

// Escapes text for safe embedding inside HTML
std::string html_escape(const std::string& s)
{
  std::string out;
  for(char c:s)
  {
    switch(c)
    {
      case '&': out+="&amp;"; break;
      case '<': out+="&lt;"; break;
      case '>': out+="&gt;"; break;
      case '"': out+="&quot;"; break;
      default: out+=c;
    }
  }
  return out;
}

void write_dashboard(const std::vector<post>& posts,const std::string& out_path)
{
  int total=static_cast<int>(posts.size());
  int stage1_count=0;
  for(const auto& p:posts) if(p.flagged) stage1_count++;

  std::vector<post> shortlist;
  for(const auto& p:posts) if(p.risk_score>=1) shortlist.push_back(p);
  std::sort(shortlist.begin(),shortlist.end(),[](const post& a,const post& b){return a.risk_score>b.risk_score;});

  std::ofstream out(out_path);

  // Plain HTML only - no <style>, no <script>.
  out<<R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Sentinel PH - Analyst Dashboard (C++ Prototype)</title>
</head>
<body>

<h1>Sentinel PH &mdash; Analyst Review Dashboard</h1>
<p><i>C++ prototype. Stage 1 keyword trigger, then Stage 2 context scoring,
then Stage 3 human review below. Runs entirely on synthetic sample data
covering Tagalog, Cebuano, Ilocano, Hiligaynon, and Waray.</i></p>

<h2>Summary</h2>
<table border="1" cellpadding="6" cellspacing="0">
<tr><th>Total posts processed</th><th>Stage 1 flagged</th><th>Final shortlist (Stage 3)</th></tr>
<tr>
<td align="center">)HTML"<<total<<R"HTML(</td>
<td align="center">)HTML"<<stage1_count<<R"HTML(</td>
<td align="center">)HTML"<<static_cast<int>(shortlist.size())<<R"HTML(</td>
</tr>
</table>

<h2>Analyst Review Queue (risk score &ge; 1)</h2>
<table border="1" cellpadding="6" cellspacing="0">
<tr>
<th>User</th><th>Dialect</th><th>Post</th><th>Categories matched</th><th>Risk score</th><th>Analyst decision</th>
</tr>
)HTML";

  for(const auto& p:shortlist)
  {
    out<<"<tr>\n"
       <<"<td>@"<<html_escape(p.username)<<"</td>\n"
       <<"<td>"<<html_escape(p.dialect)<<"</td>\n"
       <<"<td>"<<html_escape(p.text)<<"</td>\n"
       <<"<td>"<<html_escape(join_categories(p.categories_hit))<<"</td>\n"
       <<"<td align=\"center\"><b>"<<p.risk_score<<"</b></td>\n"
       <<"<td>Pending / Dismiss / Monitor / Escalate</td>\n"
       <<"</tr>\n";
  }

  out<<R"HTML(</table>

<h2>Full Dataset (for transparency)</h2>
<table border="1" cellpadding="6" cellspacing="0">
<tr>
<th>User</th><th>Dialect</th><th>Post</th><th>Stage 1 flagged</th><th>Categories matched</th><th>Risk score</th>
</tr>
)HTML";

  for(const auto& p:posts)
  {
    out<<"<tr>\n"
       <<"<td>@"<<html_escape(p.username)<<"</td>\n"
       <<"<td>"<<html_escape(p.dialect)<<"</td>\n"
       <<"<td>"<<html_escape(p.text)<<"</td>\n"
       <<"<td align=\"center\">"<<(p.flagged?"Yes":"No")<<"</td>\n"
       <<"<td>"<<html_escape(join_categories(p.categories_hit))<<"</td>\n"
       <<"<td align=\"center\">"<<p.risk_score<<"</td>\n"
       <<"</tr>\n";
  }

  out<<R"HTML(</table>

<p><small>This is a non-functional prototype for demonstration purposes
only. It uses synthetic sample data and a simplified rule-\ based scoring
system covering five Philippine languages/dialects. No real accounts, real
posts, or real individuals are involved. This page uses no CSS and no
JavaScript &mdash; it is regenerated fresh by pipeline.cpp on every run, at
a fixed review threshold of risk score &ge; 1.</small></p>

</body>
</html>
)HTML";

  out.close();
}

int main(int argc,char* argv[])
{
  std::string csv_path=argc>1?argv[1]:"sample_posts.csv";
  auto posts=load_posts(csv_path);

  for(auto& p:posts)
  {
    stage1_keyword_match(p);
    stage2_context_score(p);
  }

  int stage1_count=0;
  for(const auto& p:posts) if(p.flagged) stage1_count++;

  std::cout<<"Total posts processed: "<<posts.size()<<"\n";
  std::cout<<"Stage 1 flagged: "<<stage1_count<<"\n\n";
  std::cout<<"Final shortlist (risk_score >= 1):\n";
  std::cout<<"----------------------------------------------------------\n";

  std::vector<post> shortlist;
  for(const auto& p:posts) if(p.risk_score>=1) shortlist.push_back(p);
  std::sort(shortlist.begin(),shortlist.end(),[](const post& a,const post& b){return a.risk_score>b.risk_score;});

  for(const auto& p:shortlist)
  {
    std::cout<<"@"<<p.username<<" [score="<<p.risk_score<<"] ("<<join_categories(p.categories_hit)<<")\n"
              <<"  "<<p.text<<"\n\n";
  }

  write_dashboard(posts,"dashboard.html");
  std::cout<<"Dashboard written to dashboard.html - open it in a browser.\n";

  return 0;
}
