#include <chrono>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>
#include <cv_bridge/cv_bridge.h>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <sensor_msgs/msg/image.hpp>
// using namespace cv;

class ImgSaveNode : public rclcpp::Node
{
public:
    ImgSaveNode() : Node("image_saver_node")
    {       
        // int codec = cv::VideoWriter::fourcc('M','J','P','G');
        cv::Size frame_size(640, 480);
        double fps = 40.0;

        int codec = cv::VideoWriter::fourcc('X', 'V', 'I', 'D');
        video_writer_.open("/home/saleheen_linux/others/cv_vid/arrow_detection/Right/output.avi", codec, fps, frame_size, true);


        if (!video_writer_.isOpened()) {
            RCLCPP_ERROR(this->get_logger(), "Could not open the output video file for write.");
        }

        vid_subscriber = this->create_subscription<sensor_msgs::msg::Image>("/camera_scan_fwd/image_raw",40,
            std::bind(&ImgSaveNode::sub_video_writer_callback, this, std::placeholders::_1));
    }

    ~ImgSaveNode() {
            if (video_writer_.isOpened()) {
                video_writer_.release();
                RCLCPP_INFO(this->get_logger(), "Video writer released.");
            }
    }

private:
    
    void sub_video_writer_callback(const sensor_msgs::msg::Image::SharedPtr msg_vid){
        try {
            cv::Mat frame = cv_bridge::toCvCopy(msg_vid, "bgr8")->image;

            cv::Mat resized_frame;
            cv::Mat gray;
            cv::Mat blurred_img;
            cv:: Mat mask;

            // // Resize frame to 640x480
            cv::resize(frame, resized_frame, cv::Size(640, 480));

            uint8_t lower_val = 19;
            uint8_t upper_val = 52;
            cv::cvtColor(resized_frame, gray, cv::COLOR_BGR2GRAY);
            cv::GaussianBlur(gray,blurred_img,cv::Size(7,7), 0);
            cv::inRange(blurred_img, lower_val, upper_val,  mask);
            cv::Canny(mask,mask, 40 , 10);

            // gray = cv2.cvtColor(image_src, cv2.COLOR_BGR2GRAY)
            // image = cv2.GaussianBlur(gray,(5,5), 0)
            // mask = cv2.inRange(image, lower_val, upper_val)

            // Save individual frames as images for testing
            static int frame_count = 0;
            // cv::imwrite("/home/saleheen_linux/others/cv_vid/arrow_detection/Left/left_dir_" + std::to_string(frame_count++) + ".jpg", mask);
            cv::imwrite("/home/saleheen_linux/others/cv_vid/arrow_detection/Right/right_dir_" + std::to_string(frame_count++) + ".jpg", mask);

            if (video_writer_.isOpened()) {
                video_writer_.write(mask);
                RCLCPP_INFO(this->get_logger(), "Resized frame written to video.");
            } else {
                RCLCPP_ERROR(this->get_logger(), "Video writer is not open.");
            }

            cv::imshow("output", mask);
            cv::waitKey(1);
        } 
        catch (cv_bridge::Exception& e) {
            RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
        }
    }
    
    cv::VideoWriter video_writer_;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr vid_subscriber;

};


int main(int argc, char *argv[]){
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ImgSaveNode>());
    rclcpp::shutdown();
    return 0;
}