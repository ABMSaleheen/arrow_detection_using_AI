#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <stdint.h>
#include <array>

#include <rclcpp/rclcpp.hpp>

#include <std_msgs/msg/float32_multi_array.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <geometry_msgs/msg/pose.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <cv_bridge/cv_bridge.h>

#include <opencv2/opencv.hpp>
#include "arrow_detection/quaternion_operations.hpp"
#include <Eigen/Dense>
#include <cmath>
#include<nav_msgs/msg/odometry.hpp>
using namespace Eigen;

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

    publisher_img = this->create_publisher<sensor_msgs::msg::Image>("autonomous_node/gray_scale_img",10);


    subscriber_decision = this->create_subscription<std_msgs::msg::Float32MultiArray>(
                  "decision_node/detected_direction", 10, std::bind(&AutonmousAINode::decision_process_callback, 
                  this,std::placeholders::_1));

    subscriber_odom_rover = this->create_subscription<nav_msgs::msg::Odometry>(
        "/odom", rclcpp::SensorDataQoS(), std::bind(&AutonmousAINode::sub_odom_rover_calllback, this, std::placeholders::_1));
    
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
  cv::Mat roi_result;

  float detected_direction = -1.0; // # 0: Left # 1: Right
  float confidence = -1.0;

  Vector3d rover_pos;
  Vector3d rover_orientation_E;
  Quaterniond rover_orientation_q;
  float rover_current_ang_vel;

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

    
    cv::bitwise_and(canny, black_mask, roi_result);

    if (detected_direction == 0.0){
      action = "<---- LEFT";
    }
    else if (detected_direction== 1.0)
    {
      action = "RIGHT ---->";
    }
    // else{}
    

    auto [left_edge_point, right_edge_point] = find_non_zero_points(roi_result);
    arrow_left_edge = left_edge_point, arrow_right_edge = right_edge_point; // use this control ang_vel wrt centre_x of frame.
    arrow_mid_point = left_edge_point + (right_edge_point- left_edge_point)/2;

    // std::cout << "Left:  " << left_edge_point<< "Right:  "<< right_edge_point << std::endl;

    vid_output = roi_result;

    // std::cout << "Frame size: " << vid_output.cols << "x" << vid_output.rows << std::endl;

    edge_pxl_percentage = find_edge_percentage(roi_result);
    // std::cout << "Edge Percentage: " << edge_pxl_percentage << std::endl;


    if (row_of_interest >= 0 && row_of_interest < canny.rows &&
      center_frame_x >= 0 && center_frame_x < canny.cols) {
        vid_output.at<uint8_t>(row_of_interest - 1, center_frame_x) = 255;
        vid_output.at<uint8_t>(row_of_interest, center_frame_x) = 255;
        vid_output.at<uint8_t>(row_of_interest + 1, center_frame_x) = 255;
    }
    vid_output.at<uint8_t>(row_of_lin_vel, center_frame_x) = 255;


    cv::putText(vid_output, action, 
                cv::Point(20, 80), 
                cv::FONT_HERSHEY_DUPLEX, 
                1.0, CV_RGB(255, 255, 255), 2);

    // cv::putText(roi_result, std::to_string(velocity_lin), 
    //             cv::Point(20, 120), 
    //             cv::FONT_HERSHEY_DUPLEX, 
    //             1.0, CV_RGB(255, 255, 255), 2);


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


  void track_mid_point_of_edges(){
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
    if (edge_pxl_percentage > 0.001 && rover_current_ang_vel < 0.008){
      velocity_lin = 0.0;
      img_pub_callback(roi_result);  // Img pub to AI node

      if(detected_direction != -1.0){
        find_des_rover_orientation_q(rover_orientation_q);
      }

      // add vel_lin.
    }

  }

  Quaterniond find_des_rover_orientation_q(Quaterniond rover_orientation_q){
      // receive orientation_q  
      // --- Being received by the callback func below

      // convert to Euler.
      rover_orientation_E = quat_to_euler(rover_orientation_q);

      // based on detected direction- define desired_rover_yaw.
      double desired_rover_yaw ;
      Vector3d desired_rover_orientation_E;
      desired_rover_orientation_E.x() = rover_orientation_E.x();
      desired_rover_orientation_E.y() = rover_orientation_E.y();
      
      if (detected_direction==0){
        desired_rover_orientation_E.z() = rover_orientation_E.z() + M_PI / 2;
      }
      if (detected_direction==1){
        desired_rover_orientation_E.z() = rover_orientation_E.z() - M_PI / 2;
      }

      // pack yaw in Euler and convert back to q to send as desired quaternion.
      Quaterniond desired_rover_q = euler_to_quat(desired_rover_orientation_E);
      std::cout << "Current q" << rover_orientation_q << std::endl;
      std::cout << "Desired q" << desired_rover_q << std::endl;

      // Use pid to find ang_vel needed to go from current to desired_yaw
  }

  void send_cmd_vel(){
    // velocity_lin = 0.3f;

    track_mid_point_of_edges();
    geometry_msgs::msg::Twist velocity_cmd;
    geometry_msgs::msg::Pose pose_cmd;
    // pose_cmd.orientation.w = ;

    // pose_cmd.position.x


    velocity_cmd.linear.x = velocity_lin;
    velocity_cmd.angular.z = velocity_ang;
    // RCLCPP_INFO(this->get_logger(), "-------Fwd Speed : %.2f", velocity_lin);
    
    publisher_vel-> publish(velocity_cmd);
  }


  void img_pub_callback(cv::Mat image){

    if (image.channels() != 1) {
      RCLCPP_ERROR(this->get_logger(), "Input image is not grayscale. Ensure it has one channel.");
      return;
    }

    cv_bridge::CvImagePtr img_ptr;
    sensor_msgs::msg::Image::SharedPtr msg = cv_bridge::CvImage(std_msgs::msg::Header(), "mono8", image).toImageMsg();

    publisher_img -> publish(*msg);
  }

  void decision_process_callback(std_msgs::msg::Float32MultiArray msg){
    detected_direction = msg.data[0];
    confidence = msg.data[1];
    // if (confidence< 0.95){
    //   detected_direction = -1;
    // }
    std::cout << "direction ---" << detected_direction << "\n"<<
          "Confidence---"<< confidence<< std::endl;

  }

	void sub_odom_rover_calllback(const nav_msgs::msg::Odometry::SharedPtr msg_odom){
	// RCLCPP_WARN(this->get_logger(), "Odometry callback triggered");
		std::cout << "\nRECEIVED Rover Position DATA"   << std::endl;
    rover_current_ang_vel = msg_odom->twist.twist.angular.z;
    std::cout << "ang_vel:  --" << rover_current_ang_vel << std::endl;


		rover_pos.x() = msg_odom->pose.pose.position.x ;
		rover_pos.y() = msg_odom->pose.pose.position.y ;
		rover_pos.z() = msg_odom->pose.pose.position.z ;

		rover_orientation_q.w() = msg_odom->pose.pose.orientation.w;
		rover_orientation_q.x() = msg_odom->pose.pose.orientation.x;
		rover_orientation_q.y() = msg_odom->pose.pose.orientation.y;
		rover_orientation_q.z() = msg_odom->pose.pose.orientation.z;

		// std::cout << "\n rover_x, y , z ----" << rover_x <<", " << rover_y << ", " << rover_z << std::endl;
		// std::cout << "\n rover_yaw ---!!-" << rover_yaw <<" !! "<< std::endl;
	}
  // cv::VideoWriter video_writer_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_vel;
  rclcpp::TimerBase::SharedPtr timer_vel_pub;
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr vid_subscriber;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr publisher_img;
  rclcpp::Subscription<std_msgs::msg::Float32MultiArray>::SharedPtr subscriber_decision ;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr subscriber_odom_rover ;
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<AutonmousAINode>());
  rclcpp::shutdown();
  return 0;
}