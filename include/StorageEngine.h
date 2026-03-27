#pragma once
#include <string>

class StorageEngine {

public:
    virtual ~StorageEngine() = default; 

    virtual std::string get(const std::string& key) const = 0; 
    virtual void set(const std::string& key, const std::string& value) = 0;
    virtual void del(const std::string& key) = 0;
    virtual bool exists(const std::string& key) const = 0;

};