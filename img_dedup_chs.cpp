#include <algorithm>
#include <bitset>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// 获取图像的感知哈希值
// 参考 https://github.com/idealo/imagededup
// img: 输入图片
// return: 感知哈希值
std::string PHash(const cv::Mat& img) {
    // 转灰度图
    cv::Mat gray;
    if (img.channels() == 3) {
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = img.clone();
    }

    // 缩放到 32x32 像素 (保留结构信息，减少噪声影响)
    cv::Mat resized;
    cv::resize(gray, resized, cv::Size(32, 32), 0, 0, cv::INTER_LANCZOS4);

    // 转为浮点型并归一化到 [0,1]
    cv::Mat float_img;
    resized.convertTo(float_img, CV_32F, 1.0 / 255.0);

    // 进行二维 DCT 变换, 提取频率特征
    cv::Mat dct_coef;
    cv::dct(float_img, dct_coef);

    // 取左上角 8x8 的低频系数 (包含主要视觉结构)
    cv::Mat            low_freq = dct_coef(cv::Rect(0, 0, 8, 8)).clone();
    std::vector<float> coeffs;
    cv::Mat            flat = low_freq.reshape(1, 1);
    flat.forEach<float>([&](const float& val, const int* pos) {
        coeffs.push_back(val);
    });

    // 排序后取中位数作为阈值, 生成二值哈希
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

    // 将每个 8-bit 行打包成字节, 并转换为十六进制字符串输出
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 8; ++i) {
        ss << std::setw(2) << static_cast<int>(hash_bytes[i]);
    }

    return ss.str();
}

// 计算两个哈希值之间的汉明距离
// 参考 https://github.com/idealo/imagededup
// hash_a: 哈希值 a
// hash_b: 哈希值 b
// return: 汉明距离
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

// 查找重复的图片
// img_dir: 包含图片的文件夹路径
// return: 可去除的重复/近似图片路径数组
std::vector<std::string> FindDuplicateImgs(std::string img_dir) {
    // 递归获取所有图片的路径
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

    // 读取图片 hash 值
    std::vector<std::string> vec_img_hash;
    for (const auto& path : vec_img_path) {
        vec_img_hash.push_back(PHash(cv::imread(path)));
    }

    // 遍历所有图片组合, 查找重复/近似的图片
    std::vector<bool> vec_dup_flag(vec_img_path.size(), false);  // 重复标志位数组
    for (size_t i = 0; i < vec_img_path.size(); ++i) {
        const auto& hash1 = vec_img_hash[i];  // 第一张图片的 hash

        for (size_t j = i + 1; j < vec_img_path.size(); ++j) {
            // 如果第二张图片已被标记为重复, 就跳过
            if (vec_dup_flag[j]) {
                continue;
            }

            const auto& hash2 = vec_img_hash[j];  // 第二张图片的 hash

            // 根据 hash 值的汉明距离判断是否重复/近似
            if (HanmingDistance(hash1, hash2) <= 8) {
                vec_dup_flag[j] = true;
            }
        }
    }

    // 收集所有可去除的重复图片路径
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
