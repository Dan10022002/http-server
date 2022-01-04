// Copyright 2021 Dan10022002 dan10022002@mail.ru

#include "suggestions_collection.h"

suggestions_collection::suggestions_collection() {_suggestions = {};}

nlohmann::json suggestions_collection::suggest(const std::string& text) {
  nlohmann::json result_suggestions;
  size_t counter = 0;
  for (size_t i = 0; i < _suggestions.size(); i++)
  {
    if (text == _suggestions[i].at("id"))
    {
      nlohmann::json tmp;
      tmp["text"] = _suggestions[i].at("name");
      tmp["position"] = counter;
      counter += 1;
      result_suggestions["suggestions"].push_back(tmp);
    }
  }
  return result_suggestions;
}

void suggestions_collection::update(nlohmann::json storage) {
  std::sort(storage.begin(), storage.end(), [] (nlohmann::json& lhs, nlohmann::json& rhs) {
    return lhs.at("cost") < rhs.at("cost");
  });
  _suggestions = storage;
}