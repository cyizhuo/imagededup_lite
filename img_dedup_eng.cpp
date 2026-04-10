#include <algorithm>
#include <bitset>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// Get the perceptual hash value of an image
// Reference https://github.com/idealo/imagededup
// img: input image
// return: perceptual hash value
std::string PHash(const cv::Mat& img) {
    // Convert to grayscale
    cv::Mat gray;
    if (img.channels() == 3) {
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = img.clone();
    }

    // Resize to 32x32 pixels (preserving structural information, reducing noise impact)
    cv::Mat resized;
    cv::resize(gray, resized, cv::Size(32, 32), 0, 0, cv::INTER_LANCZOS4);

    // Convert to floating point and normalize to [0,1]
    cv::Mat float_img;
    resized.convertTo(float_img, CV_32F, 1.0 / 255.0);

    // Perform 2D DCT transform, extract frequency features
    cv::Mat dct_coef;
    cv::dct(float_img, dct_coef);

    // Take top-left 8x8 low-frequency coefficients (containing main visual structure)
    cv::Mat            low_freq = dct_coef(cv::Rect(0, 0, 8, 8)).clone();
    std::vector<float> coeffs;
    cv::Mat            flat = low_freq.reshape(1, 1);
    flat.forEach<float>([&](const float& val, const int* pos) {
        coeffs.push_back(val);
    });

    // Sort and take median as threshold, generate binary hash
    std::sort(coeffs.begin(), coeffs.end());
    float median_val    = coeffs[coeffs.size() / 2];
    uchar hash_bytes[8] = {0};
    for (int i = 0; i < 8; ++i) {
        uchar byte = 0;
        for (int j = 0; j < 8; ++j) {
            if (low_freq.at<float>(i, j) >= median_val) {
                byte |= (1 << (7 - j));
            }
        }
        hash_bytes[i] = byte;
    }

    // Pack each 8-bit row into bytes, and convert to hexadecimal string output
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 8; ++i) {
        ss << std::setw(2) << static_cast<int>(hash_bytes[i]);
    }

    return ss.str();
}

// Calculate the Hamming distance between two hash values
// Reference https://github.com/idealo/imagededup
// hash_a: hash value a
// hash_b: hash value b
// return: Hamming distance
unsigned int HanmingDistance(const std::string& hash_a,
                             const std::string& hash_b) {
    if (hash_a.empty() || hash_b.empty() || hash_a.size() != hash_b.size())
        return 20;

    uint64_t        val1 = std::stoull(hash_a, nullptr, 16);
    uint64_t        val2 = std::stoull(hash_b, nullptr, 16);
    std::bitset<64> bits(val1 ^ val2);
    unsigned int    count = static_cast<int>(bits.count());

    return count;
}

// Find duplicate images
// img_dir: folder path containing images
// return: array of duplicate/approximate image paths that can be removed
std::vector<std::string> FindDuplicateImgs(std::string img_dir) {
    // Recursively get all image paths
    std::vector<std::string> vec_img_path;
    std::vector<std::string> exts = {".jpg", ".png", ".jpeg", ".bmp"};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(img_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (std::find(exts.begin(), exts.end(), ext) != exts.end()) {
            vec_img_path.push_back(entry.path().string());
        }
    }
    std::sort(vec_img_path.begin(), vec_img_path.end());

    // Read image hash values
    std::vector<std::string> vec_img_hash;
    for (const auto& path : vec_img_path) {
        vec_img_hash.push_back(PHash(cv::imread(path)));
    }

    // Iterate through all image combinations, find duplicate/approximate images
    std::vector<bool> vec_dup_flag(vec_img_path.size(), false);  // duplicate flag array
    for (size_t i = 0; i < vec_img_path.size(); ++i) {
        const auto& hash1 = vec_img_hash[i];  // First image's hash

        for (size_t j = i + 1; j < vec_img_path.size(); ++j) {
            // Skip if second image has been marked as duplicate
            if (vec_dup_flag[j]) {
                continue;
            }

            const auto& hash2 = vec_img_hash[j];  // Second image's hash

            // Determine if duplicate/approximate based on Hamming distance of hash values
            if (HanmingDistance(hash1, hash2) <= 8) {
                vec_dup_flag[j] = true;
            }
        }
    }

    // Collect all duplicate image paths that can be removed
    std::vector<std::string> vec_dup_img_path;
    for (size_t i = 0; i < vec_img_path.size(); ++i) {
        if (vec_dup_flag[i]) {
            vec_dup_img_path.push_back(vec_img_path[i]);
        }
    }
    return vec_dup_img_path;
}

int main(int argc, char** argv) {
    std::vector<std::string> vec_dup_img_path = FindDuplicateImgs("/path/of/imgs");
}
