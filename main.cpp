#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <filesystem>

constexpr size_t MAX_INDEX_LEN = 64;
constexpr char DATA_FILE[] = "data.bin";

#pragma pack(push, 1)
struct Record {
    char index[MAX_INDEX_LEN];
    int32_t value;
    bool active;

    Record() : value(0), active(false) {
        memset(index, 0, MAX_INDEX_LEN);
    }

    Record(const std::string& idx, int32_t val) : value(val), active(true) {
        memset(index, 0, MAX_INDEX_LEN);
        strncpy(index, idx.c_str(), MAX_INDEX_LEN - 1);
    }

    bool matches(const std::string& idx, int32_t val) const {
        return active && (strcmp(index, idx.c_str()) == 0) && (value == val);
    }

    bool matches_index(const std::string& idx) const {
        return active && (strcmp(index, idx.c_str()) == 0);
    }
};
#pragma pack(pop)

class FileStorage {
private:
    std::streampos get_file_size() {
        std::ifstream in(DATA_FILE, std::ios::binary | std::ios::ate);
        if (!in) return 0;
        return in.tellg();
    }

public:
    FileStorage() {
        // Ensure file exists
        std::ofstream out(DATA_FILE, std::ios::binary | std::ios::app);
    }

    void insert(const std::string& index, int32_t value) {
        // Check if already exists
        std::ifstream in(DATA_FILE, std::ios::binary);
        Record rec;
        while (in.read(reinterpret_cast<char*>(&rec), sizeof(Record))) {
            if (rec.matches(index, value)) {
                // Already exists, do nothing
                return;
            }
        }
        in.close();

        // Append new record
        std::ofstream out(DATA_FILE, std::ios::binary | std::ios::app);
        Record new_rec(index, value);
        out.write(reinterpret_cast<const char*>(&new_rec), sizeof(Record));
    }

    void remove(const std::string& index, int32_t value) {
        std::fstream file(DATA_FILE, std::ios::binary | std::ios::in | std::ios::out);
        if (!file) {
            // File doesn't exist, nothing to delete
            return;
        }

        Record rec;
        while (file.read(reinterpret_cast<char*>(&rec), sizeof(Record))) {
            if (rec.matches(index, value)) {
                // Mark as inactive
                rec.active = false;
                std::streampos pos = file.tellg();
                pos -= static_cast<std::streamoff>(sizeof(Record));
                file.seekp(pos);
                file.write(reinterpret_cast<const char*>(&rec), sizeof(Record));
                file.flush();
                return;
            }
        }
    }

    std::vector<int32_t> find(const std::string& index) {
        std::vector<int32_t> values;
        std::ifstream in(DATA_FILE, std::ios::binary);
        Record rec;

        while (in.read(reinterpret_cast<char*>(&rec), sizeof(Record))) {
            if (rec.matches_index(index)) {
                values.push_back(rec.value);
            }
        }

        std::sort(values.begin(), values.end());
        return values;
    }
};

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    FileStorage storage;
    int n;
    std::cin >> n;
    std::cin.ignore(); // Ignore newline after n

    for (int i = 0; i < n; ++i) {
        std::string line;
        std::getline(std::cin, line);

        if (line.empty()) continue;

        if (line.rfind("insert ", 0) == 0) {
            size_t space1 = line.find(' ', 7);
            if (space1 == std::string::npos) continue;

            std::string index = line.substr(7, space1 - 7);
            std::string value_str = line.substr(space1 + 1);
            int32_t value = std::stoi(value_str);

            storage.insert(index, value);
        }
        else if (line.rfind("delete ", 0) == 0) {
            size_t space1 = line.find(' ', 7);
            if (space1 == std::string::npos) continue;

            std::string index = line.substr(7, space1 - 7);
            std::string value_str = line.substr(space1 + 1);
            int32_t value = std::stoi(value_str);

            storage.remove(index, value);
        }
        else if (line.rfind("find ", 0) == 0) {
            std::string index = line.substr(5);

            std::vector<int32_t> values = storage.find(index);

            if (values.empty()) {
                std::cout << "null\n";
            } else {
                for (size_t j = 0; j < values.size(); ++j) {
                    if (j > 0) std::cout << ' ';
                    std::cout << values[j];
                }
                std::cout << '\n';
            }
        }
    }

    return 0;
}