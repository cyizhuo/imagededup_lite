from glob import glob
from os.path import join

import numpy as np
from PIL import Image
from scipy.fftpack import dct


# Get the phash value of the image (string)
# Reference https://github.com/idealo/imagededup
# img_path: image path
# return: 16-bit phash string
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


# Calculate the Hamming distance between two hash values, smaller distance indicates higher similarity
# Reference https://github.com/idealo/imagededup
# hash1: first hash value
# hash2: second hash value
# return: Hamming distance between two hash values
def hanming_distance(hash1: str, hash2: str) -> int:
    hash1_bin = bin(int(hash1, 16))[2:].zfill(64)
    hash2_bin = bin(int(hash2, 16))[2:].zfill(64)

    return sum([i != j for i, j in zip(hash1_bin, hash2_bin)])


# Find duplicate images
# img_dir: folder path containing images
# return: array of duplicate/approximate image paths that can be removed
def find_duplicate_imgs(img_dir):
    # Recursively get all image paths
    exts = (".jpg", ".png", ".jpeg", ".bmp")
    img_paths = sorted(glob(join(img_dir, "**", "*"), recursive=True))
    img_paths = [path for path in img_paths if path.lower().endswith(exts)]

    # Read image hash values
    img_hashs = [phash(img_path) for img_path in img_paths]

    # Iterate through all image combinations to find duplicate/similar images
    dup_flag = [False] * len(img_paths)  # Duplicate flag array
    for i in range(len(img_paths)):
        hash_1 = img_hashs[i]  # First image's hash

        for j in range(i + 1, len(img_paths)):
            # Skip if second image is already marked as duplicate
            if dup_flag[j]:
                continue

            hash_2 = img_hashs[j]  # Second image's hash

            # Determine if duplicate/similar based on hash Hamming distance
            hamming_dis = hanming_distance(hash_1, hash_2)
            if hamming_dis <= 8:
                dup_flag[j] = True

    # Return array of duplicate/approximate image paths that can be removed
    dup_img_paths = [img_paths[i] for i in range(len(img_paths)) if dup_flag[i]]
    return dup_img_paths


if __name__ == "__main__":
    dup_img_paths = find_duplicate_imgs("/path/of/imgs")
