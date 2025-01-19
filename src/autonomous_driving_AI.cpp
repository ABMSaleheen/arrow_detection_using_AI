#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <cv_bridge/cv_bridge.h>

#include <opencv2/opencv.hpp>
#include <NumCpp.hpp>



class AutonmousAINode : public rclcpp::Node {

public:
  AutonmousAINode():Node("autonomous_driving_AI_node") {
    // int codec = cv::VideoWriter::fourcc('M','J','P','G');
    cv::Size frame_size(640, 480);


    vid_subscriber = this->create_subscription<sensor_msgs::msg::Image>(
                  "/camera_scan_fwd/image_raw", 40, std::bind(&AutonmousAINode::image_process_callback, 
                  this,std::placeholders::_1));

    publisher_vel = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel",30);
    timer_vel_pub = this->create_wall_timer(std::chrono::microseconds(1000),
                std::bind(&AutonmousAINode::send_cmd_vel, this));

  }

private:
  rclcpp::Time current_time;
  rclcpp::Time prev_time;
  float prev_error = 0.0f; // Pid prev error

  std::string action = "Blank";
  float velocity_lin, velocity_ang;

  int16_t center_frame_x;
  int16_t center_frame_y;

  int arrow_left_edge;
  int arrow_right_edge;
  int arrow_mid_point;
  

  int row_of_interest = 170;
  int row_of_lin_vel = row_of_interest - 5;
  int row_of_caution = 04;


  float mid_pt_error = 0.0f;
  float mid_pt_lin_vel_error = 0.0f;

  float edge_pxl_percentage;

  void image_process_callback(const sensor_msgs::msg::Image::SharedPtr msg_vid) {

    cv::Mat vid_output;
    cv::Mat frame = cv_bridge::toCvCopy(msg_vid, "bgr8")->image;

    cv::Mat resized_frame;
    cv::Mat gray;
    cv::Mat blurred_img;
    cv:: Mat mask;
    cv:: Mat canny;

    // // Resize frame to 640x480
    cv::resize(frame, resized_frame, cv::Size(640, 480));

   // ###############  --------------Segmentation-------------------------------
    uint8_t lower_val = 19;
    uint8_t upper_val = 52;
    cv::cvtColor(resized_frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray,blurred_img,cv::Size(7,7), 0);
    cv::inRange(blurred_img, lower_val, upper_val,  mask);

    //-----------------------------# Boundaries Extraction----------------------
    cv::Canny(mask,canny, 40 , 10);

    // cv::Mat canny = canny(cv::Range(r1, canny.rows), cv::Range(c1,canny.cols));  // Cropping

    uint8_t pixel_value;
    uint8_t pixel_value_lin_vel;



    // Processing mid points....................////
    center_frame_x = canny.cols/2;
    center_frame_y = canny.rows/2;


    // Describe RoI and prepare final framing.................... 
    int roiWidth = 380;
    int roiHeight = 240;
    int roi_start_x = center_frame_x - roiWidth / 2;
    int roi_start_y = center_frame_y - roiHeight / 2;


    cv::Rect region_of_interest(roi_start_x, roi_start_y, roiWidth, roiHeight);  // Mask for region to be visible
    
    cv::Mat black_mask = cv::Mat::zeros(canny.size(), CV_8UC1); // Black mask on the whole frame
    cv::rectangle(black_mask, region_of_interest, cv::Scalar(255), cv::FILLED);  // White filled region on top of the black mask

    cv::Mat roi_result;
    cv::bitwise_and(canny, black_mask, roi_result);


    // cv::putText(roi_result, action, 
    //             cv::Point(20, 80), 
    //             cv::FONT_HERSHEY_DUPLEX, 
    //             1.0, CV_RGB(255, 255, 255), 2);

    // cv::putText(roi_result, std::to_string(velocity_lin), 
    //             cv::Point(20, 120), 
    //             cv::FONT_HERSHEY_DUPLEX, 
    //             1.0, CV_RGB(255, 255, 255), 2);


    auto [left_edge_point, right_edge_point] = find_non_zero_points(roi_result);
    arrow_left_edge = left_edge_point, arrow_right_edge = right_edge_point; // use this control ang_vel wrt centre_x of frame.
    arrow_mid_point = left_edge_point + (right_edge_point- left_edge_point)/2;

    // std::cout << "Left:  " << left_edge_point<< "Right:  "<< right_edge_point << std::endl;

    vid_output = roi_result;

    // std::cout << "Frame size: " << vid_output.cols << "x" << vid_output.rows << std::endl;

    edge_pxl_percentage = find_edge_percentage(roi_result);
    std::cout << "Edge Percentage: " << edge_pxl_percentage << std::endl;




    if (row_of_interest >= 0 && row_of_interest < canny.rows &&
      center_frame_x >= 0 && center_frame_x < canny.cols) {
        vid_output.at<uint8_t>(row_of_interest - 1, center_frame_x) = 255;
        vid_output.at<uint8_t>(row_of_interest, center_frame_x) = 255;
        vid_output.at<uint8_t>(row_of_interest + 1, center_frame_x) = 255;
    }

    vid_output.at<uint8_t>(row_of_lin_vel, center_frame_x) = 255;

    cv::imshow("output", vid_output);
    if (cv::waitKey(1) == 27) {
      std::cout << "esc key is pressed by user" << std::endl;
    }
  }


  std::tuple<int,int> find_non_zero_points(cv::Mat image){
    std::vector<cv::Point> edge_points;

    cv::findNonZero(image, edge_points);
    std::vector<int> columns;

    for (const auto& point : edge_points){
      columns.push_back(point.x);
    }

    int left_edge_point_pos =  *std::min_element(columns.begin(),columns.end());
    int right_edge_point_pos = *std::max_element(columns.begin(),columns.end());

    return {left_edge_point_pos, right_edge_point_pos};
  }


  float find_edge_percentage(cv::Mat image){
    float total_pixels = image.rows* image.cols;
    float edge_pixels = cv::countNonZero(image);

    float edge_percentage = edge_pixels/ total_pixels; 
    return edge_percentage;
  }


  float PID_control(float ref_point, float current_state, float kp,float ki,float kd){

    current_time = this->now();
    float dt = (prev_time.nanoseconds()>0) ? (current_time - prev_time).seconds() : .001 ;
    prev_time = current_time;
    // RCLCPP_INFO(this->get_logger(), "dt =>>  %f", dt);

    // float prev_error;

    float error = current_state - ref_point;
    float error_der = (error - prev_error)/dt;

    // RCLCPP_INFO(this->get_logger(), "Error => %f, Previous Error => %f, Error_d => %f", error, prev_error, error_der);

    float feedback = kp*error + kd*error_der;
    // prev_error = (abs(error) > 0) ? (error) : 0.0;
    prev_error = error;
    return feedback;
  }


  void track_mid_edges(){
    // current_time = this->now();
      
    // if (velocity_lin == 0 && current_time.nanoseconds() == 0.0f){
    //   velocity_lin = 0.65;
    // }
    if (arrow_left_edge > 240 && arrow_right_edge < 400){
      float control_output_ang_vel = PID_control(arrow_mid_point, center_frame_x, 0.10f, 0.540f, 0.0f);
      velocity_ang = std::clamp(control_output_ang_vel, -1.57f, 1.57f);
    }
    else{
      velocity_ang = 0.0;
    }
    float control_output_lin_vel = PID_control(row_of_lin_vel, row_of_interest, 0.10f, 0.540f, 0.0f);
    velocity_lin = std::clamp(control_output_lin_vel, -0.7f, 0.7f);


    // if (abs(arrow_mid_point-center_frame_x) < 5){
    //   velocity_ang = 0.0;
    // }
    if (edge_pxl_percentage > 0.001){
      velocity_lin = 0.0;
    }
    // if(action == "Caution- Take Right"){
    //   velocity_lin = 0.0;
    //   velocity_ang = -1.5;
    // }
    
    // else if(action == "Caution- Take Left" ){
    //   velocity_lin = 0.0;
    //   velocity_ang = 1.5;
    // }

  }

  void send_cmd_vel(){
    // velocity_lin = 0.3f;

    track_mid_edges();
    geometry_msgs::msg::Twist velocity_cmd;

    velocity_cmd.linear.x = velocity_lin;
    velocity_cmd.angular.z = velocity_ang;
    // RCLCPP_INFO(this->get_logger(), "-------Fwd Speed : %.2f", velocity_lin);
    
    publisher_vel-> publish(velocity_cmd);
  }

  // cv::VideoWriter video_writer_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_vel;
  rclcpp::TimerBase::SharedPtr timer_vel_pub;
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr vid_subscriber;
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<AutonmousAINode>());
  rclcpp::shutdown();
  return 0;
}