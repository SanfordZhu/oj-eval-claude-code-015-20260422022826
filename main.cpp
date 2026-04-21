#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <functional>

constexpr size_t MAX_INDEX_LEN = 64;
constexpr char DATA_FILE[] = "data.bin";
constexpr int NUM_BUCKETS = 1009;  // Prime number
constexpr int MAX_RECORDS_PER_BUCKET = 200;  // Allow some overflow

#pragma pack(push, 1)
struct Record {
    char index[MAX_INDEX_LEN];
    int32_t value;
    bool active;
    int32_t next;  // Index of next record in overflow chain, -1 for none

    Record() : value(0), active(false), next(-1) {
        memset(index, 0, MAX_INDEX_LEN);
    }

    Record(const std::string& idx, int32_t val) : value(val), active(true), next(-1) {
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
    std::fstream file;
    std::streampos bucket_start;
    int total_records;

    size_t hash(const std::string& str) const {
        std::hash<std::string> hasher;
        return hasher(str) % NUM_BUCKETS;
    }

    std::streampos get_bucket_offset(size_t bucket_idx) const {
        return bucket_start + bucket_idx * sizeof(int32_t);
    }

    std::streampos get_record_offset(int record_idx) const {
        return bucket_start + NUM_BUCKETS * sizeof(int32_t) + record_idx * sizeof(Record);
    }

    int read_bucket_head(size_t bucket_idx) {
        std::streampos pos = get_bucket_offset(bucket_idx);
        file.seekg(pos);
        int head;
        file.read(reinterpret_cast<char*>(&head), sizeof(int));
        return head;
    }

    void write_bucket_head(size_t bucket_idx, int head) {
        std::streampos pos = get_bucket_offset(bucket_idx);
        file.seekp(pos);
        file.write(reinterpret_cast<const char*>(&head), sizeof(int));
    }

    Record read_record(int record_idx) {
        std::streampos pos = get_record_offset(record_idx);
        file.seekg(pos);
        Record rec;
        file.read(reinterpret_cast<char*>(&rec), sizeof(Record));
        return rec;
    }

    void write_record(int record_idx, const Record& rec) {
        std::streampos pos = get_record_offset(record_idx);
        file.seekp(pos);
        file.write(reinterpret_cast<const char*>(&rec), sizeof(Record));
    }

    int allocate_record() {
        int new_idx = total_records;
        total_records++;
        return new_idx;
    }

public:
    FileStorage() : total_records(0) {
        // Open or create file
        file.open(DATA_FILE, std::ios::binary | std::ios::in | std::ios::out);
        if (!file) {
            // Create new file
            file.open(DATA_FILE, std::ios::binary | std::ios::out);
            file.close();
            file.open(DATA_FILE, std::ios::binary | std::ios::in | std::ios::out);

            // Initialize bucket heads to -1
            bucket_start = 0;
            for (int i = 0; i < NUM_BUCKETS; i++) {
                int head = -1;
                file.write(reinterpret_cast<const char*>(&head), sizeof(int));
            }
            file.flush();
        } else {
            // File exists, read total_records from end
            file.seekg(0, std::ios::end);
            std::streampos file_size = file.tellg();
            bucket_start = 0;

            // Calculate total_records
            std::streampos records_start = NUM_BUCKETS * sizeof(int32_t);
            if (file_size > records_start) {
                total_records = (file_size - records_start) / sizeof(Record);
            }
        }
    }

    ~FileStorage() {
        if (file.is_open()) {
            file.close();
        }
    }

    void insert(const std::string& index, int32_t value) {
        size_t bucket_idx = hash(index);
        int head = read_bucket_head(bucket_idx);

        // Check if already exists
        int curr = head;
        while (curr != -1) {
            Record rec = read_record(curr);
            if (rec.matches(index, value)) {
                return;  // Already exists
            }
            curr = rec.next;
        }

        // Create new record
        int new_idx = allocate_record();
        Record new_rec(index, value);
        new_rec.next = head;

        // Write record
        write_record(new_idx, new_rec);

        // Update bucket head
        write_bucket_head(bucket_idx, new_idx);
        file.flush();
    }

    void remove(const std::string& index, int32_t value) {
        size_t bucket_idx = hash(index);
        int head = read_bucket_head(bucket_idx);

        if (head == -1) return;

        // Check first record
        Record first_rec = read_record(head);
        if (first_rec.matches(index, value)) {
            // Mark as inactive
            first_rec.active = false;
            write_record(head, first_rec);
            file.flush();
            return;
        }

        // Traverse list
        int prev = head;
        int curr = first_rec.next;
        while (curr != -1) {
            Record rec = read_record(curr);
            if (rec.matches(index, value)) {
                // Mark as inactive
                rec.active = false;
                write_record(curr, rec);
                file.flush();
                return;
            }
            prev = curr;
            curr = rec.next;
        }
        // Not found
    }

    std::vector<int32_t> find(const std::string& index) {
        std::vector<int32_t> values;
        size_t bucket_idx = hash(index);
        int head = read_bucket_head(bucket_idx);

        int curr = head;
        while (curr != -1) {
            Record rec = read_record(curr);
            if (rec.matches_index(index)) {
                values.push_back(rec.value);
            }
            curr = rec.next;
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