## base app
[samples/dpu_task/yolov3](https://github.com/Xilinx/Vitis-AI/tree/v1.4/demo/Vitis-AI-Library/samples/dpu_task/yolov3)

## compile on host
```sh
unset LD_LIBRARY_PATH
source /home/lp6m/petalinux_sdk_2020.2/environment-setup-aarch64-xilinx-linux
sh build.sh
```

## compile on edge
```
sh build.sh
```

## run on edge
### test image
```
./demo_yolov4  yolov4_tiny_conf0.3.prototxt ../yolov4_tiny_signate/yolov4_tiny_signate.xmodel test.jpg image
```
### generate video results to json
generated json files can be used in `ByteTrack-cpp-ai-edge-contest-5/app/generate_submit_file`.
```
./demo_yolov4  yolov4_tiny_conf0.3.prototxt ../yolov4_tiny_signate/yolov4_tiny_signate.xmodel test.avi video
```