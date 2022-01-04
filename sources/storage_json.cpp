// Copyright 2021 Dan10022002 dan10022002@mail.ru

#include "storage_json.h"
#include <iostream>
#include <fstream>
#include <exception>

storage_json::storage_json(const std::string& name): _name(name) {}

nlohmann::json storage_json::get_storage() const {return _storage;}

void storage_json::load() {
  try {
    std::ifstream file(_name);
    file >> _storage;
  }
  catch (const std::exception& e)
  {
    std::cout << e.what() << "\n";
  }
}