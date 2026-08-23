#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>

struct Post
{
    std::string id;
    std::string subreddit;
    std::string author;
    std::string text;
    std::string created_utc;
};

struct FlaggedPost
{
    Post post;
    std::vector<std::string> matched_categories;
    std::vector<std::string> matched_terms;
    std::vector<std::string> benign_hits;
    double score=0.0;
};

std::map<std::string, std::vector<std::string>> buildTriggerTerms()
{
    std::map<std::string, std::vector<std::string>> terms;

    terms["grievance"]=
    {
        "ayoko na", "sawa na ako", "grabe silang lahat", "sinisiraan nila ako",
        "pinapahirapan nila ako", "walang nakikinig sa akin", "sila ang dahilan",
        "gagawin ko rin sa kanila", "hindi na ito patas", "sila ang may kasalanan"
    };

    terms["farewell_leakage"]=
    {
        "paalam na", "huling post ko", "wag nyo na akong hanapin",
        "di ko na kayang tiisin", "last message ko na to", "goodbye na sa lahat",
        "hindi na ako babalik", "wala nang bukas para sa akin"
    };

    terms["isolation"]=
    {
        "walang nakakaintindi sa akin", "mag-isa na lang ako", "wala akong kaibigan",
        "iniwan nila ako", "hindi ako kabilang", "lagi na lang akong nag-iisa"
    };

    return terms;
}

std::vector<std::string> buildBenignTerms()
{
    return
    {
        "chorus", "lyrics", "spotify", "ML rank", "grand finals", "PBA", "UAAP",
        "sinehan", "kwento lang", "movie ba to", "sabi sa kanta", "vibe check",
        "joke lang", "chika lang po", "para sa GF/BF drama", "trip lang"
    };
}

std::string toLower(const std::string& s)
{
    std::string out=s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return out;
}

bool contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle)!=std::string::npos;
}

std::vector<std::string> parseCsvLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes=false;
    for(size_t i=0; i<line.size(); ++i)
    {
        char c=line[i];
        if(inQuotes)
        {
            if(c=='"')
            {
                if(i+1<line.size() && line[i+1]=='"')
                {
                    field+='"';
                    ++i;
                }
                else
                {
                    inQuotes=false;
                }
            }
            else
            {
                field+=c;
            }
        }
        else
        {
            if(c=='"')
            {
                inQuotes=true;
            }
            else if(c==',')
            {
                fields.push_back(field);
                field.clear();
            }
            else
            {
                field+=c;
            }
        }
    }
    fields.push_back(field);
    return fields;
}

std::vector<Post> loadPosts(const std::string& path)
{
    std::vector<Post> posts;
    std::ifstream file(path);
    if(!file.is_open())
    {
        std::cerr<<"ERROR: could not open input file: "<<path<<"\n";
        return posts;
    }

    std::string line;
    bool header=true;
    while(std::getline(file, line))
    {
        if(line.empty())
        {
            continue;
        }
        if(header)
        {
            header=false;
            continue;
        }
        auto fields=parseCsvLine(line);
        if(fields.size()<5)
        {
            continue;
        }
        Post p;
        p.id=fields[0];
        p.subreddit=fields[1];
        p.author=fields[2];
        p.text=fields[3];
        p.created_utc=fields[4];
        posts.push_back(p);
    }
    return posts;
}

bool stage1_trigger(const Post& post,
                     const std::map<std::string, std::vector<std::string>>& terms,
                     std::vector<std::string>& matchedCategories,
                     std::vector<std::string>& matchedTerms)
{
    std::string lowerText=toLower(post.text);
    for(const auto& [category, wordlist]:terms)
    {
        for(const auto& term:wordlist)
        {
            if(contains(lowerText, toLower(term)))
            {
                matchedCategories.push_back(category);
                matchedTerms.push_back(term);
            }
        }
    }
    return !matchedTerms.empty();
}

double categoryWeight(const std::string& category)
{
    if(category=="farewell_leakage")
    {
        return 1.6;
    }
    if(category=="grievance")
    {
        return 1.0;
    }
    if(category=="isolation")
    {
        return 0.8;
    }
    return 0.5;
}

double stage2_score(const Post& post,
                     const std::vector<std::string>& matchedCategories,
                     const std::vector<std::string>& benignTerms,
                     std::vector<std::string>& benignHits)
{
    double score=0.0;
    for(const auto& cat:matchedCategories)
    {
        score+=categoryWeight(cat);
    }

    std::string lowerText=toLower(post.text);
    for(const auto& term:benignTerms)
    {
        if(contains(lowerText, toLower(term)))
        {
            benignHits.push_back(term);
            score-=1.2;
        }
    }

    return score;
}

void logDecision(const std::string& logPath, const FlaggedPost& fp,
                  const std::string& decision, const std::string& analyst)
{
    std::ofstream log(logPath, std::ios::app);
    std::time_t now=std::time(nullptr);
    std::tm* tmPtr=std::gmtime(&now);
    std::ostringstream ts;
    ts<<std::put_time(tmPtr, "%Y-%m-%dT%H:%M:%SZ");

    log<<ts.str()<<","
       <<fp.post.id<<","
       <<fp.post.subreddit<<","
       <<std::fixed<<std::setprecision(2)<<fp.score<<","
       <<decision<<","
       <<analyst<<"\n";
}

void stage3_review(std::vector<FlaggedPost>& shortlist, const std::string& logPath)
{
    if(shortlist.empty())
    {
        std::cout<<"\n[Stage 3] Shortlist is empty. Nothing reached analyst review.\n";
        return;
    }

    std::sort(shortlist.begin(), shortlist.end(),
              [](const FlaggedPost& a, const FlaggedPost& b){ return a.score>b.score; });

    std::cout<<"\n[Stage 3] Analyst Review -- "<<shortlist.size()
             <<" item(s) shortlisted.\n";
    std::cout<<"Enter analyst name/ID for this session: ";
    std::string analyst;
    std::getline(std::cin, analyst);
    if(analyst.empty())
    {
        analyst="unspecified";
    }

    std::ifstream check(logPath);
    bool needsHeader=!check.good();
    check.close();
    if(needsHeader)
    {
        std::ofstream log(logPath);
        log<<"timestamp_utc,post_id,subreddit,score,decision,analyst\n";
    }

    int idx=1;
    for(auto& fp:shortlist)
    {
        std::cout<<"\n--------------------------------------------\n";
        std::cout<<"["<<idx<<"/"<<shortlist.size()<<"] "
                 <<"Post ID: "<<fp.post.id
                 <<"  |  r/"<<fp.post.subreddit
                 <<"  |  Score: "<<std::fixed<<std::setprecision(2)<<fp.score<<"\n";

        std::cout<<"Matched categories: ";
        for(size_t i=0; i<fp.matched_categories.size(); ++i)
        {
            std::cout<<fp.matched_categories[i];
            if(i+1<fp.matched_categories.size())
            {
                std::cout<<", ";
            }
        }
        std::cout<<"\n";

        if(!fp.benign_hits.empty())
        {
            std::cout<<"Benign-context hits (down-weighted): ";
            for(size_t i=0; i<fp.benign_hits.size(); ++i)
            {
                std::cout<<fp.benign_hits[i];
                if(i+1<fp.benign_hits.size())
                {
                    std::cout<<", ";
                }
            }
            std::cout<<"\n";
        }

        std::cout<<"Text: "<<fp.post.text<<"\n";
        std::cout<<"Decision [D]ismiss / [M]onitor / [E]scalate / [S]kip: ";

        std::string input;
        std::getline(std::cin, input);
        std::string decision="skip";
        if(!input.empty())
        {
            char c=std::tolower(input[0]);
            if(c=='d')
            {
                decision="dismiss";
            }
            else if(c=='m')
            {
                decision="monitor";
            }
            else if(c=='e')
            {
                decision="escalate";
            }
            else
            {
                decision="skip";
            }
        }

        if(decision!="skip")
        {
            logDecision(logPath, fp, decision, analyst);
            std::cout<<"-> Logged as: "<<decision<<"\n";
        }
        else
        {
            std::cout<<"-> Skipped (not logged).\n";
        }

        ++idx;
    }

    std::cout<<"\n[Stage 3] Review session complete. Decisions written to "
             <<logPath<<"\n";
}

int main(int argc, char* argv[])
{
    std::string inputPath="data/posts.csv";
    std::string logPath="data/analyst_decisions_log.csv";

    if(argc>1)
    {
        inputPath=argv[1];
    }
    if(argc>2)
    {
        logPath=argv[2];
    }

    std::cout<<"==================================================\n";
    std::cout<<" TIGMAMANUKAN -- Early Warning Analytics Layer\n";
    std::cout<<" C++ Terminal Prototype (Stages 1-3)\n";
    std::cout<<"==================================================\n";
    std::cout<<"Input file : "<<inputPath<<"\n";
    std::cout<<"Log file   : "<<logPath<<"\n";

    auto posts=loadPosts(inputPath);
    std::cout<<"\nLoaded "<<posts.size()<<" post(s) from input file.\n";
    if(posts.empty())
    {
        std::cerr<<"No posts loaded. Check the input path. Exiting.\n";
        return 1;
    }

    auto triggerTerms=buildTriggerTerms();
    auto benignTerms=buildBenignTerms();

    std::vector<FlaggedPost> stage1Candidates;

    for(const auto& post:posts)
    {
        std::vector<std::string> matchedCategories, matchedTerms;
        if(stage1_trigger(post, triggerTerms, matchedCategories, matchedTerms))
        {
            FlaggedPost fp;
            fp.post=post;
            fp.matched_categories=matchedCategories;
            fp.matched_terms=matchedTerms;
            stage1Candidates.push_back(fp);
        }
    }

    std::cout<<"\n[Stage 1] Keyword/Term Trigger\n";
    std::cout<<"  Raw posts scanned : "<<posts.size()<<"\n";
    std::cout<<"  Stage 1 candidates: "<<stage1Candidates.size()<<"\n";

    std::vector<FlaggedPost> shortlist;
    const double SHORTLIST_THRESHOLD=0.5;

    for(auto& fp:stage1Candidates)
    {
        std::vector<std::string> benignHits;
        double score=stage2_score(fp.post, fp.matched_categories, benignTerms, benignHits);
        fp.score=score;
        fp.benign_hits=benignHits;
        if(score>=SHORTLIST_THRESHOLD)
        {
            shortlist.push_back(fp);
        }
    }

    std::cout<<"\n[Stage 2] Context Scoring\n";
    std::cout<<"  Candidates scored     : "<<stage1Candidates.size()<<"\n";
    std::cout<<"  Passed threshold (>= "<<SHORTLIST_THRESHOLD<<"): "
             <<shortlist.size()<<"\n";

    double reductionPct=posts.empty() ? 0.0:
        100.0*(1.0-(double)shortlist.size()/(double)posts.size());
    std::cout<<"  Overall volume reduction: "<<std::fixed<<std::setprecision(1)
             <<reductionPct<<"% (raw -> Stage 3 shortlist)\n";

    stage3_review(shortlist, logPath);

    std::cout<<"\nRun complete.\n";
    return 0;
}
