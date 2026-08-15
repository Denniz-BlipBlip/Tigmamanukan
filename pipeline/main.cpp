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
}
