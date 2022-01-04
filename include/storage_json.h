// Copyright 2021 Dan10022002 dan10022002@mail.ru

#ifndef INCLUDE_STORAGE_JSON_H_
#define INCLUDE_STORAGE_JSON_H_

#include <nlohmann/json.hpp>
#include <string>

class storage_json {
  std::string _name;
  nlohmann::json _storage;
 public:
  explicit storage_json(const std::string& filename);
  nlohmann::json get_storage() const;
  void load();
};

#endif  // INCLUDE_STORAGE_JSON_H_