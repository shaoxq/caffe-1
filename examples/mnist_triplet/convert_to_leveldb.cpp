//
// This script converts the MNIST dataset to the leveldb format used
// by caffe to train triplet network.
// Usage:
//    convert_mnist_data input_image_file input_label_file output_db_file
// The MNIST dataset could be downloaded at
//    http://yann.lecun.com/exdb/mnist/

#include <fstream>  // NOLINT(readability/streams)
#include <string>

#include "glog/logging.h"
#include "google/protobuf/text_format.h"
#include "leveldb/db.h"
#include "stdint.h"

#include "caffe/proto/caffe.pb.h"
#include "caffe/util/math_functions.hpp"

uint32_t swap_endian(uint32_t val) {
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | (val >> 16);
}

void read_image(std::ifstream* image_file, std::ifstream* label_file,
                uint32_t index, uint32_t rows, uint32_t cols,
                char* pixels, char* label) {
    image_file->seekg(index * rows * cols + 16);
    image_file->read(pixels, rows * cols);
    label_file->seekg(index + 8);
    label_file->read(label, 1);
}

//data/mnist/train-images-idx3-ubyte data/mnist/train-labels-idx1-ubyte examples/mnist_triplet/mnist_triplet_train_leveldb
//data/mnist/t10k-images-idx3-ubyte data/mnist/t10k-labels-idx1-ubyte examples/mnist_triplet/mnist_triplet_test_leveldb
void convert_dataset(
        const char* image_filename,
        const char* label_filename,
        const char* db_filename) {
    // Open files
    std::ifstream image_file(image_filename, std::ios::in | std::ios::binary);
    std::ifstream label_file(label_filename, std::ios::in | std::ios::binary);
    CHECK(image_file) << "Unable to open file " << image_filename;
    CHECK(label_file) << "Unable to open file " << label_filename;
    // Read the magic and the meta data
    uint32_t magic;
    uint32_t num_items;
    uint32_t num_labels;
    uint32_t rows;
    uint32_t cols;

    image_file.read(reinterpret_cast<char*>(&magic), 4);
    magic = swap_endian(magic);
    CHECK_EQ(magic, 2051) << "Incorrect image file magic.";
    label_file.read(reinterpret_cast<char*>(&magic), 4);
    magic = swap_endian(magic);
    CHECK_EQ(magic, 2049) << "Incorrect label file magic.";

    image_file.read(reinterpret_cast<char*>(&num_items), 4);
    num_items = swap_endian(num_items);
    label_file.read(reinterpret_cast<char*>(&num_labels), 4);
    num_labels = swap_endian(num_labels);
    CHECK_EQ(num_items, num_labels);

    image_file.read(reinterpret_cast<char*>(&rows), 4);
    rows = swap_endian(rows);
    image_file.read(reinterpret_cast<char*>(&cols), 4);
    cols = swap_endian(cols);

    LOG(INFO) << "A total of " << num_items << " items.";
    LOG(INFO) << "Rows: " << rows << " Cols: " << cols;

    char** images = new char*[num_items];
    char* labels = new char[num_items];

    for (int i = 0; i < num_items; ++i) {
        images[i] = new char[rows * cols];
        read_image(&image_file, &label_file, i, rows, cols, images[i], &labels[i]);
    }

    // Open leveldb
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    options.error_if_exists = true;
    leveldb::Status status = leveldb::DB::Open(options, db_filename, &db);
    CHECK(status.ok()) << "Failed to open leveldb " << db_filename
                       << ". Is it already existing?";

    const int kMaxKeyLength = 10;
    char key[kMaxKeyLength];
    std::string value;

    caffe::Datum datum;
    datum.set_channels(1);  // one channel for each image in the triplet
    datum.set_height(rows);
    datum.set_width(cols);
    for (int i = 0; i < num_items; ++i) {
        datum.set_data(images[i], rows * cols);
        datum.set_label(labels[i]);

        datum.SerializeToString(&value);
        snprintf(key, kMaxKeyLength, "%08d", i);
        db->Put(leveldb::WriteOptions(), std::string(key), value);
    }
    printf("Dataset size %d\n", num_items);
    delete db;
    for (int i = 0; i < num_items; ++i)
	delete images[i];
    delete images;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        printf("This script converts the MNIST dataset to the leveldb format used\n"
                       "by caffe to train a siamese network.\n"
                       "Usage:\n"
                       "    convert_to_leveld input_image_file input_label_file "
                       "output_db_file\n"
                       "The MNIST dataset could be downloaded at\n"
                       "    http://yann.lecun.com/exdb/mnist/\n"
                       "You should gunzip them after downloading.\n");
    } else {
        google::InitGoogleLogging(argv[0]);
        convert_dataset(argv[1], argv[2], argv[3]);
    }
    return 0;
}

