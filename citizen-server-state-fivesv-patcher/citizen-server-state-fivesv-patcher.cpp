#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <limits>

//BF 32 00 00 00 80 7D 00 00 0F 84 2C 01 00 00 48 -> mov     edi, 32h    -> syncDelay = 50ms                Default sync delay
//00 20 99 44 56 EE B4 44 00 80 D4 44 00 A8 F6 45 -> 00 20 99 44         -> 1225.0f (35f * 35f)             the lowest sync delay distance constant
//48 C1 EF 02 E9 9C 00 00 00 45 84 F6 0F 84 93 00 -> shr     rdi, 2      -> syncDelay /= 4;                  syncDelay in first if playerEntity (probably unused)
//B8 0C 00 00 00 48 0F 42 F8 49 8B 1C 24 44 0F B7 -> mov     eax, 0Ch    -> syncDelay /= 4 = 12 (0xC)        syncDelay in second if playerEntity -> compiler decided to not shift right here but instead directly initialize to the rounded result of 50 / 4 even if it's the same operation Note: rdi is later initialized with value of 32bit register eax

bool is_pow_of_two(const int num) {
    if (num <= 0) {
        return false;
    }
    return (num & (num - 1)) == 0;
}

int pow_of_two_to_shift_amount_fast(int divisor) {
    if (!is_pow_of_two(divisor)) {
        throw std::invalid_argument("divisor must be a power of two");
    }
    int shiftAmount = 0;
    while ((divisor & 1) == 0) {
        divisor >>= 1;
        shiftAmount++;
    }
    return shiftAmount;
}

std::vector<uint8_t> read_dll_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open file: " + filepath);
    }

    file.seekg(0, std::ios::end);
    const size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<long long>(fileSize));

    if (!file) {
        throw std::runtime_error("Error reading file: " + filepath);
    }

    return buffer;
}

struct pattern_match {
    size_t offset;
    std::vector<uint8_t> bytes;
};

std::vector<pattern_match> find_pattern(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern) {
    std::vector<pattern_match> matches;

    if (data.size() < pattern.size() || pattern.empty()) {
        return matches;
    }

    for (size_t i = 0; i <= data.size() - pattern.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < pattern.size(); ++j) {
            if (data[i + j] != pattern[j]) {
                found = false;
                break;
            }
        }

        if (found) {
            pattern_match match;
            match.offset = i;
            match.bytes = std::vector<uint8_t>(data.begin() + static_cast<long long>(i), data.begin() + static_cast<long long>(i) + static_cast<long long>(pattern.size()));
            matches.push_back(match);
        }
    }

    return matches;
}

bool patch_file(const std::string& filepath, const size_t offset, const std::vector<uint8_t>& newBytes) {
    std::fstream file(filepath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) {
        std::cerr << "Could not open file for patching: " << filepath << '\n';
        return false;
    }

    file.seekp(static_cast<long long>(offset), std::ios::beg);
    file.write(reinterpret_cast<const char*>(newBytes.data()), static_cast<long long>(newBytes.size()));

    if (!file) {
        std::cerr << "Error writing to file: " << filepath << '\n';
        return false;
    }

    return true;
}

std::vector<uint8_t> float_to_bytes(const float value) {
    std::vector<uint8_t> result(4);
    std::memcpy(result.data(), &value, sizeof(float));
    return result;
}

float bytes_to_float(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 4) return 0.0f;
    float result;
    std::memcpy(&result, bytes.data(), sizeof(float));
    return result;
}

int main() {
    std::string dllPath{};
    int syncDelay{50};
    int syncDivider{4};

    std::cout << "Enter DLL path: ";
    std::getline(std::cin, dllPath);

    std::cout << "Enter new syncDelay value (default: 50): ";
    std::cin >> syncDelay;

    if (syncDelay <= 0 || syncDelay > 255) {
        std::cerr << "Error: syncDelay must be between 1 and 255" << '\n';
        return 1;
    }

    std::cout << "Enter sync delay divisor (must be a power of 2, default: 4): ";
    std::cin >> syncDivider;

    try {
        bool patchLowestSyncDelayDistance{false};
	    float lowestSyncDelayDistance{1225.0f};
	    if (!is_pow_of_two(syncDivider)) {
            std::cerr << "Error: divisor must be a power of two" << '\n';
            return 1;
        }

        int shiftAmount = pow_of_two_to_shift_amount_fast(syncDivider);
        int dividedValue = syncDelay / syncDivider;

        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Do you want to patch the lowest sync delay distance constant? (y/n): ";
        char response;
        std::cin >> response;

        if (response == 'y' || response == 'Y') {
            patchLowestSyncDelayDistance = true;
            std::cout << "Enter new lowest sync delay distance value (default: 1225): ";
            std::cin >> lowestSyncDelayDistance;
        }

        std::vector<uint8_t> dllData = read_dll_file(dllPath);

        std::vector<uint8_t> pattern1 = { 0xBF, 0x32, 0x00, 0x00, 0x00, 0x80, 0x7D, 0x00, 0x00, 0x0F, 0x84, 0x2C, 0x01, 0x00, 0x00, 0x48 }; // mov edi, 32h
        std::vector<uint8_t> pattern2 = { 0x48, 0xC1, 0xEF, 0x02, 0xE9, 0x9C, 0x00, 0x00, 0x00, 0x45, 0x84, 0xF6, 0x0F, 0x84, 0x93, 0x00 }; // shr rdi, 2
        std::vector<uint8_t> pattern3 = { 0xB8, 0x0C, 0x00, 0x00, 0x00, 0x48, 0x0F, 0x42, 0xF8, 0x49, 0x8B, 0x1C, 0x24, 0x44, 0x0F, 0xB7 }; // mov eax, 0Ch
        std::vector<uint8_t> pattern4 = { 0x00, 0x20, 0x99, 0x44, 0x56, 0xEE, 0xB4, 0x44, 0x00, 0x80, 0xD4, 0x44, 0x00, 0xA8, 0xF6, 0x45 }; // lowest sync delay distance pattern (1225.0f)

        auto matches1 = find_pattern(dllData, pattern1);
        auto matches2 = find_pattern(dllData, pattern2);
        auto matches3 = find_pattern(dllData, pattern3);
        auto matches4 = find_pattern(dllData, pattern4);

        std::cout << "Found " << matches1.size() << " matches for pattern 1 (mov edi, 32h)" << '\n';
        std::cout << "Found " << matches2.size() << " matches for pattern 2 (shr rdi, 2)" << '\n';
        std::cout << "Found " << matches3.size() << " matches for pattern 3 (mov eax, 0Ch)" << '\n';
        std::cout << "Found " << matches4.size() << " matches for pattern 4 (1225.0f constant)" << '\n';

        std::vector<uint8_t> newPattern1 = { 0xBF, static_cast<uint8_t>(syncDelay), 0x00, 0x00, 0x00 }; // mov edi, new_value
        std::vector<uint8_t> newPattern2 = { 0x48, 0xC1, 0xEF, static_cast<uint8_t>(shiftAmount) }; // shr rdi, new_shift_amount
        std::vector<uint8_t> newPattern3 = { 0xB8, static_cast<uint8_t>(dividedValue), 0x00, 0x00, 0x00 }; // mov eax, new_divided_value

        std::vector<uint8_t> newPattern4;
        if (patchLowestSyncDelayDistance) {
            newPattern4 = float_to_bytes(lowestSyncDelayDistance);
        }

        bool success = true;

        if (!matches1.empty()) {
            for (const auto& match : matches1) {
                if (!patch_file(dllPath, match.offset, newPattern1)) {
                    success = false;
                }
                else {
                    std::cout << "Patched mov edi, 32h -> mov edi, "
                        << std::hex << static_cast<int>(syncDelay)
                        << "h at offset 0x" << std::hex << match.offset << std::dec << '\n';
                }
            }
        }
        else {
            std::cerr << "Warning: Pattern 1 not found, couldn't patch." << '\n';
        }

        if (!matches2.empty()) {
            for (const auto& match : matches2) {
                if (!patch_file(dllPath, match.offset, newPattern2)) {
                    success = false;
                }
                else {
                    std::cout << "Patched shr rdi, 2 -> shr rdi, "
                        << std::dec << shiftAmount
                        << " at offset 0x" << std::hex << match.offset << std::dec << '\n';
                }
            }
        }
        else {
            std::cerr << "Warning: Pattern 2 not found, couldn't patch." << '\n';
        }

        if (!matches3.empty()) {
            for (const auto& match : matches3) {
                if (!patch_file(dllPath, match.offset, newPattern3)) {
                    success = false;
                }
                else {
                    std::cout << "Patched mov eax, 0Ch -> mov eax, "
                        << std::hex << dividedValue
                        << "h at offset 0x" << std::hex << match.offset << std::dec << '\n';
                }
            }
        }
        else {
            std::cerr << "Warning: Pattern 3 not found, couldn't patch." << '\n';
        }

        if (patchLowestSyncDelayDistance && !matches4.empty()) {
            for (const auto& match : matches4) {
                if (!patch_file(dllPath, match.offset, newPattern4)) {
                    success = false;
                }
                else {
                    std::cout << "Patched lowest sync delay distance constant: 1225 -> "
                        << lowestSyncDelayDistance
                        << " at offset 0x" << std::hex << match.offset << std::dec << '\n';
                    std::cout << "  Raw bytes: ";
                    for (auto byte : newPattern4) {
                        std::cout << std::hex << static_cast<int>(byte) << " ";
                    }
                    std::cout << std::dec << '\n';
                }
            }
        }
        else if (patchLowestSyncDelayDistance) {
            std::cerr << "Warning: Pattern 4 (lowest sync delay distance constant) not found, couldn't patch." << '\n';
        }

        if (success) {
            std::cout << "All patches applied successfully!" << '\n';
        }
        else {
            std::cerr << "Some patches failed to apply." << '\n';
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}