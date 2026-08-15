#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "categories.hpp"

struct post  
{
  std::string id,username,dialect,timestamp;
  std::vector<std::string>categoriesHit;
  int risk_score=0;
  bool flagged=false;
};

static std::vector<std::string>parseCV(const std::string& line)
{
  std::vector<std::string>fields;
  std::string field;
  bool inQoutes=false;
  for(size_t i=0;i<line.size();++i)
  {
    char c=line[i];
    if(c=='"'){inQoutes=!inQoutes;}
    else if(c==','&&!inQoutes)
    {
      fields.push_back(field);
      field.clear();
    }
    else field+=c;
  }
}
