#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <cstring>
#include <algorithm>

// Mock File class
class MockFile {
public:
    MockFile() : position(0), mode(""), open(false) {}
    
    bool isOpen() const { return open; }
    
    size_t write(const uint8_t* buf, size_t size) {
        if (!open || (mode != "w" && mode != "w+" && mode != "a+" && mode != "r+")) {
            return 0;
        }
        
        if (mode == "a+" || mode == "a") {
            position = data.size();
        }
        
        if (position + size > data.size()) {
            data.resize(position + size);
        }
        
        std::copy(buf, buf + size, data.begin() + position);
        position += size;
        return size;
    }
    
    size_t write(uint8_t b) {
        return write(&b, 1);
    }
    
    size_t read(uint8_t* buf, size_t size) {
        if (!open || (mode != "r" && mode != "r+" && mode != "w+" && mode != "a+")) {
            return 0;
        }
        
        size_t available_size = std::min(size, data.size() - position);
        if (available_size > 0) {
            std::copy(data.begin() + position, 
                     data.begin() + position + available_size, 
                     buf);
            position += available_size;
        }
        return available_size;
    }
    
    int read() {
        if (!open || position >= data.size()) {
            return -1;
        }
        return data[position++];
    }
    
    bool seek(size_t pos) {
        if (!open || pos > data.size()) {
            return false;
        }
        position = pos;
        return true;
    }
    
    size_t position;
    size_t size() const { return data.size(); }
    bool available() { return position < data.size(); }
    void close() { open = false; }
    
    // For testing
    std::vector<uint8_t>& getData() { return data; }
    const std::string& getMode() const { return mode; }
    
private:
    friend class MockLittleFS;
    std::vector<uint8_t> data;
    std::string mode;
    bool open;
    
    bool openFile(const char* m) {
        mode = m;
        open = true;
        if (mode == "w" || mode == "w+") {
            data.clear();
            position = 0;
        } else if (mode == "a" || mode == "a+") {
            position = data.size();
        } else {
            position = 0;
        }
        return true;
    }
};

// Mock LittleFS class
class MockLittleFS {
public:
    MockLittleFS() : mounted(false) {}
    
    bool begin(bool formatOnFail = false) {
        mounted = true;
        return true;
    }
    
    void end() {
        mounted = false;
        files.clear();
    }
    
    MockFile open(const char* path, const char* mode) {
        if (!mounted) {
            MockFile f;
            return f;
        }
        
        MockFile file;
        if (strcmp(mode, "r") == 0 || strcmp(mode, "r+") == 0) {
            auto it = files.find(path);
            if (it == files.end()) {
                return file;
            }
            file = it->second;
        } else {
            file = files[path];
        }
        
        file.openFile(mode);
        return file;
    }
    
    bool exists(const char* path) {
        return files.find(path) != files.end();
    }
    
    bool remove(const char* path) {
        return files.erase(path) > 0;
    }
    
    bool rename(const char* pathFrom, const char* pathTo) {
        auto it = files.find(pathFrom);
        if (it == files.end()) {
            return false;
        }
        files[pathTo] = it->second;
        files.erase(it);
        return true;
    }
    
    bool mkdir(const char* path) {
        directories.insert(path);
        return true;
    }
    
    bool rmdir(const char* path) {
        return directories.erase(path) > 0;
    }
    
    // Testing helpers
    void reset() {
        files.clear();
        directories.clear();
        mounted = false;
    }
    
    size_t fileCount() const {
        return files.size();
    }
    
    bool isMounted() const {
        return mounted;
    }
    
    MockFile* getFile(const char* path) {
        auto it = files.find(path);
        if (it != files.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
private:
    std::map<std::string, MockFile> files;
    std::set<std::string> directories;
    bool mounted;
};

// Global instance for testing
extern MockLittleFS MockFS;

// Mock FS class for compatibility
class FS {
public:
    MockFile open(const char* path, const char* mode) {
        return MockFS.open(path, mode);
    }
    
    bool exists(const char* path) {
        return MockFS.exists(path);
    }
    
    bool remove(const char* path) {
        return MockFS.remove(path);
    }
    
    bool rename(const char* pathFrom, const char* pathTo) {
        return MockFS.rename(pathFrom, pathTo);
    }
    
    bool mkdir(const char* path) {
        return MockFS.mkdir(path);
    }
    
    bool rmdir(const char* path) {
        return MockFS.rmdir(path);
    }
};

// For test code using LittleFS directly
#define LittleFS MockFS