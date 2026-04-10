from glob import glob
from os.path import join

import numpy as np
from PIL import Image
from scipy.fftpack import dct


# 获取图片的 phash 值 (字符串)
# 参考 https://github.com/idealo/imagededup
# img_path: 图片路径
# return: 16 位 phash 字符串
def phash(img_path):
    img = Image.open(img_path)

    img = img.resize((32, 32), Image.LANCZOS)
    img = img.convert("L")
    img = np.array(img).astype(np.uint8)

    dct_coef = dct(dct(img, axis=0), axis=1)
    dct_reduced_coef = dct_coef[:8, :8]
    median_coef_val = np.median(np.ndarray.flatten(dct_reduced_coef)[1:])

    hash_mat = dct_reduced_coef >= median_coef_val
    hash_str = "".join("%0.2x" % x for x in np.packbits(hash_mat))

    return hash_str


# 计算两个 hash 值的汉明距离, 距离越小说明越相似
# 参考 https://github.com/idealo/imagededup
# hash1: hash 值 1 号
# hash2: hash 值 2 号
# return: 两个 hash 值的汉明距离
def hanming_distance(hash1: str, hash2: str) -> int:
    hash1_bin = bin(int(hash1, 16))[2:].zfill(64)
    hash2_bin = bin(int(hash2, 16))[2:].zfill(64)

    return sum([i != j for i, j in zip(hash1_bin, hash2_bin)])


# 查找重复的图片
# img_dir: 包含图片的文件夹路径
# return: 可去除的重复/近似图片路径数组
def find_duplicate_imgs(img_dir):
    # 递归获取所有图片的路径
    exts = (".jpg", ".png", ".jpeg", ".bmp")
    img_paths = sorted(glob(join(img_dir, "**", "*"), recursive=True))
    img_paths = [path for path in img_paths if path.lower().endswith(exts)]

    # 读取图片 hash 值
    img_hashs = [phash(img_path) for img_path in img_paths]

    # 遍历所有图片组合, 查找重复/近似的图片
    dup_flag = [False] * len(img_paths)  # 重复标志位数组
    for i in range(len(img_paths)):
        hash_1 = img_hashs[i]  # 第一张图片的 hash

        for j in range(i + 1, len(img_paths)):
            # 如果第二张图片已被标记为重复, 就跳过
            if dup_flag[j]:
                continue

            hash_2 = img_hashs[j]  # 第二张图片的 hash

            # 根据 hash 值的汉明距离判断是否重复/近似
            hamming_dis = hanming_distance(hash_1, hash_2)
            if hamming_dis <= 8:
                dup_flag[j] = True

    # 返回可去除的重复/近似图片路径数组
    dup_img_paths = [img_paths[i] for i in range(len(img_paths)) if dup_flag[i]]
    return dup_img_paths


if __name__ == "__main__":
    dup_img_paths = find_duplicate_imgs("/path/of/imgs")

