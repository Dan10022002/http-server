// Copyright 2021 Dan10022002 dan10022002@mail.ru

#ifndef INCLUDE_SUGGESTIONS_COLLECTION_H_
#define INCLUDE_SUGGESTIONS_COLLECTION_H_
#include <nlohmann/json.hpp>
#include <string>

class suggestions_collection {
  nlohmann::json _suggestions;
 public:
  suggestions_collection();
  nlohmann::json suggest(const std::string& text);
  void update(nlohmann::json storage);
};

#endif  // INCLUDE_SUGGESTIONS_COLLECTION_H_