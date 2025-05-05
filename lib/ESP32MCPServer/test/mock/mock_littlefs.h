#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

// Mock File class for testing
class MockFile {
public:
    MockFile(const std::string& name, const std::string& content) 
        : name_(name), content_(content), position_(0) {}
    
    size_t size() const { return content_.size(); }
    const char* name() const { return name_.c_str(); }
    
    size_t read(uint8_t* buf, size_t size) {
        if (position_ >= content_.size()) return 0;
        size_t available = content_.size() - position_;
        size_t to_read = std::min(size, available);
        memcpy(buf, content_.c_str() + position_, to_read);
        position_ += to_read;
        return to_read;
    }
    
    size_t write(const uint8_t* buf, size_t size) {
        std::string new_content((char*)buf, size);
        if (position_ >= content_.size()) {
            content_ += new_content;
        } else {
            content_.replace(position_, size, new_content);
        }
        position_ += size;
        return size;
    }
    
    void seek(size_t position) {
        position_ = std::min(position, content_.size());
    }

private:
    std::string name_;
    std::string content_;
    size_t position_;
};

// Mock LittleFS class for testing
class MockLittleFS {
public:
    bool begin(bool formatOnFail = false) {
        if (!formatted_ && formatOnFail) {
            format();
        }
        return formatted_;
    }
    
    void format() {
        files_.clear();
        formatted_ = true;
    }
    
    bool exists(const char* path) {
        return files_.find(path) != files_.end();
    }
    
    std::shared_ptr<MockFile> open(const char* path, const char* mode = "r") {
        if (strcmp(mode, "w") == 0) {
            auto file = std::make_shared<MockFile>(path, "");
            files_[path] = file;
            return file;
        }
        
        auto it = files_.find(path);
        if (it != files_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    bool remove(const char* path) {
        return files_.erase(path) > 0;
    }
    
    bool rename(const char* from, const char* to) {
        auto it = files_.find(from);
        if (it != files_.end()) {
            files_[to] = it->second;
            files_.erase(it);
            return true;
        }
        return false;
    }
    
    std::vector<std::string> listFiles() const {
        std::vector<std::string> result;
        for (const auto& pair : files_) {
            result.push_back(pair.first);
        }
        return result;
    }

private:
    bool formatted_ = false;
    std::map<std::string, std::shared_ptr<MockFile>> files_;
};

extern MockLittleFS LittleFS;