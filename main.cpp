#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>

constexpr size_t MAX_INDEX_LEN = 64;
constexpr char DATA_FILE[] = "data.bin";
constexpr int TABLE_SIZE = 100003;  // Prime number, slightly larger than max records

#pragma pack(push, 1)
struct Slot {
    char index[MAX_INDEX_LEN];
    int32_t value;
    bool active;
    bool occupied;

    Slot() : value(0), active(false), occupied(false) {
        memset(index, 0, MAX_INDEX_LEN);
    }

    Slot(const std::string& idx, int32_t val) : value(val), active(true), occupied(true) {
        memset(index, 0, MAX_INDEX_LEN);
        strncpy(index, idx.c_str(), MAX_INDEX_LEN - 1);
    }

    bool matches(const std::string& idx, int32_t val) const {
        return occupied && active && (strcmp(index, idx.c_str()) == 0) && (value == val);
    }

    bool matches_index(const std::string& idx) const {
        return occupied && active && (strcmp(index, idx.c_str()) == 0);
    }

    bool is_empty() const {
        return !occupied;
    }
};
#pragma pack(pop)

class FileStorage {
private:
    std::fstream file;

    size_t hash(const std::string& str) const {
        size_t h = 5381;
        for (char c : str) {
            h = ((h << 5) + h) + c;  // h * 33 + c
        }
        return h % TABLE_SIZE;
    }

    std::streampos get_slot_offset(size_t slot_idx) const {
        return slot_idx * sizeof(Slot);
    }

    Slot read_slot(size_t slot_idx) {
        std::streampos pos = get_slot_offset(slot_idx);
        file.seekg(pos);
        Slot slot;
        file.read(reinterpret_cast<char*>(&slot), sizeof(Slot));
        return slot;
    }

    void write_slot(size_t slot_idx, const Slot& slot) {
        std::streampos pos = get_slot_offset(slot_idx);
        file.seekp(pos);
        file.write(reinterpret_cast<const char*>(&slot), sizeof(Slot));
    }

public:
    FileStorage() {
        // Open or create file
        file.open(DATA_FILE, std::ios::binary | std::ios::in | std::ios::out);
        if (!file) {
            // Create new file with initial size
            file.open(DATA_FILE, std::ios::binary | std::ios::out);
            // Initialize all slots as empty
            Slot empty_slot;
            for (int i = 0; i < TABLE_SIZE; i++) {
                file.write(reinterpret_cast<const char*>(&empty_slot), sizeof(Slot));
            }
            file.close();
            file.open(DATA_FILE, std::ios::binary | std::ios::in | std::ios::out);
        }
    }

    ~FileStorage() {
        if (file.is_open()) {
            file.close();
        }
    }

    void insert(const std::string& index, int32_t value) {
        size_t h = hash(index);
        size_t start = h;

        // First, check if already exists
        do {
            Slot slot = read_slot(h);
            if (slot.matches(index, value)) {
                return;  // Already exists
            }
            if (slot.is_empty()) {
                break;  // Will insert here
            }
            h = (h + 1) % TABLE_SIZE;
        } while (h != start);

        // Find empty slot (guaranteed to find one since TABLE_SIZE > max records)
        h = start;
        do {
            Slot slot = read_slot(h);
            if (slot.is_empty()) {
                Slot new_slot(index, value);
                write_slot(h, new_slot);
                return;
            }
            h = (h + 1) % TABLE_SIZE;
        } while (h != start);
    }

    void remove(const std::string& index, int32_t value) {
        size_t h = hash(index);
        size_t start = h;

        do {
            Slot slot = read_slot(h);
            if (slot.matches(index, value)) {
                slot.active = false;
                write_slot(h, slot);
                return;
            }
            if (slot.is_empty()) {
                break;  // Not found
            }
            h = (h + 1) % TABLE_SIZE;
        } while (h != start);
    }

    std::vector<int32_t> find(const std::string& index) {
        std::vector<int32_t> values;
        size_t h = hash(index);
        size_t start = h;

        do {
            Slot slot = read_slot(h);
            if (slot.matches_index(index)) {
                values.push_back(slot.value);
            }
            if (slot.is_empty()) {
                break;
            }
            h = (h + 1) % TABLE_SIZE;
        } while (h != start);

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