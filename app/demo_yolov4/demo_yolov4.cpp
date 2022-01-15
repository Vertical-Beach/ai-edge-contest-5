#include <glog/logging.h>
#include <google/protobuf/text_format.h>

#include <cmath>
#include <iostream>
#include <numeric>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <vitis/ai/dpu_task.hpp>
#include <vitis/ai/nnpp/yolov3.hpp>
#include <fstream>
#include <math.h>

using namespace std;
using namespace cv;

// The parameters of yolov3_voc, each value could be set as actual needs.
// Such format could be refer to the prototxts in /etc/dpu_model_param.d.conf/.

const string readFile(const char *filename){
  ifstream ifs(filename);
  return string(istreambuf_iterator<char>(ifs),
                istreambuf_iterator<char>());
}

class YoloRunner{
  public:
    unique_ptr<vitis::ai::DpuTask> task;
    vitis::ai::proto::DpuModelParam modelconfig;
    cv::Size model_input_size;
    vector<vitis::ai::library::InputTensor> input_tensor;
    struct bbox{
      int label;
      float xmin;
      float xmax;
      float ymin;
      float ymax;
      float score;
      bbox(vitis::ai::YOLOv3Result::BoundingBox yolobbox, float img_width, float img_height){
        this->label = yolobbox.label;
        this->score = yolobbox.score;
        this->xmin = std::max((float)0, yolobbox.x * img_width);
        this->ymin = std::max((float)0, yolobbox.y * img_height);
        this->xmax = std::min((yolobbox.x + yolobbox.width) * img_width, img_width);
        this->ymax = std::min((yolobbox.y + yolobbox.height) * img_height, img_height);
      }
    };

  public: YoloRunner(const char* modelconfig_path, const char* modelfile_path){
    const string config_str = readFile(modelconfig_path);
    auto ok = google::protobuf::TextFormat::ParseFromString(config_str, &(this->modelconfig));
    if (!ok) {
      cerr << "Set parameters failed!" << endl;
      abort();
    }
    this->task = vitis::ai::DpuTask::create(modelfile_path);
    this->input_tensor = task->getInputTensor(0u);
    int width = this->input_tensor[0].width;
    int height = this->input_tensor[0].height;
    this->model_input_size = cv::Size(width, height);
    this->task->setMeanScaleBGR({0.0f, 0.0f, 0.0f},
                        {0.00390625f, 0.00390625f, 0.00390625f});
  }
  private: cv::Mat Preprocess(cv::Mat img){
    cv::Mat resized_img;
    cv::resize(img, resized_img, this->model_input_size);
    return resized_img;
  }
  public: vector<bbox> Run(cv::Mat img){
    cv::Mat resized_img = this->Preprocess(img);
    vector<int> input_cols = {img.cols};
    vector<int> input_rows = {img.rows};
    vector<cv::Mat> inputs = {resized_img};
    task->setImageRGB(inputs);
    task->run(0);

    auto output_tensor = task->getOutputTensor(0u);
    auto results = vitis::ai::yolov3_post_process(
        input_tensor, output_tensor, this->modelconfig, input_cols, input_rows);
    auto result = results[0]; //batch_size is 1
    vector<bbox> bboxes;
    for(auto& yolobbox: result.bboxes){
      bboxes.push_back(bbox(yolobbox, img.cols, img.rows));
    }
    return bboxes;
  }

};

int main(int argc, char* argv[]) {
  char* configfile  = argv[1];
  char* modelfile = argv[2];
  char* imgfile = argv[3];
  cout << configfile << " " << modelfile << " " << imgfile;
  auto runner = YoloRunner(configfile, modelfile);
  cv::Mat img = cv::imread(imgfile);
  vector<YoloRunner::bbox> bboxes = runner.Run(img);

  string label_names[] = {"car", "pedestrian"};
  for (auto& box : bboxes) {
    int label = box.label;
    float confidence = box.score;
    cout << label_names[box.label] << " " << box.score << " " << box.xmin << " " << box.xmax << " " << box.ymin << " " << box.ymax << endl;
    rectangle(img, Point(box.xmin, box.ymin), Point(box.xmax, box.ymax),
              Scalar(0, 255, 0), 3, 1, 0);
  }
  imwrite("result.jpg", img);

  return 0;
}
